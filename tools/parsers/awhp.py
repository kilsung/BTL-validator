import xlrd
from .base import parse_sheet

def parse_awhp_list(filepath: str) -> list:
    wb = xlrd.open_workbook(filepath)
    return parse_sheet(wb, 'AWHP_List')
