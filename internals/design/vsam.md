# Design note: VSAM (issue #229)

Status: draft for the maintainer's decision (block D). Nothing here is
built or measured by this note. File:line references were read from
pl1c370 `main` at 11ece42 on 2026-10-17; the tree was changing during
the write. "Unverified" marks every statement that
no manual read for this note and no measurement supports.

Short form: VSAM is an Optimizer feature (level OPT). PL/I (F) has no
VSAM, so the twins spell out the expected output. The run time gets a
new KEYED back end beside REGIONAL and INDEXED: an assembler glue
`rt/pl1vsam.asm` (ACB, RPL, OPEN, GET, PUT, ERASE, POINT, CLOSE) and a C
part `rt/pl1iovs.c`, reached through a descriptor hook like
`V(PL1KHOK)`, so a program without VSAM links none of it. Tiers go ESDS,
KSDS sequential, KSDS direct, RRDS, then BKWD, alternate index paths and
SAMEKEY. The hard part is not the code. It is the test fixtures: MVS
3.8j has no temporary VSAM, every cluster needs a catalog, and the
cobc370 session lost a user catalog three times doing exactly this.

## 1. What the language requires

Sources (pdftotext output of the PDFs in `internals/manuals/`):

- GC33-0009-4 (Optimizer Language Reference, Oct 1976), chapter 12
  "Record-oriented Transmission": ENVIRONMENT options for VSAM,
  pp. 168-170; GENKEY, p. 174; KEYLENGTH and KEYLOC ("Key Location
  Option (KEYLOC)"); "VSAM Organization", pp. 192-194; SAMEKEY, Part II
  pp. 375-376; ON-conditions and ONCODEs, Part II Section H,
  pp. 383-387; built-in subroutines, p. 112.
- SC33-0006-7 (Optimizer Programmer's Guide, Sep 1985), chapter 7
  "Using VSAM Data Sets from PL/I", pp. 222-261.
- SC33-0025-3 (Optimizer Execution Logic, Sep 1985), chapter 8: access
  methods per file type, Fig. 61 p. 158; the VSAM open (IBMBOPE),
  pp. 163-165; the VSAM section of the IOCB, pp. 376-377.
- SC33-0027-5 (Optimizer Messages, Feb 1982): IBM225I to IBM236I,
  pp. 213-214; IBM241I, p. 214.1; IBM829I to IBM836I, p. 236.
- SC26-3114 (PL/I for MVS & VM Language Reference, pdftotext output of
  -0): SAMEKEY, p. 373; ENVIRONMENT options list with VSAM and KEYLOC
  (text lines 14540-14550).

### 1.1 ENVIRONMENT options

The options that apply to a file declared ENVIRONMENT(VSAM) are (SC33-0006-7
p. 229; GC33-0009-4 p. 169): VSAM, BKWD, BUFND(n), BUFNI(n), BUFSP(n),
PASSWORD(spec), REUSE, SIS, SKIP, COBOL, GENKEY, SCALARVARYING. The
options checked against the data set at OPEN are RECSIZE and, for a KSDS,
KEYLENGTH and KEYLOC; a conflict raises UNDEFINEDFILE (SC33-0006-7
p. 229; message IBM225I, SC33-0027-5 p. 213). Other options are ignored.

- **VSAM**: the file accesses a VSAM data set (p. 229).
- **PASSWORD(spec)**: a CHARACTER constant without repetition factor, or
  a level-1 element STATIC unsubscripted CHARACTER variable; padded or cut
  to 8 and passed to VSAM. Wrong password: the operator gets the number
  of tries the DEFINE allowed, then UNDEFINEDFILE (p. 230; IBM228I;
  ONCODE 89, GC33-0009-4 p. 386).
- **REUSE**: an OUTPUT file treats the data set as empty at each OPEN;
  only if the cluster was defined REUSE; not with alternate indexes, BKWD,
  INPUT or UPDATE (p. 230; IBM241I; ONCODE 94).
- **BKWD**: SEQUENTIAL INPUT or UPDATE reads backwards, positioned at the
  last record at OPEN; not with REUSE or GENKEY; WRITE not allowed
  (p. 231). Not in the issue's list, but in both manuals: included here
  as a late tier.
- **SKIP**: OPTCD SKP for KEYED SEQUENTIAL INPUT or UPDATE of a KSDS
  (p. 231). **SIS**: for a KSDS through a DIRECT file (p. 232). Both are
  "never an error" to give or omit: performance only.
- **BUFND(n)**, **BUFNI(n)**, **BUFSP(n)**: data buffers, index buffers,
  total buffer space; n an integer or a FIXED BINARY(31) STATIC variable
  (p. 232).
- **GENKEY**: as for INDEXED (GC33-0009-4 p. 174): a key shorter than
  KEYLEN is a generic key. **KEYLOC(n)**: 1-based position of the
  embedded key, checked against the cluster.

The Optimizer writes record format and sizes as RECSIZE(n), BLKSIZE(n),
KEYLENGTH(n), F/FB/V/VB/U without parentheses (GC33-0009-4 ch. 12). PL/I
(F) writes F(b,r) (GC28-8201-4). PL1C/370 parses only the PL/I (F) form
today (section 2). RECSIZE and KEYLENGTH are needed for the checks above.

### 1.2 Data set types and file attributes

KSDS, ESDS, RRDS correspond roughly to INDEXED, CONSECUTIVE, REGIONAL(1)
(SC33-0006-7 p. 222). Permitted combinations (Fig. 94, p. 228):

| | SEQUENTIAL | KEYED SEQUENTIAL | DIRECT |
|---|---|---|---|
| INPUT | ESDS KSDS RRDS path | ESDS KSDS RRDS path | KSDS RRDS unique path |
| OUTPUT | ESDS RRDS | ESDS KSDS RRDS | KSDS RRDS unique path |
| UPDATE | ESDS KSDS RRDS path | ESDS KSDS RRDS path | KSDS RRDS unique path |

The statements allowed per type are Fig. 96 (ESDS, pp. 237-238), Fig. 97
(indexed: KSDS or a path, pp. 240-242) and Fig. 98 (RRDS, pp. 244-246).
EVENT appears for UNBUFFERED files; it belongs to #228 and stays refused.

### 1.3 Keys

- **KSDS** (and a path): the recorded key, embedded in the record
  (p. 224). KEYTO gives the recorded key.
- **ESDS**: the key is the RBA, CHARACTER(4), "not normally printable";
  obtained by KEYTO on WRITE or READ, used in KEY of READ and REWRITE,
  never in KEYFROM (p. 224).
- **RRDS**: the relative record number, from 1, as CHARACTER(8); a source
  key "must represent an unsigned integer", cut or padded with blanks
  (zeros) on the left; KEYTO gives CHARACTER(8) "with leading zeros
  suppressed" (pp. 226-227; GC33-0009-4 p. 193).

### 1.4 Statement semantics (summary)

- **ESDS** (pp. 237-238): load by SEQUENTIAL OUTPUT in order presented,
  KEYTO on WRITE gives the RBA. SEQUENTIAL INPUT/UPDATE read in load
  order; READ KEY(rba) repositions. UPDATE: WRITE appends; REWRITE
  without KEY rewrites the record read last, with KEY the one at the
  RBA; a REWRITE must not change the length. **No DELETE.**
- **KSDS** (pp. 239-242): load by KEYED SEQUENTIAL OUTPUT in ascending
  key order, KEYFROM required; on a non-empty KSDS a SEQUENTIAL OUTPUT
  file appends, first key above the highest. SEQUENTIAL INPUT/UPDATE: in
  key order (descending with BKWD); READ KEY positions; KEYED SEQUENTIAL
  UPDATE: WRITE KEYFROM inserts anywhere, duplicate on a unique index
  raises KEY; REWRITE with or without KEY; DELETE with or without KEY.
  DIRECT INPUT/OUTPUT/UPDATE: by key; duplicate WRITE raises KEY.
  EXCLUSIVE, UNLOCK, NOLOCK are ignored for VSAM (Fig. 97 note 1; SC33-0025-3
  p. 174: "For VSAM files the EXCLUSIVE attribute is ignored").
- **RRDS** (pp. 243-246): load by OUTPUT, DIRECT or SEQUENTIAL; SEQUENTIAL
  WRITE without KEYFROM takes the next slot, KEYTO tells which; writing
  an occupied slot raises KEY with KEYFROM, ERROR without. Sequential
  reads skip empty slots. UPDATE: REWRITE and DELETE with or without KEY.
- READ FILE(f); means IGNORE(1) (Fig. 96-98 note 2).
- "There is no concept of fixed-length records in VSAM": RECORD is not
  raised for a short record as with non-VSAM data sets (p. 233).
- Deleted records are not accessible (p. 234), unlike INDEXED's X'FF'
  records.
- Several files on one data set: PL/I opens all with MACRF=DSN; a second
  UPDATE file touching a held control interval gets ERROR 1027 (p. 236).

### 1.5 SAMEKEY

`SAMEKEY(x)`, x a RECORD file: BIT(1), '1'B after a successful operation
(or just before RECORD is raised) when the record is followed by another
with the same key; '0'B if the file is not KEYED SEQUENTIAL, not open,
not a VSAM indexed data set, or no such record follows; after another
condition '0'B if positioning changed or was lost, else unchanged
(GC33-0009-4 Part II pp. 375-376; SC26-3114 p. 373). Its use is the
nonunique alternate index path (SC33-0006-7 p. 240).

### 1.6 Conditions

ONCODEs the Optimizer defines for VSAM (GC33-0009-4 pp. 383-387): TRANSMIT
43 to 46 (index set and sequence set, input and output); KEY 51 (not
found), 52 (duplicate), 53 (KEYFROM below the previous key in sequential
creation), 55, 56, 57 (no space for a keyed record), 58 (key outside the
ranges); UNDEFINEDFILE 89 (password), 91 (option invalid for VSAM), 92
(VSAM OPEN error, or BKWD positioning failed), 93, 94 (REUSE on a
non-reusable cluster), 95 (empty alternate index); ERROR 1022 (no DASD
space for VSAM), 1025 (not enough virtual storage for VSAM), 1026 (no
position established), 1027 (record held in exclusive control), 1028
(non-mounted volume), 1029 (repositioning failed), 1030 (index upgrade
error), 1031 (invalid sequential write). The messages are IBM225I to
IBM241I and IBM829I to IBM836I (SC33-0027-5 pp. 213-236). The ONCODE of
IBM225I (an ENVIRONMENT value that does not match the cluster) is not
given in the text read: open question 7.

### 1.7 Compatibility (files not declared VSAM)

CONSECUTIVE and INDEXED files, and files without ENVIRONMENT, "are able to
access VSAM data sets without alteration"; REGIONAL files are not
(SC33-0006-7 p. 233). The library detects VSAM at OPEN with RDJFCB
(SC33-0025-3 p. 163). An INDEXED file uses VSAM's ISAM compatibility
interface when the DD has AMP='RECFM=..' or 'OPTCD=..' or NCP > 1
(p. 234). This note treats it as optional (tier V6).

### 1.8 Level

All of it is OPT: VSAM, BKWD, BUFND, BUFNI, BUFSP, PASSWORD, REUSE, SIS,
SKIP, KEYLOC, KEYLENGTH, RECSIZE, WRITE ... KEYTO and SAMEKEY are in
GC33-0009-4 and not in GC28-8201-4. GENKEY is PL/I (F)'s
(already parsed). `-lang=F` refuses each through `beyond_f(line, L_OPT, ...)`.

## 2. What PL1C/370 has today

- ENVIRONMENT is parsed in `src/parse.c:1388-1470`: INDEXED, REGIONAL(n),
  F/V/VS/VBS/U(b[,r]), BUFFERS, NCP, INDEXAREA, and the flag options
  CONSECUTIVE, LEAVE, REWIND, CTLASA, CTL360, COBOL, NOWRITE, GENKEY,
  TRKOFL. Anything else is fatal "ENVIRONMENT option %s is not supported
  yet" (`src/parse.c:1462`). VSAM, KEYLOC, RECSIZE, KEYLENGTH are not
  parsed. GENKEY sets `EO_GENKEY` (`src/pl1c.h:83`), which nothing reads
  (`grep EO_GENKEY src/` hits only the definition and the parse table).
- File attributes are bits in an int (`rt/pl1abi.h:50-61`; the last used
  is `FL_REG3 4096`). The descriptor is
  `CL8 name, F attributes, A(0), V(PL1KHOK) or 0, F recfm,lrecl,blksize`
  (`src/gen.c:6030-6041`, `rt/pl1iof.h:74-81`). Only a REGIONAL or
  INDEXED file references `V(PL1KHOK)`, so only such a program links
  `rt/pl1iokey.c`. The ABI stamp is `PL1RT002` (`rt/pl1abi.h:14`).
- `open_file` (`rt/pl1io.c:181-195`) sends a REGIONAL or INDEXED file to
  the hook; `read_record` sends sequential INDEXED and REGIONAL(2)/(3)
  reads there (`rt/pl1io.c:1039-1040`). The hook is `pl1_key_hook`
  (`rt/pl1iokey.c:773-836`): op 0 open, 1 close, 2 read, 3 rewrite, 4
  LOCATE KEYFROM write. Keyed statements enter through
  `pl1_read_key`/`pl1_write_key`/`pl1_rewrite_key`/`pl1_delete_key`
  (`rt/pl1iokey.c:864-890`), which set ONKEY first (`onkey`,
  `rt/pl1iokey.c:839-862`).
- The access method glue is assembler, one member per method, each with
  an area below the line and C prototypes in `rt/pl1rt.h:89-148`:
  `rt/pl1isam.asm` (QISAM, BISAM: open by mode, PUT, GET with an EODAD,
  PUTX, READ/WRITE K, SETL), `rt/pl1bdam.asm`, `rt/pl1bdk.asm`,
  `rt/pl1qsam.asm`. Host builds get stubs (`rt/pl1io.c:284-306`).
  `rt/pl1isam.asm` is the model for a VSAM member: a model control block
  copied into a per-file area, `DCBOFLGS` tested after OPEN, the EODAD
  entered with its own base.
- Keys: INDEXED cuts or pads a source key to KEYLEN (`fit_key`,
  `rt/pl1iokey.c:721-729`), checks an embedded key against KEYFROM
  (KEY 55, `rt/pl1iokey.c:731-741`), maps BISAM exception bits to KEY 51,
  52, 55, 57 (`rt/pl1iokey.c:743-751`).
- Checks: KEY/KEYFROM/KEYTO need KEYED; WRITE on REGIONAL or INDEXED
  needs KEYFROM; DIRECT READ, REWRITE, DELETE need KEY
  (`src/check.c:2095-2114`). KEYTO is parsed on READ only
  (`src/parse.c:3387`); WRITE ... KEYTO (ESDS, RRDS) is new syntax.
- EXCLUSIVE/UNLOCK/NOLOCK are accepted with no effect (#217,
  `src/parse.c:1381-1386`); that already matches the VSAM rule.
- **The macros are there.** `~/.local/cc370/maclib370/macros/` (TK5's
  SYS1.MACLIB and SYS1.AMODGEN) has ACB, RPL, EXLST, GENCB, MODCB,
  SHOWCB, TESTCB, POINT, ERASE, ENDREQ, CHECK, VERIFY, the DSECTs IFGACB
  and IFGRPL, and GET/PUT with `RPL=` expand to the VSAM request
  (`GET.macro`: `L 15,24(1)` the ACB from the RPL, `L 15,8(15)` its
  interface routine). The ACB macro takes MACRF ADR, CNV, SKP, DDN, DSN,
  AIX, NSR, LSR, ICI, NIS, SIS, RST, NRS (`IDAVSACB.macro:137-162`); RPL
  OPTCD takes KEY, ADR, CNV, SEQ, DIR, SKP, SYN, ASY, NUP, UPD, NSP, KEQ,
  KGE, FKS, GEN, MVE, LOC, FWD, BWD, ARD, LRD (`IDAVSOPT.macro:78-98`).
  So BKWD (BWD, LRD), SKIP (SKP), SIS, REUSE (RST), GENKEY (GEN) all have
  their macro operands on 3.8j.
- **LIBC/370 has a VSAM API**: `include/mvs/vsam.h:592-699`,
  `src/mvs/vsam/` (vsopen, vsread, vswrite, vsupdate, vsdelete, vssteq,
  vsstge, vstell). It is ACB/RPL by MODCB in inline assembler with an
  EXLST (EODAD, LERAD, SYNAD) whose exits use `PDPPRLG` frames
  (`src/mvs/vsam/@@vsopen.c:30-275`). 2.10.1 fixed KSDS dynamic PUT and
  the not-found read, measured on MVS (libc370 `CHANGELOG.md:245-258`).
  It has no BUFND/BUFNI/BUFSP, PASSWD, RST, BWD/LRD, SKP, SIS, GEN or
  NSP, and one RPL per file. libc370 `docs/consumer-notes.md:286` still
  says "no test covers it" (test/mvs/tstksds.c and tstvsrd.c exist: the
  note may be stale -- unverified).
- **Prior art on TK5: cobc370** (`~/repos/mvs/cobc370/docs/VSAM-PLAN.md`)
  built native VSAM for KSDS, ESDS and RRDS, diffed against Jay Moseley's
  VSAMIO. Findings that transfer, as cobc370 measured them:
  - No EXLST is needed: without EODAD, end of data comes back as R15 8,
    feedback 4 (slice 1).
  - A PUT needs RECLEN set; AREALEN is only the area size (slice 2).
  - REUSE clusters plus MACRF=RST make a test re-runnable without
    catalog writes (slice 2).
  - In load mode a duplicate key is feedback 12, like any out-of-sequence
    key; feedback 8 means duplicate only for a direct insert (slices 2, 5).
  - UPD makes GET hold the record for PUT/ERASE; an insert needs NUP, so
    an I-O file needs either two RPLs or an OPTCD change (slices 4, 5).
  - Position is per RPL; a direct GET leaves a position for a following
    sequential GET only with **NSP**, not NUP (slice 10).
  - An RRDS RPL needs ARG even for sequential PUT: without it, S0C4 on
    the first PUT (slice 8).
  - Catalog incidents: three of four DEFINE CLUSTERs against one user
    catalog broke it (IEC331I 024-002, IEC333I), each with condition code
    0; DEFINE USERCATALOG on one volume put Hercules into a program
    interrupt loop. The working setup was a separate fixture catalog
    with its own data space on a work volume, REUSE clusters defined once
    ("The fixture catalog", "The rebuild", "DEFINE USERCATALOG on SVD004
    crashed MVS"). Which stand that was is not stated there; it is not
    known to be m38b.

## 3. Proposed design

### 3.1 Compiler

- `src/parse.c` ENVIRONMENT: add VSAM, BKWD, REUSE, SIS, SKIP (flags),
  KEYLOC(n), KEYLENGTH(n), RECSIZE(n), BUFND/BUFNI/BUFSP(n | variable),
  PASSWORD('c' | variable). Each calls `beyond_f(line, L_OPT, ...)`.
  VSAM sets a new `FL_VSAM` (8192) with FL_RECORD. Refuse VSAM together
  with INDEXED, REGIONAL or CONSECUTIVE, and the combinations the manual
  forbids (REUSE with BKWD, BKWD with GENKEY), at compile time.
- The Optimizer's record-format spelling (F, FB, V, VB, U without
  parentheses, BLKSIZE(n)) is not needed for VSAM, where record format
  options are ignored. Keep it out of this issue (open question 9).
- `src/check.c`: per type the compiler cannot know KSDS from ESDS from
  RRDS (the cluster decides at OPEN), so the type rules (no DELETE on an
  ESDS, no SEQUENTIAL OUTPUT KSDS without KEYFROM) are run-time checks
  with ERROR 1009 ("conflicts with the file attributes"), as INDEXED's
  DIRECT OUTPUT refusal is now (`rt/pl1iokey.c:672-676`). What is
  statically wrong stays a compile error: WRITE on a BKWD file, KEYTO on
  WRITE for a file that is not VSAM.
- `src/parse.c:3387`: allow KEYTO on WRITE (and LOCATE) for VSAM files.
- `src/resolve.c`: SAMEKEY(f), a built-in function, BIT(1), L_OPT, ->
  `PL1SAMK` in the VSAM member, so only VSAM programs link it.
- `src/gen.c` descriptor: a VSAM file gets `V(PL1VHOK)` in the hook
  word and an extension after BLKSIZE:
  `F flags (BKWD REUSE SIS SKIP GENKEY), F keyloc, F keylen, F recsize,
  A(bufnd) A(bufni) A(bufsp) A(password)` -- each A pointing at a
  constant or at the STATIC variable, 0 for not given. Only VSAM
  descriptors are longer, so other programs are unchanged in size.
- `rt/pl1abi.h`: FL_VSAM, the extension's layout, `PL1_ABI_STAMP`
  to `PL1RT003` (the descriptor layout changes).

### 3.2 Run time

`rt/pl1iovs.c` (C) and `rt/pl1vsam.asm`, reached through
`V(PL1VHOK)`. `open_file` and `read_record` gain a branch for FL_VSAM
next to the KEYED one (`rt/pl1io.c:186-195`, `:1039`). The keyed entry
points (`pl1_read_key` and the others) dispatch on FL_VSAM.

**Assembler glue** (modelled on `rt/pl1isam.asm`): an area per file,
below the line, fullword aligned: ACB (76 bytes, from IFGACB), RPL (76,
IFGRPL), a 4-byte ARG word (RRDS, ESDS RBA), a SHOWCB/MODCB list. Entry
points, called from C with R1 at the argument values:

    int pl1_vopen(void *a, const char dd8[8], int macrf, const struct vopt *o)
        copy the model ACB and RPL, DDNAME, MACRF bits, BUFND, BUFNI,
        BUFSP, PASSWD; OPEN; return ACBERFLG (0 = open)
    void pl1_vinfo(void *a, int v[])   SHOWCB ACB: KEYLEN, RKP, LRECL,
        and the type (KSDS, ESDS, RRDS) -- how to tell them apart from
        the ACB after OPEN is a probe (P4)
    int pl1_vreq(void *a, int op, int optcd, void *area, int len,
                 void *arg, int keylen)
        op GET, PUT, ERASE, POINT, ENDREQ; sets AREA, AREALEN, RECLEN,
        ARG, KEYLEN and the OPTCD bits by direct stores into the RPL
        (IFGRPL names), issues the request; returns R15 << 8 | feedback
    int pl1_vfdbk(void *a, int v[])    RECLEN, RBA after a request
    void pl1_vclose(void *a)

No EXLST, as cobc370 found: R15 and the feedback code carry end of data
and every error, and no exit enters the run time on VSAM's terms. One
RPL per file; the OPTCD is set per request (NUP for an insert, UPD for a
read that a REWRITE or DELETE follows, NSP for a keyed read that a
sequential read follows). Direct stores into ACB and RPL instead of
MODCB: the run time sets the fields each request anyway, and a store
cannot fail. MODCB stays the fallback if a field turns out to need it
(open question 1).

**Statement to request** (KSDS shown; ESDS uses ADR, no ERASE; RRDS uses
KEY with a 4-byte binary ARG):

| File | Statement | Request |
|---|---|---|
| SEQUENTIAL OUTPUT | WRITE KEYFROM | PUT SEQ NUP (MACRF OUT, RST with REUSE) |
| SEQUENTIAL INPUT | READ | GET SEQ (BWD after POINT LRD with BKWD) |
| | READ KEY | GET DIR NSP KEQ (GEN if GENKEY and short key; SKP if SKIP) |
| SEQUENTIAL UPDATE | READ | GET SEQ UPD |
| | REWRITE | PUT UPD (the held record), or GET DIR UPD KEQ + PUT with KEY |
| | DELETE | ERASE (held), or GET DIR UPD + ERASE with KEY |
| | WRITE KEYFROM | PUT DIR NUP (insert) |
| DIRECT INPUT | READ KEY | GET DIR KEQ |
| DIRECT OUTPUT | WRITE KEYFROM | PUT DIR NUP |
| DIRECT UPDATE | READ KEY | GET DIR UPD KEQ |
| | REWRITE KEY | GET DIR UPD (unless held) + PUT UPD |
| | DELETE KEY | GET DIR UPD + ERASE |

ONKEY: `onkey()` (`rt/pl1iokey.c:839-862`) knows only the REGIONAL and
INDEXED key shapes; the VSAM back end sets ONKEY itself on KEY and
RECORD -- KSDS the recorded key (KEYLEN), ESDS the 4-character RBA, RRDS
the CHARACTER(8) record number -- what the Optimizer gives there is not
stated in the text read (to be decided with question 8).

KEYTO: KSDS the key from the record (RKP, KEYLEN); ESDS the RBA (SHOWCB
RPL FIELDS=RBA) as CHARACTER(4); RRDS the ARG word after the request,
edited to CHARACTER(8), leading zeros suppressed (to be measured against
the manual's words: blanks, open question 8).

**Records**: input files use GET with OPTCD LOC: VSAM returns the
address of the record in its own buffer, the C part copies
min(record, target) and raises RECORD 21/22 as now. No buffer of ours.
UPDATE and OUTPUT files use MVE: one buffer of the cluster's maximum
LRECL (SHOWCB ACB LRECL after OPEN), allocated at OPEN, freed at CLOSE.
LOCATE writes into that buffer; READ SET points into it (LOC buffers are
valid only until the next request; to be checked against PL/I's READ SET
lifetime rule, open question 10).

**Error mapping** (proposal; VSAM feedback codes as cobc370 measured them
and as the OS/VS VSAM Programmer's Guide gives them -- that guide is not
in `internals/manuals/`, so every code not measured by cobc370 is
unverified; probe P5 measures all of them on m38b):

| R15 / feedback | Meaning | PL/I |
|---|---|---|
| 8 / X'04' | end of data | ENDFILE (sequential), KEY 51 (keyed read) |
| 8 / X'08' | duplicate key (direct insert) | KEY 52 |
| 8 / X'0C' | out of sequence (load) | KEY 53 (KEYFROM), ERROR 1031 otherwise |
| 8 / X'10' | record not found | KEY 51 |
| 8 / X'14' | control interval held | ERROR 1027 |
| 8 / X'18' | volume not mounted | ERROR 1028 |
| 8 / X'1C' | no space | KEY 57 (keyed), ERROR 1022 |
| 8 / X'20' | invalid RBA | KEY 51 (ESDS) |
| 8 / X'24' | key outside ranges | KEY 58 |
| 8 / X'28' | not enough virtual storage | ERROR 1025 |
| 8 / X'2C' | area too small | RECORD 22 (cannot happen with LOC) |
| 8 / X'58' | no position | ERROR 1026 |
| 8 / X'5C' | PUT/ERASE without a held record | ERROR 1007 |
| 8 / X'6C' | wrong record length (RRDS, REWRITE of ESDS) | RECORD, or ERROR 1009 |
| 12 / any | physical error | TRANSMIT 41, 42 (43 to 46 by component) |
| 0 / X'08' after GET | more records with this key (nonunique path) | SAMEKEY '1'B |

OPEN errors (ACBERFLG) go to UNDEFINEDFILE: 89 password, 91, 92, 94 REUSE
on a non-reusable cluster, 95 empty alternate index; the IBM22xI message
texts as the Optimizer's, in the form PL1C/370's other run-time messages
take (`rt/pl1iokey.c:32-38` is today's form for a missing DD).

### 3.3 Compatibility (optional, V6)

An INDEXED file on a KSDS: MVS 3.8j's OPEN may route a DSORG=IS DCB on a
VSAM cluster to VSAM's ISAM interface by itself (unverified; probe P7).
If it does, PL1C/370 needs no code, and PL/I (F) is a real oracle: the
same INDEXED program runs under both against a KSDS. CONSECUTIVE files
on an ESDS go through LIBC/370's BSAM (`fopen`), and BSAM cannot open a
VSAM data set (unverified); detecting it (RDJFCB, as the Optimizer did)
and switching to the VSAM back end is real work for little use.
Proposal: V6 = the probe and, if the ISAM interface works, a test that
documents it; no CONSECUTIVE compatibility.

## 4. Tiers, twins and probes

Twins: PL/I (F) has no VSAM. Each test is `<name>.opt.pli` (the VSAM
program) and `<name>.pli` (the PL/I (F) twin that PUTs the expected
SYSPRINT, as for every feature PL/I (F) lacks). That checks the program's
view only. What lands in the cluster is checked twice: the test program
reads its own cluster back through a second file, and one test per tier
gets an IDCAMS PRINT step after the PL1C/370 step whose output is
compared with a stored expectation -- a tool that is not PL1C/370's own,
so reading code does not vouch for writing code.

**V0 -- runner and fixtures (no compiler change).**
- `tests/run_mvs.py`: `<name>.ams`, IDCAMS input run as a step before the
  PL1C/370 step (`.amsf` before the PL/I (F) step for V6); `<name>.dd`
  already adds the cluster DDs (`tests/run_mvs.py:327`). A VSAM test is a
  job of its own if its cluster is shared.
- Fixtures, once, by the maintainer (catalog writes; open question 2):
  a user catalog, an alias, a data space, REUSE clusters.

**V1 -- ESDS.** ENVIRONMENT(VSAM) parsed; FL_VSAM; descriptor extension;
PL1RT003; open/close; SEQUENTIAL OUTPUT (load, REUSE), SEQUENTIAL INPUT,
KEYTO on WRITE and READ (RBA), READ KEY(rba), SEQUENTIAL UPDATE REWRITE
(same length), WRITE append, DELETE refused (ERROR 1009). Tests: load and
read back; KEYTO and READ KEY with a saved RBA; REWRITE; append without
REUSE (cluster emptied by the `.ams` step); missing DD (UNDEFINEDFILE 84).
`-lang=F` refusal of each `.opt.pli` is automatic.

**V2 -- KSDS sequential.** Load with KEYFROM (KEY 53 out of order, 52
duplicate -- both feedback 12 in load mode, so the C part must tell them
apart by comparing with the last key, as `pl1_locate_key` does for
INDEXED, `rt/pl1iokey.c:912-918`); SEQUENTIAL INPUT with KEY positioning
and KEYTO; GENKEY; SKIP; KEYED SEQUENTIAL UPDATE: REWRITE, DELETE, WRITE
insert; KEYLOC/KEYLENGTH/RECSIZE checks -> UNDEFINEDFILE; embedded key
mismatch on REWRITE. Tests per behaviour, plus one IDCAMS PRINT.

**V3 -- KSDS direct.** DIRECT INPUT, OUTPUT, UPDATE; SIS; KEY 51/52;
BUFND/BUFNI/BUFSP passed to the ACB (only checked to open, P3 says
whether SHOWCB can read them back); PASSWORD if P8 allows a test without
an operator prompt.

**V4 -- RRDS.** SEQUENTIAL and DIRECT, CHARACTER(8) keys (KEY 54 on a
non-digit, as REGIONAL), KEYTO, empty slots skipped, occupied slot: KEY
with KEYFROM, ERROR without.

**V5 -- BKWD, LOCATE/READ SET, paths, SAMEKEY.** BKWD (POINT LRD, BWD);
LOCATE and READ SET on VSAM; an alternate index path (unique and
nonunique), DIRECT refused on a nonunique path, SAMEKEY. Only if P6 shows
DEFINE AIX, DEFINE PATH and BLDINDEX work on m38b.

**V6 -- compatibility probe** (section 3.3).

**Probes to run first (listed, not submitted):**
- P1 read-only: `LISTCAT ALIAS` / `LISTCAT USERCATALOG` on m38b, to see
  which catalogs exist, which volume has room for a fixture catalog, and
  whether an alias for HERC01 test data exists. (IDCAMS LISTCAT is a job:
  confirm-first.)
- P2 fixture setup (catalog writes; only after a DASD checkpoint of the
  stand, as cobc370 did): `DEFINE USERCATALOG (NAME(UCPL1C) VOLUME(v)
  CYLINDERS(5 1))`, `DEFINE ALIAS (NAME(PL1CVS) RELATE(UCPL1C))`,
  `DEFINE SPACE (VOLUMES(v) CYLINDERS(30 5)) CATALOG(UCPL1C)`, then per
  fixture `DEFINE CLUSTER (NAME(PL1CVS.KSDS1) INDEXED KEYS(5 0)
  RECORDSIZE(80 80) REUSE ...)`, an ESDS (NONINDEXED), an RRDS
  (NUMBERED), a KSDS with KEYS(6 10), a non-REUSE KSDS, and for V5 an AIX
  with NONUNIQUEKEY UPGRADE, a PATH, BLDINDEX. Names are proposals.
- P3 an assembler probe (tests/mvs style) that OPENs each cluster and
  prints SHOWCB ACB FIELDS=(KEYLEN,RKP,LRECL,BUFND,BUFNI,CINV,...)
  -- which fields 3.8j's SHOWCB has is part of the question -- and the
  region in use before and after OPEN (or IEF374I VIRT of two programs
  that differ only by the OPEN), to measure VSAM's storage per file.
- P4 how the run time tells KSDS, ESDS and RRDS apart after OPEN (an ACB
  or SHOWCB field; libc370 takes the type from its caller instead).
- P5 the feedback codes: one assembler probe per error in section 3.2's
  table, printing R15 and the feedback byte.
- P6 DEFINE AIX, DEFINE PATH, BLDINDEX on 3.8j; a GET through a nonunique
  path, R15 0 with feedback 8 when another record with the key follows.
- P7 the ISAM interface: an existing INDEXED test (t123ikey-like) under
  PL/I (F) and PL1C/370 with its DD pointing at a KSDS (with and without
  AMP='RECFM=F').
- P8 PASSWORD: whether VSAM password checking prompts the operator on
  TK5 (RAKF installed); a prompt is a WTOR the runner would have to
  answer.
- P9 MACRF=DSN on two files of one cluster, and ERROR 1027 when both
  update the same control interval.

## 5. Cost and memory

Code size (estimates, from the size of the INDEXED part):
`rt/pl1iovs.c` 700-900 lines, `rt/pl1vsam.asm` 300-400 lines, parse and
check 200 lines, gen 50 lines, runner 40 lines, 20-25 test programs. Six
tiers. The fixture work (V0) is the slowest part, because each catalog
operation is confirm-first and the stand must be checkpointed before it.

Memory -- MVS's first constraint:
- A program without VSAM: **nothing**. The descriptor hook keeps
  `rt/pl1iovs.c` and `rt/pl1vsam.asm` out of the link, as `V(PL1KHOK)`
  does for KEYED files. Non-VSAM descriptors keep their size.
- A program with VSAM: the two members, estimated 6-9 KB of code
  (unverified until built). SAMEKEY in the same member.
- Per open VSAM file: the area (ACB 76 + RPL 76 + ARG + lists, about
  200 bytes), the pfile fields (tens of bytes), two key buffers of
  KEYLEN, and for UPDATE/OUTPUT one record buffer of the maximum LRECL.
  Input files need no record buffer (GET LOC).
- VSAM's own storage, GETMAINed by OPEN in the region: data buffers
  (BUFND, default STRNO+1 = 2, each a data CI), index buffers (BUFNI,
  default STRNO = 1, an index CI) and control blocks. The defaults are
  the VSAM Programmer's Guide's, not measured on 3.8j: for a 4 KB data CI
  and a 512-byte index CI that is about 9 KB plus control blocks per
  KSDS (unverified; P3 measures it). Proposal: never raise BUFND/BUFNI
  ourselves; pass the program's values through, nothing more. The
  manuals' advice to raise them is for speed, not for us to follow.
- VSAM's modules live in the system (unverified for 3.8j: LPA or
  loaded into the region; P3 shows it by the region difference).

## 6. Risks

- **The catalog.** The cobc370 incidents (section 2) are the main risk:
  catalog damage with condition code 0, and one MVS crash. Fixtures in a
  catalog of their own, REUSE clusters defined once, a DASD checkpoint
  before every catalog-writing job. m38b is a shared stand (the soak
  results already carry a fetch-error caveat, `TODO.md`): a broken
  catalog there hurts other sessions.
- **No temporary VSAM** on 3.8j: tests cannot be self-contained as the
  REGIONAL ones are (`&&` data sets in `<name>.dd`). Tests on one cluster
  cannot run in parallel jobs.
- **Feedback codes** are taken from cobc370's measurements and the VSAM
  guide's numbering; the 3.8j VSAM is old and may differ (P5).
- **Operator prompts**: a wrong PASSWORD or a not-mounted volume makes
  VSAM ask the operator. A test that triggers one hangs.
- **PL/I (F) cannot check any of it**: every expected line is written by
  hand from the manuals. The IDCAMS PRINT check and the read-back are the
  only independent views of the data.
- **Interplay**: PLICKPT (#233) with an open VSAM file -- whether 3.8j's
  checkpoint/restart supports VSAM is unverified.

## 7. Open questions for the maintainer

1. Own assembler glue (this note) or extend LIBC/370's `vs*` API with
   buffers, password, RST, BWD, SKP, SIS, GEN, NSP and LOC, and use it?
   The glue keeps the PL/I semantics in one place and needs no libc370
   release; extending libc370 helps cobc370 and C programs too.
2. May a fixture user catalog be defined on m38b, on which volume, and
   who runs the setup (with a DASD checkpoint first)?
3. REUSE fixtures emptied at each OPEN (cobc370's way), or DELETE/DEFINE
   per run in a `.ams` step (more catalog writes)?
4. Compatibility: is the ISAM-interface probe (V6) enough, with no
   CONSECUTIVE-on-ESDS support?
5. Alternate index paths and SAMEKEY (V5): worth a tier, or SAMEKEY
   only as a function that answers '0'B outside paths?
6. PASSWORD: support and test it, or support it untested if P8 shows
   operator prompts?
7. ONCODEs where the manuals are silent (IBM225I's code, the feedback
   mapping in 3.2): accept the proposal?
8. RRDS KEYTO "with leading zeros suppressed": blanks in their place
   (this note's reading), or a shorter string?
9. The Optimizer's record-format spelling (FB, RECSIZE(n), BLKSIZE(n)):
   part of this issue, or an issue of its own? VSAM needs RECSIZE and
   KEYLENGTH only for checking.
10. READ SET on VSAM input: point into VSAM's buffer (LOC, valid until the
    next request) or copy into a buffer of ours (one LRECL per file)?
11. BKWD is not in #229's list but is OPT: include it (V5)?
