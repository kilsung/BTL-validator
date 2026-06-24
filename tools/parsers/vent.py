import xlrd
from .base import parse_sheet

def parse_vent_list(filepath: str) -> list:
    """Vent_dxhrv_List: col5=vent_cni 우선, 없으면 col6=dxhrv_cni fallback
    vent_dxhrv_mode=True → F열(vent_cni)이 '-'인 행에 vent_skip=True 마킹
    """
    wb = xlrd.open_workbook(filepath)
    return parse_sheet(wb, 'Vent_dxhrv_List', cni_col=5, cni_fallback_col=6,
                       vent_dxhrv_mode=True)
