import xlrd
from .base import parse_sheet


def parse_cdu_list(filepath: str) -> list:
    """CDU 관제점 시트 파싱.

    우선순위:
      1) CDU_List
      2) Aircon_List (하위 호환)
    """
    wb = xlrd.open_workbook(filepath)

    for sheet_name in ('CDU_List', 'Aircon_List'):
        if sheet_name in wb.sheet_names():
            return parse_sheet(wb, sheet_name)

    raise ValueError('CDU_List 또는 Aircon_List 시트를 찾을 수 없습니다')
