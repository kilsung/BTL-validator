import xlrd
from .base import parse_sheet

# ODU_List 시트 서브타입별 기대값 컬럼 (0-based 인덱스)
# 엑셀 Row3(헤더): R=super4, S=super3, T=ghp_gen1, U=water4, V=super5, W=ghp_gen3,
#                  X=single, Y=multi, Z=multiv_s, AA=ghp_hybrid, AB=ghp_super,
#                  AC=water5, AD=multiv_i
_ODU_SUBTYPE_COLS = {
    17: 'super4',
    18: 'super3',
    19: 'ghp_gen1',
    20: 'water4',
    21: 'super5',
    22: 'ghp_gen3',
    23: 'single',
    24: 'multi',
    25: 'multiv_s',
    26: 'ghp_hybrid',
    27: 'ghp_super',
    28: 'water5',
    29: 'multiv_i',
}


def parse_odu_list(filepath: str) -> list:
    wb = xlrd.open_workbook(filepath)
    ws = wb.sheet_by_name('ODU_List')

    points = parse_sheet(wb, 'ODU_List')

    # pno → {subtype: expected_value} 매핑 빌드 (R~AD 컬럼)
    pno_subtype_map: dict = {}
    for r in range(4, ws.nrows):
        pno_raw = ws.cell_value(r, 1)
        if not pno_raw:
            continue
        try:
            pno = int(pno_raw)
        except (ValueError, TypeError):
            continue
        subtype_vals: dict = {}
        for col, subtype in _ODU_SUBTYPE_COLS.items():
            if col >= ws.ncols:
                continue
            v = ws.cell_value(r, col)
            if v == '' or v == '-':
                continue
            try:
                subtype_vals[subtype] = float(v)
            except (ValueError, TypeError):
                pass
        if subtype_vals:
            pno_subtype_map[pno] = subtype_vals

    # 각 포인트에 subtype_expected 주입
    for p in points:
        pno = p.get('point_no', 0)
        p['subtype_expected'] = pno_subtype_map.get(pno, {})

    return points
