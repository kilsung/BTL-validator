import xlrd
from .base import parse_sheet

def parse_aircon_list(filepath: str) -> list:
    wb = xlrd.open_workbook(filepath)
    return parse_sheet(wb, 'Aircon_List')
