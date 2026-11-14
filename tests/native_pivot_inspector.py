"""Independent ZIP/XML and openpyxl inspection for retained native-pivot fixtures."""
import argparse
import json
import math
import datetime
import posixpath
import zipfile
from pathlib import Path
from lxml import etree
import openpyxl
from openpyxl.utils.cell import range_boundaries
from openpyxl.utils.datetime import to_excel

S = {'s': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main',
     'r': 'http://schemas.openxmlformats.org/package/2006/relationships',
     'c': 'http://schemas.openxmlformats.org/drawingml/2006/chart'}

def inspect(path):
    with zipfile.ZipFile(path) as package:
        names = set(package.namelist())
        assert len(names) == len(package.namelist()), 'duplicate ZIP member'
        trees = {name: etree.fromstring(package.read(name)) for name in names
                 if name.endswith('.xml') or name.endswith('.rels')}
        for name, tree in trees.items():
            if not name.endswith('.rels'):
                continue
            folder, filename = posixpath.split(name)
            base = posixpath.join(posixpath.dirname(folder), filename[:-5])
            for rel in tree:
                if rel.get('TargetMode') == 'External':
                    continue
                target = rel.get('Target', '')
                target = target[1:] if target.startswith('/') else posixpath.normpath(posixpath.join(posixpath.dirname(base), target))
                assert target in names, (name, 'dangling relationship', target)
        tables = []
        for name, tree in trees.items():
            if tree.tag != '{%s}pivotTableDefinition' % S['s']:
                continue
            fields = tree.find('s:pivotFields', S)
            assert int(fields.get('count')) == len(fields), (name, 'pivot field count')
            assert tree.find('s:dataFields', S) is not None, (name, 'native measure fields')
            tables.append(tree.get('name'))
        for name, tree in trees.items():
            if tree.tag == '{%s}pivotCacheDefinition' % S['s']:
                fields = tree.find('s:cacheFields', S)
                assert int(fields.get('count')) == len(fields), (name, 'cache field count')
                for field in fields:
                    shared = field.find('s:sharedItems', S)
                    if shared is not None:
                        assert int(shared.get('count', len(shared))) == len(shared), (name, 'shared item count')
        wb = openpyxl.load_workbook(path, data_only=False)
        pivots = [p for ws in wb for p in ws._pivots]
        assert len(pivots) == len(tables), 'independent reader recognizes every native pivot'
        for pivot in pivots:
            cache = pivot.cache
            assert cache.records is not None, 'saved native cache records exist'
            fields = [f for f in cache.cacheFields if f.databaseField]
            assert cache.recordCount == len(cache.records.r), 'native record count'
            for record in cache.records.r:
                assert len(record._fields) == len(fields), 'record/source field widths agree'
                for field, value in zip(fields, record._fields):
                    if value.tagname == 'x':
                        assert field.sharedItems is not None and value.v < len(field.sharedItems._fields), 'shared item index'
        qualified_pivots = {p.name: ws.title for ws in wb for p in ws._pivots}
        validated_chart_points = 0
        for name, tree in trees.items():
            source = tree.find('c:pivotSource', S)
            if source is None:
                continue
            native_name = source.findtext('c:name', namespaces=S)
            matched = [pivot for pivot in qualified_pivots if native_name.endswith('!' + pivot)]
            assert len(matched) == 1, (name, 'native PivotChart links to exactly one local table')
            pivot_name = matched[0]
            assert native_name == f'[{Path(path).name}]{qualified_pivots[pivot_name]}!{pivot_name}', (name, 'PivotChart source qualifier must follow Save As basename')
            for reference in tree.findall('.//c:ser/c:val/c:numRef', S):
                formula = reference.findtext('c:f', namespaces=S)
                sheet_name, address = formula.rsplit('!', 1)
                if sheet_name.startswith("'") and sheet_name.endswith("'"):
                    sheet_name = sheet_name[1:-1].replace("''", "'")
                c1, r1, c2, r2 = range_boundaries(address)
                assert c1 == c2, (name, 'one numeric source column per series')
                cache = reference.find('c:numCache', S)
                assert int(cache.find('c:ptCount', S).get('val')) == r2-r1+1, (name, 'numeric cache point count matches source')
                cached = {int(point.get('idx')): float(point.findtext('c:v', namespaces=S))
                          for point in cache.findall('c:pt', S)}
                for offset, row in enumerate(range(r1, r2+1)):
                    expected = wb[sheet_name].cell(row, c1).value
                    if isinstance(expected, (datetime.datetime, datetime.date, datetime.time)):
                        expected = to_excel(expected, wb.epoch)
                    if expected is None or expected == '':
                        assert offset not in cached, (name, 'blank series values remain absent')
                    else:
                        assert offset in cached and math.isclose(cached[offset], float(expected), rel_tol=1e-9, abs_tol=1e-9), (name, 'native cached chart value matches rendered report cell', row, c1)
                        validated_chart_points += 1
        pivot_charts = [name for name, tree in trees.items()
                        if tree.tag == '{%s}chartSpace' % S['c'] and tree.find('c:pivotSource', S) is not None]
        return {'path': str(Path(path).resolve()), 'openpyxl': openpyxl.__version__,
                'pivots': tables, 'records': [len(p.cache.records.r) for p in pivots],
                'pivot_charts': pivot_charts, 'validated_chart_points': validated_chart_points}

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('files', nargs='+')
    args = parser.parse_args()
    for filename in args.files:
        print(json.dumps(inspect(filename), ensure_ascii=False))
