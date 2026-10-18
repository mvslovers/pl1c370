import csv, collections, statistics, re
R=list(csv.DictReader(open('scan.tsv'),delimiter='\t'))
T=lambda x:x=='True'
mk=lambda r: T(r['pstart']) or T(r['prolog']) or T(r['unrefd'])
for r in R:
    r['plsidr']= r['idr_tr'].startswith('PL')
    r['ns']=int(r['nstmtlines']); r['pr']=int(r['paired'])
    r['is_pls']= r['plsidr'] or (mk(r) and r['ns']>=5) or r['pr']>=5 or T(r['oldsig'])
    r['L']=int(r['lines']); r['nc']=int(r['ncstmts']); r['mx']=int(r['maxstmt'])
ibm=[r for r in R if r['top']=='ibm' and r['is_pls']]
var=[r for r in R if r['top']=='variants' and r['is_pls']]
print('PLS ibm',len(ibm),'variants',len(var))
# rule contributions
allibm=[r for r in R if r['top']=='ibm']
print(' via idr',sum(r['plsidr'] for r in allibm),' via markers+ns',sum(mk(r) and r['ns']>=5 for r in allibm),
      ' via paired',sum(r['pr']>=5 for r in allibm),' via oldsig',sum(T(r['oldsig']) for r in allibm))
print(' oldsig-only adds', sum(T(r['oldsig']) and not (r['plsidr'] or (mk(r) and r['ns']>=5) or r['pr']>=5) for r in allibm))
print(' paired-only adds', [r['path'] for r in allibm if r['pr']>=5 and not (r['plsidr'] or (mk(r) and r['ns']>=5) or T(r['oldsig']))])
# kinds per manifest? per prefix
pc=collections.Counter(r['path'].split('/')[1] for r in ibm)
tot=collections.Counter(r['path'].split('/')[1] for r in allibm)
print('per prefix (pls/all):', ', '.join(f'{k} {v}/{tot[k]}' for k,v in pc.most_common()))
print('nprefixes',len(pc))
# variants dedupe
ibm_sha={r['sha'] for r in allibm}; ibm_mem={r['member'] for r in allibm}
pls_mem={r['member'] for r in ibm}
c=collections.Counter()
for r in var:
    if r['sha'] in ibm_sha: c['identical to ibm']+=1
    elif r['member'] in ibm_mem: c['other version of ibm member']+=1
    else: c['name not in ibm']+=1
print('variants:',c, 'distinct sha', len({r['sha'] for r in var}), 'distinct members', len({r['member'] for r in var}))
print('variant dirs', collections.Counter('/'.join(r['path'].split('/')[:3]) for r in var).most_common())
print('variant member not pls in ibm', [r['path'] for r in var if r['member'] in ibm_mem and r['member'] not in pls_mem])
# sums
def summ(S,name):
    L=[r['L'] for r in S]; nc=[r['nc'] for r in S]; mx=[r['mx'] for r in S]
    q=lambda a:[sorted(a)[int(len(a)*p)] for p in (0.1,0.25,0.5,0.75,0.9)]+[max(a)]
    print(name,'n',len(S),'lines',sum(L),'sum ncstmts',sum(nc),'sum maxstmt',sum(mx),'sum nstmts',sum(int(r['nstmts']) for r in S))
    print('  lines q10,25,50,75,90,max',q(L)); print('  ncstmts q',q(nc)); print('  maxstmt q',q(mx))
    bins=[0,100,250,500,1000,2000,4000,8000,16000,10**9]
    h=collections.Counter(next(f'{bins[i]}-{bins[i+1]}' for i in range(len(bins)-1) if bins[i]<=x<bins[i+1]) for x in L)
    print('  lines hist',[ (f'{bins[i]}-{bins[i+1]}',h[f'{bins[i]}-{bins[i+1]}']) for i in range(len(bins)-1)])
    bins2=[0,1,25,50,100,200,400,800,1600,10**9]
    h=collections.Counter(next(f'{bins2[i]}-{bins2[i+1]}' for i in range(len(bins2)-1) if bins2[i]<=x<bins2[i+1]) for x in nc)
    print('  ncstmts hist',[ (f'{bins2[i]}-{bins2[i+1]}',h[f'{bins2[i]}-{bins2[i+1]}']) for i in range(len(bins2)-1)])
    big=sorted(S,key=lambda r:-r['nc'])[:8]; print('  largest by ncstmts',[(r['member'],r['L'],r['nc']) for r in big])
summ(ibm,'IBM')
uv=[r for r in var if r['sha'] not in ibm_sha]
summ(uv,'VARIANTS non-identical')
# IDR
print('IDR translator class:',collections.Counter(('PLSnnnn' if re.match(r'PLS\d{4}$',r['idr_tr']) else r['idr_tr'] or '(none)') for r in ibm))
hh=[r['idr_tr'][3:] for r in ibm if re.match(r'PLS\d{4}$',r['idr_tr'])]
print(' PLSnnnn: max HH',max(int(x[:2]) for x in hh),' max MM',max(int(x[2:]) for x in hh), 'n',len(hh))
print(' version x translator',collections.Counter((('PLSnnnn' if re.match(r'PLS\d{4}$',r['idr_tr']) else r['idr_tr']),r['idr_ver']) for r in ibm if r['idr_tr']).most_common())
print(' IDR year by version',sorted(collections.Counter((r['idr_ver'],r['idr_date'][:2]) for r in ibm if r['idr_tr']).items()))
print(' IDR year',sorted(collections.Counter(r['idr_date'][:2] for r in ibm if r['idr_tr']).items()))
# eyecatchers
print('eye yy',sorted(collections.Counter(r['eye_yy'] for r in ibm if r['eye_yy']).items()))
print('eye2 yy',sorted(collections.Counter(r['eye2'].split()[-1] for r in ibm if r['eye2'] and not r['eye_yy']).items()))
print('neither eye',sum(1 for r in ibm if not r['eye_yy'] and not r['eye2']))
# eye vs IDR date consistency
same=sum(1 for r in ibm if r['eye_yy'] and r['idr_date'] and r['eye_yy']+r['eye_ddd']==r['idr_date']); both=sum(1 for r in ibm if r['eye_yy'] and r['idr_date'])
print('eye date == IDR date',same,'of',both)
# generations
def regstyle(path):
    return None
print('fmt x idr x oldsig:',collections.Counter((r['fmt'],bool(r['idr_tr']),r['oldsig']) for r in ibm).most_common())
print('fmt x dcl_c>0:',collections.Counter((r['fmt'],int(r['dcl_c'])>0) for r in ibm).most_common())
print('fmt C letters',collections.Counter(r['cletters'] for r in ibm if r['fmt']=='C').most_common())
print('fmt x incl>0',collections.Counter((r['fmt'],int(r['incl'])>0) for r in ibm).most_common())
print('fmt x gen>0',collections.Counter((r['fmt'],int(r['gen'])>0) for r in ibm).most_common())
print('fmt x pct>0',collections.Counter((r['fmt'],int(r['pct'])>0) for r in ibm).most_common())
print('fmt x proc>0',collections.Counter((r['fmt'],int(r['proc'])>0) for r in ibm).most_common())
print('fmt totals lines/ncstmts',{f:(sum(r['L'] for r in ibm if r['fmt']==f),sum(r['nc'] for r in ibm if r['fmt']==f)) for f in 'ABC'})
print('fmt x idr version', collections.Counter((r['fmt'],r['idr_ver']) for r in ibm).most_common())
open('pls_ibm.txt','w').write('\n'.join(r['path'] for r in ibm))
open('pls_var.txt','w').write('\n'.join(r['path'] for r in var))
