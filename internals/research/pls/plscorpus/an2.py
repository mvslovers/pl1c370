import csv, collections
R=list(csv.DictReader(open('scan.tsv'),delimiter='\t'))
T=lambda x:x=='True'
ibm=[r for r in R if r['top']=='ibm']
c=collections.Counter(r['idr_tr'][:3] if r['idr_tr'] else '' for r in ibm); print(c.most_common(20))
c=collections.Counter(r['idr_tr'] for r in ibm if r['idr_tr'] and not r['idr_tr'].startswith('PLS')); print(c.most_common(30))
def grp(f,n=8):
    g=[r for r in ibm if f(r)]; print(len(g)); 
    for r in g[:n]: print('   ',r['path'],r['lines'],r['nstmtlines'],r['nstmts'],r['idr_tr'],r['eye_name'],r['eye_yy'])
print('no markers stmt>=10'); grp(lambda r: not(T(r['pstart']) or T(r['prolog']) or T(r['unrefd']) or T(r['datd'])) and not r['idr_tr'] and int(r['nstmtlines'])>=10, 15)
print('pstart only'); grp(lambda r: T(r['pstart']) and not T(r['prolog']) and not T(r['unrefd']) and not r['idr_tr'])
print('IDR only'); grp(lambda r: not(T(r['pstart']) or T(r['prolog']) or T(r['unrefd']) or T(r['datd'])) and r['idr_tr'])
