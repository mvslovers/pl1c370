import csv, collections
R=list(csv.DictReader(open('scan.tsv'),delimiter='\t'))
T=lambda x:x=='True'
mk=lambda r: T(r['pstart']) or T(r['prolog']) or T(r['unrefd'])
for r in R:
    r['plsidr']= r['idr_tr'].startswith('PL')
    r['ns']=int(r['nstmtlines']); r['pr']=int(r['paired'])
    r['is_pls']= r['plsidr'] or (mk(r) and r['ns']>=5) or r['pr']>=5
ibm=[r for r in R if r['top']=='ibm']
print('ibm pls',sum(r['is_pls'] for r in ibm))
print('pls by paired only:')
for r in ibm:
    if r['is_pls'] and not r['plsidr'] and not mk(r): print('  ',r['path'],r['lines'],r['ns'],r['nstmts'],r['pr'],r['fmt'],r['regeq'])
print('marker excluded:')
for r in ibm:
    if mk(r) and not r['is_pls']: print('  ',r['path'],r['lines'],r['ns'],r['pr'])
print('not pls but paired 1-4 or ns>=50:')
for r in ibm:
    if not r['is_pls'] and (r['pr']>0 or (r['ns']>=50 and T(r['regeq']))): print('  ',r['path'],r['lines'],r['ns'],r['nstmts'],r['pr'],r['regeq'])
