# Tier 93: EXCLUSIVE, NOLOCK, UNLOCK

Issue #217, block A. Record locking of DIRECT UPDATE files
(GC28-8201-4, "The EXCLUSIVE Attribute", "The UNLOCK Statement", "The
NOLOCK Option").

## The rules

- **EXCLUSIVE** (EXCL), a file attribute; it implies RECORD and KEYED.
  A READ of an EXCLUSIVE file without NOLOCK locks the record against
  other tasks until the task REWRITEs or DELETEs it, UNLOCKs it or
  closes the file. The task that holds a lock can go on using the
  record.
- **READ ... NOLOCK** reads without locking.
- **UNLOCK FILE(f) KEY(k);** releases the lock on record k.

## Measured on PL/I (F) V5.5

Probes.

- **Q1EXC, JOB01880:** one task, a REGIONAL(1) file declared DIRECT
  UPDATE EXCLUSIVE. READ of a record twice, READ NOLOCK (the option
  before and after INTO), UNLOCK of a record read, of one read with
  NOLOCK and of key 99999 (not in the data set), READ, REWRITE and
  UNLOCK, DELETE and UNLOCK, READ of the deleted record: all run, no
  condition is raised (no KEY for 99999), and the data set holds what
  the REWRITE and DELETE left. (TITLE is an OPEN option, not an
  attribute: in the DECLARE, IEM0124I, JOB01878.)
- **Q2EXE, JOB01878:**
  - EXCLUSIVE with STREAM: IEM2818I, conflicting attribute ignored;
  - UNLOCK without KEY: IEM0237I, the statement replaced by a null
    statement;
  - READ ... NOLOCK and UNLOCK of a file without EXCLUSIVE: accepted
    without a message.

## In PL1C/370

- **parse.c:** EXCLUSIVE sets RECORD and KEYED; with STREAM it is an
  error. NOLOCK is skipped in a READ. UNLOCK is parsed (FILE and KEY,
  both required) and becomes a null statement.
- There are no tasks, so nothing is locked; that matches what one task
  sees on PL/I (F). The key of an UNLOCK is not evaluated (a deviation:
  a function reference in it is not called).

## Test

`t246exc` (Q1EXC, with the dummy records counted instead of listed):
identical in JOB01883.
