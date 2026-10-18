#!/usr/bin/env python3
"""Scan mvs38-ibmsrc for PL/S compiler output. Read-only. Writes scan.tsv next to this script."""
import os, re, collections, hashlib, json, sys
ROOT = '/Users/mike/repos/mvs/mvs38-ibmsrc'
OUT = os.path.dirname(os.path.abspath(__file__))

END_RE = re.compile(r"^\S*\s+END\b.*?\(C'([^']*)',(\d+),(\d+)\)")
EYE2_RE = re.compile(r"DC\s+C'[^A-Z0-9@#$ ]?([A-Z0-9@#$]{1,8})\s+(\d\d)\s+([A-Z]{3})\s+(\d\d)")
EYE_RE = re.compile(r"DC\s+C'[^A-Z0-9@#$ ]?([A-Z0-9@#$]{1,8})\s+(\d\d)\.(\d\d\d)")
STMT_RE = re.compile(r'^\d{4}$')
DCL_SRC_RE = re.compile(r'^\*\s*(DCL|DECLARE)\b')          # DCL as comment
DCL_BARE_RE = re.compile(r'^\s+(DCL|DECLARE)\s')          # DCL without '*' (pure PL/S)
PROC_RE = re.compile(r'^\*.*\bPROC(EDURE)?\b.*(OPTIONS|;)')

rows = []
for top in ('ibm', 'variants', 'macros', 'ext'):
    for dp, dn, fn in os.walk(os.path.join(ROOT, top)):
        for f in fn:
            if f.startswith('.'):
                continue
            p = os.path.join(dp, f)
            raw = open(p, 'rb').read()
            h = hashlib.sha256(raw).hexdigest()
            lines = raw.decode('latin-1').splitlines()
            r = dict(path=os.path.relpath(p, ROOT), top=top, member=os.path.splitext(f)[0],
                     sha=h, lines=len(lines))
            stmts = set(); cstmts = set(); stmtsB=set(); cstmtsB=set(); posA=posB=0; eye2=None; stmtsC=set(); cstmtsC=set(); posC=0; cletters=collections.Counter()
            ncomment = nplscmt = nstmtlines = 0
            pstart = prolog = unrefd = datd = False
            endidr = None; eye = None
            dcl_c = dcl_b = 0; incl = 0; gen = 0; pct = 0; proc = 0; gotopls = 0; mend = 0; macro = 0
            paired = 0; prevsemi = False; regeq = False; oldsig = False; baresemi = 0
            for i, l in enumerate(lines):
                l80 = l.ljust(80)
                had = posA + posB + posC
                if l.startswith('*'):
                    ncomment += 1
                    if l.startswith('*/*'):
                        nplscmt += 1
                    if DCL_SRC_RE.match(l): dcl_c += 1
                    if '%INCLUDE' in l: incl += 1
                    if re.match(r'^\*\s*(\w+:\s*)?GEN\b', l): gen += 1
                    if re.match(r'^\*\s*%', l): pct += 1
                    if '%GOTO' in l: gotopls += 1
                    if PROC_RE.match(l): proc += 1
                else:
                    if DCL_BARE_RE.match(l): dcl_b += 1
                    if '@PSTART' in l: pstart = True
                    if l.startswith('@PROLOG') or ' @PROLOG' in l[:20]: prolog = True
                    if 'AGO   .@UNREFD' in l or 'AGO .@UNREFD' in l: unrefd = True
                    if l.startswith('@DATD'): datd = True
                    m = END_RE.match(l)
                    if m: endidr = m.groups()
                    if eye is None and i < 40:
                        m = EYE_RE.search(l[:72])
                        if m: eye = m.groups()
                    tok = l80[9:15].strip()
                    if tok == 'MEND': mend += 1
                    if tok == 'MACRO': macro += 1
                s = l80[67:71]
                if STMT_RE.match(s) and l80[66] == ' ' and l80[71] == ' ':
                    stmts.add(int(s)); posA += 1
                    if l.startswith('*'): cstmts.add(int(s))
                else:
                    s = l80[66:70]
                    if STMT_RE.match(s) and l80[65] == ' ' and l80[70:72] == '  ':
                        stmtsB.add(int(s)); posB += 1
                        if l.startswith('*'): cstmtsB.add(int(s))
                    else:
                        m = re.match(r'(\d\d)([A-Z.])(\d{4})$', l80[64:71])
                        if m and l80[63] == ' ' and l80[71] == ' ':
                            stmtsC.add(int(m.group(3))); posC += 1; cletters[m.group(2)] += 1
                            if l.startswith('*'): cstmtsC.add(int(m.group(3)))
                got = (posA + posB + posC) > had
                if not l.startswith('*'):
                    if got and prevsemi: paired += 1
                    if re.match(r'^@(0?[0-9A-F]|1[0-5])\s+EQU\s', l): regeq = True
                    prevsemi = False
                    if l.startswith('.@001    ANOP') or l.startswith('         LCLA  &T,&SPN'): oldsig = True
                    if l[:72].rstrip().endswith(';'): baresemi += 1
                elif ';' in l[:72]:
                    prevsemi = True
                if eye2 is None and i < 40:
                    m = EYE2_RE.search(l[:72])
                    if m: eye2 = m.groups()
            fmt = max((posA,'A'),(posB,'B'),(posC,'C'))
            fmt = fmt[1] if fmt[0] else ''
            if fmt == 'B': stmts, cstmts = stmtsB, cstmtsB
            if fmt == 'C': stmts, cstmts = stmtsC, cstmtsC
            nstmtlines = max(posA, posB, posC)
            r.update(oldsig=oldsig, baresemi=baresemi, paired=paired, regeq=regeq, fmt=fmt, posA=posA, posB=posB, posC=posC, cletters=''.join(sorted(cletters)), eye2=' '.join(eye2) if eye2 else '', ncomment=ncomment, nplscmt=nplscmt, nstmtlines=nstmtlines,
                     nstmts=len(stmts), maxstmt=max(stmts) if stmts else 0,
                     minstmt=min(stmts) if stmts else 0, ncstmts=len(cstmts),
                     pstart=pstart, prolog=prolog, unrefd=unrefd, datd=datd,
                     idr_tr=endidr[0] if endidr else '', idr_ver=endidr[1] if endidr else '',
                     idr_date=endidr[2] if endidr else '',
                     eye_name=eye[0] if eye else '', eye_yy=eye[1] if eye else '',
                     eye_ddd=eye[2] if eye else '',
                     dcl_c=dcl_c, dcl_b=dcl_b, incl=incl, gen=gen, pct=pct, proc=proc,
                     gotopls=gotopls, macro=macro, mend=mend)
            rows.append(r)

keys = list(rows[0].keys())
with open(os.path.join(OUT, 'scan.tsv'), 'w') as fo:
    fo.write('\t'.join(keys) + '\n')
    for r in rows:
        fo.write('\t'.join(str(r[k]) for k in keys) + '\n')
print(len(rows), 'files')
