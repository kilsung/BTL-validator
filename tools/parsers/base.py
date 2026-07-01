"""공통 엑셀 파싱 기반 함수
모든 장치별 파서가 공유하는 범용 시트 파싱 로직"""
import re

# xlrd는 btl_validator.py에서 이미 체크하므로, 여기서는 그냥 import
try:
    import xlrd
    XLRD_OK = True
except ImportError:
    xlrd = None
    XLRD_OK = False

VALID_TYPES = {'AI', 'AO', 'AV', 'BI', 'BO', 'BV', 'MI', 'MO'}


def _parse_device_filter(text: str) -> list:
    """I열(question) 텍스트에서 장치 필터 조건 파싱.

    지원 형식:
      장치설정:ahu-standard
      장치설정:ahu-nor,ahu-oah
      장치설정:if(facilitySubType==ahu-nor or facilitySubType==ahu-oah){ ... }

    ※ facilityName은 사용자 설정값이므로 필터 조건으로 사용하지 않는다.

    반환: 허용된 facilitySubType 리스트 (빈 리스트 = 필터 없음, 모든 장치 검증)
    """
    if not text:
        return []

    # facilitySubType==xxx 패턴만 파싱 (facilityName은 사용자 설정값이라 제외)
    if 'facilitySubType' in text:
        names = re.findall(r'facilitySubType\s*==\s*([\w\-]+)', text)
        return [n.strip() for n in names if n.strip()]

    # control_method 조건은 device_filter가 아닌 control_method_skip으로 처리
    if 'control_method' in text:
        return []

    # 단순 이름 목록: 장치설정:xxx,yyy (facilityName/SubType 없는 경우)
    # 단, if/else 키워드나 특수문자가 포함된 경우는 무시 (범위 안내 등 다른 조건)
    m = re.search(r'장치\s*설\s*정\s*:([^\n\r{(]+)', text)
    if m:
        raw = m.group(1).strip()
        names = [n.strip() for n in re.split(r'[,\s]+', raw)
                 if n.strip() and n.strip().lower() not in ('if', 'else', 'or', 'and')
                 and re.match(r'^[\w\-]+$', n.strip())]
        return names

    return []


def _parse_no_point_info(text: str) -> bool:
    """I열 텍스트에서 '관련 포인트 없음' 패턴을 감지한다.

    해당 포인트는 BACnet 오브젝트로 노출되지 않으므로 검증을 SKIP한다.
    '포인트에 관련 정보 없음', '관련 포인트 없음' 등의 변형을 지원한다.
    """
    if not text:
        return False
    return bool(re.search(r'관련\s*포인트\s*없음|포인트에\s*관련\s*정보\s*없음', text))


def _parse_pair_state_map(text: str) -> dict:
    """H열(direction) 텍스트에서 MO command → 기대 MI state 매핑을 파싱한다.

    지원 형식 (H열 실제 포맷):
      if(command==1) set 2(low)          ← Aircare FanSpeed: set X = 1-based BACnet state
      if(command==1) set 0(cool)         ← Chiller: set X = 0-based raw index
      if(command==2 or command==3) not use
      if(command==5) set 4(heat)

    'set X' 값 해석 규칙:
      - set 값 중 0이 포함되면 → 0-based raw index → BACnet 1-based state (+1 보정)
        예) Chiller: set 0(cool)→state1, set 4(heat)→state5
      - set 값이 모두 ≥1이면 → 이미 1-based BACnet state (보정 없음)
        예) Aircare FanSpeed: set 2(low)→state2, set 3(mid)→state3

    반환: {cmd_state(int): expected_mi_state(int), ...}
    예) Aircare FanSpeed: {1:2, 2:3, 3:4, 4:5}
        Chiller Mode:     {} (identity: cmd1→state1, cmd5→state5 → 빈 dict)
    매핑이 없거나 모든 cmd==expected이면 빈 dict 반환.
    """
    if not text or 'command' not in text:
        return {}
    raw_pairs = []
    for m in re.finditer(
        r'if\s*\(\s*command\s*==\s*(\d+)\s*\)\s*set\s+(\d+)', text, re.IGNORECASE
    ):
        raw_pairs.append((int(m.group(1)), int(m.group(2))))
    if not raw_pairs:
        return {}

    # set X에 0이 포함되면 0-based → BACnet 1-based (+1 보정)
    needs_offset = any(x == 0 for _, x in raw_pairs)
    result = {}
    for cmd, raw_val in raw_pairs:
        mi_st = raw_val + 1 if needs_offset else raw_val
        result[cmd] = mi_st

    # 모든 매핑이 identity(cmd==expected)이면 offset 없음 → 빈 dict 반환
    if all(k == v for k, v in result.items()):
        return {}
    return result


def _parse_control_method_skip(text: str) -> str:
    """I열 텍스트에서 control_method==XXX 이면 SKIP 조건을 파싱한다.

    지원 형식:
      장치설정:if("extendedInfo": {"control_method": "awhpWtCtrl"}){ skip} else {execution}
      장치설정:if("extendedInfo": "control_method"== "awhpWtCtrl" ){ skip} else ...
    반환: SKIP 해야 할 control_method 값 문자열 (없으면 빈 문자열)
    """
    if not text or 'control_method' not in text:
        return ''
    # ':'(JSON 형식) 또는 '=='(비교 연산자 형식) 모두 지원
    # 닫는 따옴표는 선택적(["\']?) — 엑셀에서 생략된 경우도 지원
    m = re.search(r'["\']?control_method["\']?\s*(?:[:\"]+|==)\s*["\']([\w]+)["\']?', text)
    if not m:
        return ''
    method = m.group(1).strip()
    # 메소드 매치 이후 첫 번째 {...} 블록 확인
    rest = text[m.end():]
    m_block = re.search(r'\{([^}]*)\}', rest)
    if m_block and re.search(r'skip', m_block.group(1), re.IGNORECASE):
        return method
    return ''


def _parse_ctrl_ranges(text: str) -> dict:
    """I열 텍스트에서 control_method별 AV 범위 조건을 파싱한다.

    지원 형식 (I열 실제 포맷):
      장치설정:if("extendedInfo": "control_method"== "awhpWtCtrl" ){ skip}
             else if("extendedInfo": "control_method"== "awhpWioCtrl"){5℃ ~ 65℃}
             else{16℃ ~ 30℃}

    반환: {ctrl_method: (min, max), '__default__': (min, max)}
      - skip 분기는 포함하지 않음 (control_method_skip으로 별도 처리)
      - Flask jsonify sort_keys=True 호환을 위해 else 키는 None 대신 '__default__' 사용
    """
    if not text or 'control_method' not in text:
        return {}

    result = {}

    # ':' (JSON 형식) 또는 '==' (비교 형식) 모두 지원— 매칭 후 첫 번째 {...} 블록 = 해당 범위
    # 닫는 따옴표는 선택적(["\']?) — 엑셀에서 생략된 경우도 지원
    for m in re.finditer(
        r'["\']?control_method["\']?\s*(?:[:\"]+|==)\s*["\']([\w]+)["\']?',
        text
    ):
        method = m.group(1).strip()
        rest = text[m.end():]
        m_block = re.search(r'\{([^}]*)\}', rest)
        if not m_block:
            continue
        block_content = m_block.group(1).strip()
        if re.search(r'skip', block_content, re.IGNORECASE):
            continue  # skip 분기는 control_method_skip으로 처리
        lo, hi = _parse_av_range(block_content)
        if lo is not None and hi is not None:
            result[method] = (lo, hi)

    # else{...} 기본 범위 추출 (else if 없는 단독 else)
    m_else = re.search(
        r'(?<!if)\belse\b\s*\{([^}]*)\}', text, re.IGNORECASE
    )
    if m_else:
        lo, hi = _parse_av_range(m_else.group(1).strip())
        if lo is not None and hi is not None:
            result['__default__'] = (lo, hi)  # Flask jsonify sort_keys=True 호환을 위해 None 대신 문자열 사용

    return result


def _parse_av_range(text: str):
    """unit_info / question 텍스트에서 AV min/max 파싱"""
    if not text or text in ('-', ''):
        return None, None
    m_min = re.search(r'"minValue"\s*:\s*([\d.]+)', text)
    m_max = re.search(r'"maxValue"\s*:\s*([\d.]+)', text)
    if m_min and m_max:
        return float(m_min.group(1)), float(m_max.group(1))
    m = re.search(r'([\d.]+)[^\d~～]*[~～][^\d~～]*([\d.]+)', text)
    if m:
        return float(m.group(1)), float(m.group(2))
    return None, None


def _parse_bv_active(text: str):
    """BV Active 텍스트에서 active value 파싱 (예: 'Reset(1)' → 1)"""
    if not text or text in ('-', ''):
        return 1
    m = re.search(r'\((\d+)\)', text)
    return int(m.group(1)) if m else 1


def _parse_judge_condition(text: str) -> dict:
    """Question 텍스트에서 판정설정 조건 파싱.

    지원 문구 형태 (공백/개행 무시):
      판정설정:if (command==1) {senseBodyPowsav=0 & senseBodyEcooper=0}
               if(command==2) : senseBodyPowsav=1
               else if(command==3) : senseBodyEcooper

    반환: {state_int: {cni_name: expected_int, ...}, ...}
    값이 명시되지 않으면 1로 간주.
    판정설정 문구가 없으면 빈 dict 반환.

    특수 키 '__post_write_state':
      'if(Both==0) : Label(N)' 패턴이 있으면 검증 후 state N으로 복귀.
      예: 판정설정:if(Both==0):None(3) → {'__post_write_state': 3, ...}
    """
    if not text or '판정' not in text:
        return {}

    # '판정설정:' 이후 텍스트만 추출
    m_start = re.search(r'판정\s*설\s*정\s*:', text)
    if not m_start:
        return {}
    cond_text = text[m_start.end():]

    # 공백 정규화 (개행 → 공백)
    cond_text = cond_text.replace('\n', ' ').replace('\r', ' ')

    result = {}

    # 특수 패턴: if(Both==0) : Label(N) → 검증 후 복귀 state 추출
    m_both = re.search(r'if\s*\(\s*Both\s*==\s*0\s*\)\s*:?\s*[^(]*\((\d+)\)', cond_text, re.IGNORECASE)
    if m_both:
        result['__post_write_state'] = int(m_both.group(1))

    # 패턴: (else if | if) (command == X) { ... } 또는 : cni=val
    # 각 if/else if 블록을 순서대로 추출
    blocks = re.split(r'(?:else\s+)?if\s*\(?\s*command\s*==\s*', cond_text)[1:]
    for block in blocks:
        # 첫 숫자가 state 번호
        m_num = re.match(r'(\d+)', block)
        if not m_num:
            continue
        state_num = int(m_num.group(1))
        rest = block[m_num.end():]

        # `{...}` 또는 `)? : ...` 형태에서 조건 텍스트 추출
        m_brace = re.search(r'\{([^}]*)\}', rest)
        m_colon = re.search(r'\)\s*:\s*(.+?)(?=(?:else\s*)?if|$)', rest)

        cond_str = ''
        if m_brace:
            cond_str = m_brace.group(1)
        elif m_colon:
            cond_str = m_colon.group(1)

        # cni=val 쌍 파싱, &로 구분
        conds = {}
        for part in re.split(r'[&,]', cond_str):
            part = part.strip()
            m_eq = re.match(r'(\w+)\s*=\s*(\d+)', part)
            if m_eq:
                conds[m_eq.group(1)] = int(m_eq.group(2))
            elif re.match(r'\w+', part):
                # 값 없는 cni → 1 로 간주
                cni_name = part.strip()
                if cni_name:
                    conds[cni_name] = 1
        if conds:
            result[state_num] = conds

    return result


def _parse_pre_condition(text: str):
    """Question 텍스트에서 사전설정 조건 파싱.

    지원 형식 (복수 구분: '/' 또는 개행 또는 쉼표):
      '사전설정:mode=>cool'                          → cni_writes 1개
      '사전설정:mode=>cool/사전설정:fan=>suphigh'    → cni_writes 2개 (슬래시)
      '사전설정:occupied=>on,heatUpperLim=>32.0'    → occupied + cni_writes 1개 (쉼표 연속)
      '사전설정:mode=>cool,fan=>suphigh'             → cni_writes 2개 (쉼표 연속)
      '사전설정:occupied=>on'                        → {'occupied': True}
      '사전설정:if(acMode && acOper is exist){acMode=>heat,acOper=>On}'
                                                     → {'condition': {'exists': ['acMode','acOper']},
                                                         'cni_writes': [...]}

    반환: dict 또는 None
    """
    if not text or '사전' not in text:
        return None

    result = {}

    # ── 조건부 사전설정: if(cni1 && cni2 is exist){cni1=>val1,cni2=>val2} ──────
    # 조건 충족 시 cni_writes 적용, 불충족 시 사전설정 스킵하고 검증 진행
    m_if = re.search(r'사전설정:\s*if\s*\(([^)]+)\)\s*\{([^}]+)\}', text, re.IGNORECASE)
    if m_if:
        cond_part = m_if.group(1)  # 'acMode && acOper is exist'
        body_part = m_if.group(2)  # 'acMode=>heat,acOper=>On'
        # &&로 분리 후 'is exist' 제거하여 CNI명만 추출
        exists_cnis = []
        for token in re.split(r'&&', cond_part):
            cni = re.sub(r'\s+is\s+exist\b.*', '', token.strip(), flags=re.IGNORECASE).strip()
            if cni:
                exists_cnis.append(cni)
        if exists_cnis:
            result['condition'] = {'exists': exists_cnis}
        # body 내 cni=>val 쌍 파싱
        body_writes = []
        for pair_m in re.finditer(r'([\w]+)\s*=+>?\s*([^\s,}]+)', body_part):
            body_writes.append({'cni': pair_m.group(1), 'value': pair_m.group(2)})
        if body_writes:
            result['cni_writes'] = body_writes
        return result if result else None

    # occupied: BO/BV active 여부 (특수 bool 처리)
    if re.search(r'occupied\s*=+>?\s*on', text, re.IGNORECASE):
        result['occupied'] = True

    # ── 복수 사전설정 파싱 전략 ───────────────────────────────────────────
    # 형식 A: '사전설정:cni1=>val1/사전설정:cni2=>val2' → 슬래시/개행 구분
    # 형식 B: '사전설정:cni1=>val1,cni2=>val2'          → 첫 항목 뒤 쉼표 연속
    #
    # 1단계: '사전설정:' 접두어가 있는 블록을 슬래시/개행으로 분리해 처리
    # 2단계: 각 블록 내부에서 쉼표로 이어진 추가 cni=>값 쌍도 파싱
    cni_writes = []

    # '사전설정:...' 블록 추출 (슬래시·개행까지, 쉼표 포함)
    for block_m in re.finditer(r'사전설정:(.*?)(?=사전설정:|$)', text, re.DOTALL):
        block = block_m.group(1).strip()
        # 블록 내 '/' 또는 개행은 종료로 처리
        block = re.split(r'[/\n]', block)[0].strip()

        # 블록 내 'cni=>val' 쌍을 쉼표 구분으로 모두 파싱
        # 첫 번째: 'cni=>val' 형식
        # 이후: ',cni=>val' 형식 (사전설정: 접두어 없는 연속 항목)
        for pair_m in re.finditer(r'([\w]+)\s*=+>?\s*([^\s,/\n]+)', block):
            cni_name = pair_m.group(1).strip()
            val_str  = pair_m.group(2).strip()
            if cni_name.lower() == 'occupied':
                continue  # occupied는 위에서 별도 처리
            cni_writes.append({'cni': cni_name, 'value': val_str})

    if cni_writes:
        result['cni_writes'] = cni_writes

    return result if result else None


def parse_sheet(wb, sheet_name: str,
                cni_col: int = 5,
                cni_fallback_col: int = 6,
                expio_mode: bool = False,
                vent_dxhrv_mode: bool = False) -> list:
    """범용 시트 파서

    Parameters
    ----------
    wb                : xlrd Workbook 객체
    sheet_name        : 시트 이름
    cni_col           : primary CNI 컬럼 인덱스 (기본=5)
    cni_fallback_col  : fallback CNI 컬럼 인덱스 (기본=6)
    vent_dxhrv_mode   : True → F열(vent_cni)이 '-'인 행에 vent_skip=True 마킹
    expio_mode        : True → AI/BI read only, AV=단순 write 검증
    """
    if not XLRD_OK:
        raise RuntimeError("xlrd 라이브러리 필요: pip install 'xlrd==1.2.0'")

    try:
        ws = wb.sheet_by_name(sheet_name)
    except Exception:
        raise RuntimeError(f"시트를 찾을 수 없습니다: {sheet_name}")

    points = []

    for r in range(4, ws.nrows):
        # 최대 20컬럼 읽기 (ncols 초과는 빈 문자열로 처리)
        raw = [ws.cell_value(r, c) if c < ws.ncols else '' for c in range(20)]

        obj_type = str(raw[4]).strip()
        if obj_type not in VALID_TYPES:
            continue

        instance = raw[0]
        if not instance:
            continue

        # ── State text 목록 (Multi-state 용, col 10~16) ──────────────────
        state_texts = []
        for c in range(10, 17):
            t = str(raw[c]).strip()
            if t and t not in ('-', ''):
                state_texts.append(t)

        # ── CNI 파싱 ─────────────────────────────────────────────────────
        f_cni = str(raw[cni_col]).strip()          # F열: vent_cni
        g_cni = str(raw[cni_fallback_col]).strip() # G열: dxhrv_cni

        # Vent/DXHRV 공통 시트: F열(vent_cni)이 '-'이면 vent 장치에서 skip
        # vent_dxhrv_mode=True일 때만 마킹 (다른 파서에 오염 방지)
        vent_cni_is_dash = vent_dxhrv_mode and (f_cni in ('-', ''))

        # 파싱 시점의 cni: pair_instance 매핑을 위한 대표값 (F 우선, 없으면 G)
        cni_raw = f_cni if f_cni not in ('-', '') else g_cni

        cni_list = [c.strip() for c in cni_raw.replace('\r', '').split('\n')
                    if c.strip() and c.strip() != '-']
        if not cni_list:
            cni_list = ['']  # SKIP 처리용 빈 값 유지

        # ── AV/AO min/max 파싱 ───────────────────────────────────────────
        av_min, av_max = None, None
        if obj_type in {'AV', 'AO'} and not expio_mode:
            av_min, av_max = _parse_av_range(str(raw[10]).strip())
            if av_min is None:
                av_min, av_max = _parse_av_range(str(raw[8]).strip())

        # ── BV active value 파싱 ─────────────────────────────────────────
        bv_active = None
        if obj_type == 'BV':
            bv_active = _parse_bv_active(str(raw[11]).strip() if raw[11] else '')

        # ── commandable / write_mode ─────────────────────────────────────
        if expio_mode:
            # EXPIO: AI/BI=read only, AV/BV=write
            commandable = obj_type in {'AV', 'BV'}
            # AV는 min/max 없으므로 단순 write 검증으로 전환
            write_mode = 'simple' if obj_type == 'AV' else 'standard'
        else:
            commandable = obj_type in {'AO', 'BO', 'MO', 'AV', 'BV'}
            write_mode = 'standard'

        # ── 사전설정 조건 파싱 ────────────────────────────────────────────
        pre_condition = None
        if commandable and not expio_mode:
            pre_condition = _parse_pre_condition(str(raw[8]).strip())

        # ── 판정설정 조건 파싱 ────────────────────────────────────────────
        judge_condition = {}
        if commandable and not expio_mode:
            judge_condition = _parse_judge_condition(str(raw[8]).strip())

        # ── 장치 필터 파싱 (장치설정: / facilityName== / facilitySubType==) ──
        device_filter = _parse_device_filter(str(raw[8]).strip())

        # ── 포인트 없음 / control_method SKIP / ctrl_ranges 파싱 ───────────
        q_text = str(raw[8]).strip()
        no_point_info       = _parse_no_point_info(q_text)
        control_method_skip = _parse_control_method_skip(q_text)
        # AV: control_method별 범위가 다른 경우 (예: AWHP settemp)
        ctrl_ranges = _parse_ctrl_ranges(q_text) if obj_type == 'AV' else {}

        # ── pair_state_map: H열(direction, col7) MO command→MI state 오프셋 ──
        # 형식: if(command==1) set 2(low) → {1: 2, 2: 3, ...}
        # MO write 시 폴링 기대값을 pair MI state로 변환하는 데 사용
        h_text = str(raw[7]).strip() if len(raw) > 7 else ''
        pair_state_map = _parse_pair_state_map(h_text) if obj_type == 'MO' else {}

        base = {
            'instance':          int(instance),
            'point_no':          int(raw[1]) if raw[1] else 0,
            'name':              str(raw[2]).strip(),
            'obj_name_template': str(raw[3]).strip(),
            'object_type':       obj_type,
            'question':          str(raw[8]).strip(),
            'unit_info':         str(raw[10]).strip(),
            'state_texts':       state_texts,
            'number_of_states':  len(state_texts),
            'av_min':            av_min,
            'av_max':            av_max,
            'bv_active':         bv_active,
            'pair_instance':     None,
            'commandable':       commandable,
            'write_mode':        write_mode,
            'pre_condition':     pre_condition,
            'judge_condition':   judge_condition,
            'device_filter':     device_filter,  # 허용 facilityName/SubType 리스트 (빈 리스트=전체)
            'device_unit_name':  '',             # btl_validator.py에서 b2→unit_name 매핑 후 채움
            'no_point_info':     no_point_info,      # True → 포인트 없음, 검증 SKIP
            'control_method_skip': control_method_skip, # 이 control_method이면 SKIP ('awhpWtCtrl' 등)
            'ctrl_ranges':       ctrl_ranges,          # {ctrl_method: (min,max), None: (min,max)} 장치별 범위
            'vent_skip':         vent_cni_is_dash,   # Vent 모드에서 skip할 관제점 (F열='-')
            'vent_cni':          f_cni if not vent_cni_is_dash else '',  # F열 CNI (Vent 검증용)
            'dxhrv_cni':         g_cni,                                   # G열 CNI (DXHRV 검증용)
            'point_name':        g_cni,                                   # G열 point_name (ACPI_BY_NAME, 스킵 조건용)
            'pair_state_map':    pair_state_map,  # H열: {cmd_state: expected_mi_state} (FanSpeed 등 오프셋 보정)
            'llim_instance':     None,  # settempLlim 등 하한 AI/AV 인스턴스 (동적 av_min용)
            'llim_type':         None,  # 'AI' 또는 'AV'
            'ulim_instance':     None,  # settempUlim 등 상한 AI/AV 인스턴스 (동적 av_max용)
            'ulim_type':         None,  # 'AI' 또는 'AV'
        }

        # 복수 CNI = compound 관제점 → 인스턴스당 1개 엔트리로 통합
        # (2개 CNI를 펌웨어가 조합해 state를 결정하므로 검증도 1회만 수행)
        entry = dict(base)
        entry['cni'] = cni_list[0] if cni_list else ''
        entry['idx'] = len(points) + 1
        points.append(entry)

    # ── Pair 매핑 후처리 ──────────────────────────────────────────────────
    SETTING_TO_STATUS = {'BO': 'BI', 'MO': 'MI', 'AO': 'AI', 'AV': 'AI'}
    cni_index: dict = {}
    for p in points:
        c = p.get('cni', '')
        if c and c not in ('-', ''):
            cni_index.setdefault(c, []).append((p['instance'], p['object_type']))

    for p in points:
        otype = p['object_type']
        expected_status = SETTING_TO_STATUS.get(otype)
        if not expected_status:
            continue
        cni = p.get('cni', '')
        if not cni or cni in ('-', ''):
            continue
        for inst, itype in cni_index.get(cni, []):
            if itype == expected_status and inst != p['instance']:
                p['pair_instance'] = inst
                break

        # AV: cni+'Llim' / cni+'Ulim' CNI의 AI/AV 인스턴스를 동적 write 범위용으로 연결
        if otype == 'AV':
            for inst, itype in cni_index.get(cni + 'Llim', []):
                if itype in ('AI', 'AV'):
                    p['llim_instance'] = inst
                    p['llim_type'] = itype
                    break
            for inst, itype in cni_index.get(cni + 'Ulim', []):
                if itype in ('AI', 'AV'):
                    p['ulim_instance'] = inst
                    p['ulim_type'] = itype
                    break

            # settempLlim AV: 상한 제약 = settempUlim AI (Llim < Ulim 조건 보장)
            # 동적 av_max = settempUlim_AI - 1.0 로 계산하여 30=30 제약 위반 방지
            if cni.lower() == 'settempllim' and p.get('ulim_instance') is None:
                for u_inst, u_type in cni_index.get('settempUlim', []):
                    if u_type == 'AI':
                        p['ulim_instance'] = u_inst
                        p['ulim_type'] = 'AI'
                        break

    return points
