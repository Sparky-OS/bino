#!/usr/bin/env python3
"""Check application mapping and independent FFmpeg hardware-transfer evidence."""
import argparse,csv,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);p.add_argument('--path',choices=('import','map','software'),required=True);a=p.parse_args()
rows=list(csv.DictReader((a.directory/'trace.csv').open()))
count=lambda event: sum(r['event']==event and int(r['pts_us'])>=0 for r in rows)
transfers=a.directory/'transfers.csv'
transfers=len(transfers.read_text().splitlines()) if transfers.exists() else 0
result={'path':a.path,'imports':count('texture_import'),'maps':count('map'),'cpu_uploads':count('cpu_upload'),'hardware_transfers':transfers}
print(json.dumps(result,indent=2),flush=True)
assert (a.directory/'app-exit.txt').read_text().strip()=='0'
if a.path=='import':
 assert result['imports']>=20 and result['maps']==result['cpu_uploads']==transfers==0,result
else:
 assert result['imports']==0 and result['maps']>=20,result
 assert (transfers>=20) if a.path=='map' else (transfers==0),result
