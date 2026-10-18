# Tier 95: ENVIRONMENT, the record format

Issue #215, block A. The ENVIRONMENT attribute beyond REGIONAL and
INDEXED (GC28-8201-4, "The ENVIRONMENT Attribute"; GC28-6594-7, "Record
formats").

## The rules

- The record format options `F(blksize[,lrecl])`, `V(blksize[,lrecl])`,
  `U(blksize)`, `VS`, `VBS`, with CTLASA and CTL360 for the control
  character, give the data set's DCB.
- BUFFERS(n), NCP(n), INDEXAREA, NOWRITE, GENKEY, TRKOFL, LEAVE,
  REWIND, COBOL and CONSECUTIVE are options of the access methods.

## Measured on PL/I (F) V5.5

Probes (the table is in its README).

- **Q1ENV, JOB01874:** the DDs give no DCB, and the data sets get the
  ENVIRONMENT's: `F(41)` on a PRINT file is FA 41/41, with lines of 40
  characters after the control character; `V(64,60)` is VB 60/64, not
  V; `F(800,80)` is FB 80/800; `U(100)` is U 100/100, holding the
  30-byte record written.
- **Q2ENV, JOB01876:** `ENV(F(800,80))` against a DD with
  `DCB=(RECFM=FB,LRECL=80,BLKSIZE=1600)`, and with `DCB=(BLKSIZE=400)`:
  BLKSIZE 800 both times. The ENVIRONMENT wins, as the values of a DCB
  macro win over the DD's in the merge at OPEN.

## In PL1C/370

- **parse.c** takes every ENVIRONMENT option of PL/I (F) and keeps the
  record format, the sizes and the options; **gen.c** works out RECFM,
  LRECL and BLKSIZE as PL/I (F) does (U: LRECL = BLKSIZE; F: B when
  LRECL is below BLKSIZE; V: LRECL = BLKSIZE - 4 when not given, B with
  an LRECL; VS: S; VBS: B and S; PRINT or CTLASA: A; CTL360: M) and
  puts them in the file's descriptor (`rt/pl1abi.h`, ABI stamp
  PL1RT002). Measured are the four forms of Q1ENV; V without an LRECL,
  VS, VBS, CTLASA and CTL360 follow the manual, not a measurement.
- **pl1io.c** hands them to LIBC/370's `fopen` as `recfm=`, `lrecl=`,
  `blksize=` (`env_mode`). LIBC/370 puts them in the DCB before OPEN
  since 2.13.0 (libc370#554), so they win over the DD and the data set
  label as on PL/I (F).
- A PRINT file's line size before OPEN's LINESIZE is the record's,
  less the control character (and a V record's RDW): F(41) gives 40.
- A record read takes its length from `fread`, which returns it since
  LIBC/370 2.14.0 (libc370#559): a U record reads back at the length it
  was written. The run time now needs LIBC/370 2.14.0.
- The access-method options (BUFFERS, NCP, ...) are accepted and have
  no effect (`docs/language-levels.md`).

## Test

`t247env`: P1, S2, R3, R4 as in Q1ENV, written to `&&` data sets
without DCB, then read back record by record and listed with their
lengths. Identical in JOB01953; the full suite 249 of 249 in JOB01956
to JOB01974, on LIBC/370 2.14.0.

On the way: P1 first came out as one 100-character line cut into
41-byte records (JOB01910, JOB01913): the file's line buffer was set up
before its descriptor was attached, so the line size was still 120.
