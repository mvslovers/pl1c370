# Tier 32 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probes in each
with a `.dd` file holding its INDEXED data set (one DD statement,
`SPACE=(CYL,1)`, `DCB=(DSORG=IS,RECFM=F,BLKSIZE=20,KEYLEN=n,
OPTCD=LY,CYLOFL=1)`): Q1IDX (JOB00496, test `t122idx`) and Q2IKEY
(JOB00498, test `t123ikey`; JOB00497 was a first form whose keys were
all equal, so it looped). The mechanism comes from GY28-6801-6,
"INDEXED Data Sets"; the details of QISAM and BISAM are from the MVS
3.8 macros (DCB, DCBD, IHBRDWRK, SETL, CHECK, IHB01) and from our own
runs. With Tiers 0 to 31: 126 programs identical (JOB00530 to
JOB00534).

## The language

- `DCL F FILE RECORD KEYED ENV(INDEXED)`. A source key is a character
  string, cut or padded with blanks to KEYLEN: `KEYFROM('B008')` with
  KEYLEN=5 is `'B008 '`, and `'B0090XYZ'` is `'B0090'`. Keys collate
  in EBCDIC, so `'B008 '` comes after `'B0070'`.
- **SEQUENTIAL OUTPUT** creates the data set in ascending key order. A
  lower key raises KEY 53 and an equal key KEY 52; the record is not
  written and the program goes on after a normal return.
- **SEQUENTIAL INPUT** reads in key order. KEYTO gets the recorded key
  as a string is assigned: blanks on the right (`'B0010   '` into
  CHAR(8)), cut on the right (`'K1 '` into CHAR(3) from KEYLEN 2).
  Deleted records are left out (OPTCD=L).
- **SEQUENTIAL UPDATE**: REWRITE replaces the record just read, and
  DELETE without KEY deletes it.
- **DIRECT INPUT/UPDATE**:
  - READ KEY: a missing key raises KEY 51, and so does a deleted one.
  - WRITE KEYFROM adds a record, also below the lowest and above the
    highest key. An existing key raises KEY 52; a deleted key is no
    obstacle (`t124idx2`).
  - REWRITE KEY and DELETE KEY raise KEY 51 for a missing key. A
    deleted record is replaced or deleted again without a condition
    (`t124idx2`).
- Messages without an on-unit, then ERROR (Q2IKEY):
  - `IHE131I FILE IX - KEYED RECORD NOT FOUND`
  - `IHE132I ... ATTEMPT TO ADD DUPLICATE KEY`
  - `IHE133I ... KEY SEQUENCE ERROR`
  - `IHE130I ... KEY CONDITION SIGNALED`

## QISAM and BISAM, as found

`rt/pl1isam.asm`:

- **Creation:** QISAM load mode, `MACRF=PM`; PUT takes the key
  followed by the data.
- **Sequential access:** QISAM scan mode, `MACRF=(GL,SK)` for input
  and `(GL,SK,PU)` for update. `SETL B` starts at the beginning. GET
  in locate mode returns the data in R1 and **its key in R0**, not one
  in front of the other. PUTX rewrites the record in place. The EODAD
  exit sets up its base and frame on its own. That is a precaution: the
  0C6 it was first suspected of came from BISAM (below).
- **Direct access:** BISAM with **`MACRF=(RUC,WUAC)`**. Without the C,
  OPEN loads no CHECK module, DCBSETL (DCB+76) stays 1, and CHECK
  branches to address 1 (0C6, JOB00518). READ K and KU are at DCB+88,
  WRITE KN at DCB+92. The exception code is at DECB+24.
- **The data area of BISAM:** an unblocked record lies **16 bytes into
  the area**, behind the system's prefix with the overflow link
  (JOB00522). The same holds for WRITE K and KN.
- **LRECL:** with only BLKSIZE in the DD statement, OPEN ends with
  **S03B**. A DCB open exit sets LRECL to BLKSIZE, as PL/I (F)'s open
  process does.
- **Deleting:** READ KU, X'FF' in the first data byte, WRITE K.
  REWRITE is WRITE K alone; for unblocked records it needs no READ
  first.

## Not done

Blocked records, embedded keys (RKP), V records, more than one DD
statement, READ KEY on a SEQUENTIAL file (positioning), GENKEY, and
KEY 57 (no space). KEY 55 and 57 have their messages from the
GC28-8201-4 ONCODE table, not measured.
