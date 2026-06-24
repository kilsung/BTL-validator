"""장치별 파서 레지스트리
장치 타입 키 → {label, parse 함수} 매핑"""
from .aircon  import parse_aircon_list
from .cdu     import parse_cdu_list
from .vent    import parse_vent_list
from .ahu     import parse_ahu_list
from .odu     import parse_odu_list
from .awhp    import parse_awhp_list
from .general import parse_general_list
from .expio   import parse_expio_list
from .chiller import parse_chiller_list
from .aircare import parse_aircare_list

# 등록 순서 = UI 드롭다운 표시 순서
DEVICE_REGISTRY: dict = {
 #   'Aircon':  {'label': '에어컨 (Aircon)',       'parse': parse_aircon_list},
    'CDU':     {'label': 'CDU',                  'parse': parse_cdu_list},
 #   'Vent':    {'label': '환기/DXHRV (Vent)',     'parse': parse_vent_list},
 #   'AHU':     {'label': 'AHU',                   'parse': parse_ahu_list},
 #   'ODU':     {'label': '실외기 (ODU)',           'parse': parse_odu_list},
 #   'AWHP':    {'label': 'AWHP',                  'parse': parse_awhp_list},
 #   'General': {'label': '일반 (General)',         'parse': parse_general_list},
 #   'EXPIO':   {'label': 'EXPIO',                 'parse': parse_expio_list},
 #   'Chiller': {'label': '칠러 (Chiller/ISC2)',   'parse': parse_chiller_list},
 #   'Aircare': {'label': '공기청정 (Aircare)',     'parse': parse_aircare_list},
}

__all__ = ['DEVICE_REGISTRY']
