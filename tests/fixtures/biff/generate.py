"""Independent BIFF8 fixture. Regenerate with Python xlwt 1.3.0; no app code."""
from pathlib import Path
import datetime
import xlwt
book = xlwt.Workbook(encoding='utf-8')
book.set_colour_RGB(0x21, 18, 52, 86)
sheet = book.add_sheet('Inputs 2026')
sheet.write(0, 0, 'Café 東京')
sheet.write(1, 0, 1234.5, xlwt.easyxf('font: name Arial, height 280, bold on, italic on, underline single, colour 33; pattern: pattern solid, fore_colour yellow; alignment: horiz right, vert top, wrap on; borders: bottom double, bottom_colour red;', num_format_str='#,##0.00'))
sheet.write(2, 0, True)
sheet.write(3, 0, '00123')
sheet.write(4, 0, '=literal')
sheet.write(5, 0, datetime.datetime(2026, 11, 14), xlwt.easyxf(num_format_str='yyyy-mm-dd'))
sheet.row(1).height = 600
sheet.row(1).height_mismatch = True
sheet.row(7).hidden = True
sheet.col(0).width = 18 * 256
sheet.col(2).hidden = True
sheet.write_merge(9, 10, 0, 2, 'Merged')
sheet.show_grid = False
other = book.add_sheet("Owner's data")
other.write(0, 0, -42)
other.write(0, 1, False)
book.save(str(Path(__file__).with_name('independent.xls')))
large = xlwt.Workbook()
sheet = large.add_sheet('Sparse distant corners')
sheet.write(0, 0, 'Start')
sheet.write(65535, 16, 'Distant corner')
large.save(str(Path(__file__).with_name('allocation-budget.xls')))
