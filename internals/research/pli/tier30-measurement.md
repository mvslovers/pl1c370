# Tier 30 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probes in each
with a `.dd` file holding its REGIONAL data set's DD statement
(UNIT=SYSDA, BLKSIZE=20): Q1REG (JOB00427), Q2REG (JOB00429; JOB00428
was a first form that passed a constant to an undeclared parameter
and looped), Q3KEY (JOB00430). The mechanism comes from GY28-6801-6,
"REGIONAL Data Sets", and from PL/I (F)'s library modules IHEOPZ
(DIRECT OUTPUT formatting) and IHEITC (SEQUENTIAL creation).

## REGIONAL(1)

- `DCL F FILE RECORD KEYED ENV(REGIONAL(1))`; `DIRECT` implies KEYED. A
  region is one unblocked F record, numbered from 0. A source key is a
  character string; its last eight characters are the region number,
  blanks counting as zeros (`KEY(' 7')` is 7). An arithmetic key
  converts to a string first. Any other character raises KEY 54.
- A dummy record has X'FF' in its first byte. READ, sequential or
  direct, returns dummy records like any other; the program has to
  recognise them.

## Creation

- **SEQUENTIAL OUTPUT** writes with BSAM in load mode, in ascending
  regions. A region that is skipped gets a dummy record: the record
  written before it, its first byte X'FF' (Q2REG: regions 1 and 2
  after `AAA...` read `X'FFC1'`, then `AAAAAAAA`). At CLOSE the rest
  of the extent is filled with such dummy records. IHEOPZ and IHEITC
  issue WRITE SD until WRITE returns 8 (no space, nothing written).
  Two tracks at BLKSIZE=20 held 172 regions, one track 86 (a 3390's
  count for 20-byte blocks; the device type itself was not read).
- A region lower than the last one raises KEY 53 (key sequence error),
  the same region KEY 52 (duplicate). Neither record is written, and
  the program goes on after a normal return (Q1REG).
- A region beyond the space is not a KEY condition. PL/I (F) ends with
  system abend D37 (Q2REG, region 86 of 86), as PL1C/370 does.
- **DIRECT OUTPUT** fills the whole extent with dummy records at OPEN.
  Their bytes after the X'FF' are what PL/I (F)'s buffer held
  (`X'FF08...'`, `X'FF0A...'` in Q2REG). PL1C/370 writes zeros there,
  so the tests never print them. Only the records written are
  compared.

## Access

- **DIRECT INPUT/UPDATE** goes through BDAM with relative block
  addressing. READ KEY, WRITE KEYFROM and REWRITE KEY overwrite or read
  the region, dummy or not. DELETE writes X'FF' followed by zeros
  (Q2REG, `DEL 2`). A region beyond the data set raises KEY 56, and the
  READ target is left unchanged.
- **SEQUENTIAL INPUT/UPDATE** reads every region in order, dummies
  included: 172 records, 168 of them dummies, after four were written.
  KEYTO gives the region number right-aligned in eight characters, with
  blanks on the left (`'       0'`). A longer variable gets more blanks
  on the left; a shorter one gets the right end (`CHAR(3)`: `'  1'`).
  REWRITE without KEY replaces the record just read.
- After OPEN ... OUTPUT on an existing data set it is created anew.

## The KEY condition

| ONCODE | Cause | Message without an on-unit, then ERROR |
|---|---|---|
| 50 | SIGNAL KEY(f) | `IHE130I FILE REG - KEY CONDITION SIGNALED` |
| 52 | duplicate region (sequential creation) | `IHE132I FILE REG - ATTEMPT TO ADD DUPLICATE KEY` |
| 53 | region lower than the last one | `IHE133I FILE REG - KEY SEQUENCE ERROR` |
| 54 | not a region number | `IHE134I FILE REG - KEY CONVERSION ERROR` |
| 56 | region beyond the data set | `IHE136I FILE REG - KEYED RELATIVE RECORD/TRACK OUTSIDE DATA SET LIMIT` |

Each message ends `AT OFFSET +nnnnn FROM ENTRY POINT name` (Q3KEY).

## Implementation

`rt/pl1bdam.asm` holds BSAM load mode (WRITE SF and SD, with WRITE's
return code passed back) and BDAM (READ DI and WRITE DI by relative
block, OPTCD=R). Sequential input uses `rt/pl1qsam.asm`, opened INPUT
or UPDAT. `tests/run_mvs.py` and `submit.py` add `<name>.dd` to both
steps of a test.

Tests `t116reg`, `t117rkey` (JOB00432) and `t118rdum` (JOB00434); with
Tiers 0 to 29: 120 programs identical (JOB00442 to JOB00444).

## Not done

REGIONAL(2) and (3) (recorded keys), INDEXED, LOCATE and READ SET,
UNBUFFERED, EVENT, keyed I/O of VARYING strings. Also missing: a DO
with several specifications (`DO K = 0, 2, 5`), which the tests avoid.
