# Tier 37: LOCATE, READ SET, REWRITE without FROM

Measured 2026-10-13 on MVSTK5-BLD: a probe (JOB00599); the test `t135loc` (JOB00601) is identical, its REPORT bytes
included.

## The rules

- `LOCATE x FILE(f) [SET(p)]` gives a buffer for the next output record
  and points p to it (without SET, x's own based pointer). The record
  has the length of x: 16 bytes for `1 R, 2 K FIXED BIN(31), 2 T
  CHAR(12)`, 8 for `1 S, 2 L FIXED BIN(15), 2 U CHAR(6)`. It is written
  by the next WRITE or LOCATE on the file, or by its CLOSE. The values
  the program put into the buffer before that are what is written.
- `READ FILE(f) SET(p)` points p to the record read. It stays there
  until the next READ.
- `REWRITE FILE(f)` without FROM, on an UPDATE file, rewrites the record
  last read with SET, as the program changed it in the buffer.
- **Reading through a variable longer than the record** (R over the
  8-byte record in Q1LOC) shows the bytes after the record: PL/I (F)'s
  buffer (`6546120 ORT`). PL1C/370's buffer holds other bytes there, so
  the test does not do this.

## In PL1C/370

`rt/pl1io.c`:

- `pl1_locate` (PL1LOCT) keeps the located record in a buffer of the
  file, at the structure's phase, cleared. `flush_locate` writes it at
  the next WRITE, LOCATE or CLOSE, or at the end of the program.
- `pl1_read_set` (PL1RSET) returns the address of the record read: the
  QSAM buffer of an UPDATE file, else the record buffer.
- `pl1_rewrite_set` (PL1RWST) puts the QSAM record back (PUTX), and for
  INDEXED files goes through the KEYED hook.

## Not done

KEY options with SET (READ ... SET KEY on INDEXED or REGIONAL files),
LOCATE with KEYFROM, and VARYING based variables in LOCATE.
