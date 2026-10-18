# Tier 42: LOCATE KEYFROM, READ SET with KEY and KEYTO

Measured 2026-10-13 on MVSTK5-BLD with the probe
which has a REGIONAL(1) and an INDEXED
(KEYLEN=5) data set. JOB00699 stopped at its first LOCATE; JOB00700 is
the form with an ERROR on-unit and a label for each step. The test
`t143sky` is identical (JOB00704).

## LOCATE KEYFROM

- **On an INDEXED file being created, LOCATE KEYFROM works** like WRITE
  KEYFROM. The record is written at the next LOCATE, WRITE or CLOSE, with
  the values the program assigned in the buffer.
- **The key is checked first.** `K0001` after `K0012` is KEY 53 (ONKEY
  `K0001`). Then:
  - no new buffer is given, and the pointer still addresses the record
    located before;
  - that record is still open: the program's next assignments, `LOW` /
    `OUT OF ORDER`, land in it, and it is written under K0012.
- **On REGIONAL(1) the first run ended** at the first LOCATE with
  `IHE029I FILE R1 - UNSUPPORTED FILE OPERATION`. In JOB00700 a LOCATE
  after three WRITEs gave no message, but no record appeared either.
  PL1C/370 refuses LOCATE KEYFROM for a file declared REGIONAL.

## READ SET with KEY and KEYTO

- **READ SET KEY on a DIRECT file**, REGIONAL(1) or INDEXED, is
  `IHE029I ... UNSUPPORTED FILE OPERATION`, then ERROR with ONCODE 1009.
- **On a SEQUENTIAL INDEXED file** it positions at the key and points to
  the record, as READ INTO KEY does (Tier 40). `READ ... SET(P)
  KEYTO(KC)` then gives the next record and its key (K0012).

## In PL1C/370

`rt/pl1iokey.c` has three new entries:

- **`pl1_locate_key` (PL1LOCK)** checks the key against the last one.
  Only then does it write the record located before (`pl1x_flushloc`)
  and give the new buffer, marked as keyed: the hook's op 4 PUTs it with
  the key.
- **`pl1_read_set_key` (PL1RSKY)** does SETL K on a SEQUENTIAL INDEXED
  file, and IHE029I on any other.
- **`pl1_read_set_keyto` (PL1RSKT)** copies the key, as READ INTO KEYTO
  does.

WRITE KEYFROM on a file being created first writes a record that LOCATE
left.

## Not done

- LOCATE KEYFROM on REGIONAL files.
- READ SET KEY on SEQUENTIAL REGIONAL files.
- SEQUENTIAL UPDATE with READ SET KEY followed by REWRITE (not
  measured).
