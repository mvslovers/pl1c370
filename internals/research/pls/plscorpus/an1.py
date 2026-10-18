import csv, collections
R=list(csv.DictReader(open('scan.tsv'),delimiter='\t'))
T=lambda x:x=='True'
for top in ('ibm','variants','macros','ext'):
    c=collections.Counter()
    for r in R:
        if r['top']!=top: continue
        pls_idr = 'PL' in r['idr_tr']
        c[(T(r['pstart']),T(r['prolog']),T(r['unrefd']),T(r['datd']),pls_idr, int(r['nstmtlines'])>=10)]+=1
    print(top)
    for k,v in sorted(c.items(),key=lambda x:-x[1]): print('  pstart,prolog,unrefd,datd,plsIDR,stmt>=10',k,v)
