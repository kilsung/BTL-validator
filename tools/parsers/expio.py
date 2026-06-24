import xlrd
from .base import parse_sheet

def parse_expio_list(filepath: str) -> list:
    """EXPIO_List: AI/BI=read only, AV=단순 write 검증(min/max 없음), BV=write 검증"""
    wb = xlrd.open_workbook(filepath)
    return parse_sheet(wb, 'EXPIO_List', expio_mode=True)
