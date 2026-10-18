# Tier 72: LINESIZE, PAGESIZE expressions, ANSI's TAB

PL/I (F) takes `LINESIZE(e)` and `PAGESIZE(e)` on OPEN, each an
expression. PL1C/370 took a constant PAGESIZE only and no LINESIZE at
all. ANSI X3.53-1976 adds the TAB option of OPEN (8.5.1.2) and the TAB
format item (8.7.2.10, CM147); PL/I (F) has neither.

Measured 2026-10-14 on MVSTK5-BLD with a probe (JOB01120). The test `t211lsz` is the probe, identical in JOB01125.
The TAB test `t212tab` is a twin pair: the twin places the same
characters with COLUMN. It is identical in JOB01123.

## The rules (PL/I (F))

- **LINESIZE(N * 12 + 2)** with N = 4 gives lines of 50 positions:
  - LIST items go to positions 1, 25 and 49;
  - an EDIT field of 60 characters fills the line and goes on in the
    next one.
- **A LIST item that does not fit in the rest of the line:**
  - a string goes on in position 1 of the next line: `AB` at 49 and 50,
    then `CDEF...`;
  - a number starts the next line instead. The third of `PUT LIST(1, 2,
    3, 4, 5)` is in position 1 of line 2, not in position 49.
- **PAGESIZE(N + 2)** gives pages of six lines.
- **LINENO** of the record that goes past the page is 1, on the new
  page. PL1C/370 gave 7 here; fixed.

## The rules taken from ANSI (TAB)

- **TAB(t1, t2, ...)** gives the tab positions of a PRINT file, in
  ascending order (8.5.1.2).
- **The next LIST or DATA item** goes to the first tab position t with
  t - 1 greater than the current column. Without one, it starts a new
  line, at the first tab position unless that is 1 (8.7.2.10, Case
  3.2).
- **TAB(w)** moves w tab positions on, if there are w of them left in
  the line. Otherwise it starts a new line, at its first tab position
  unless that is 1. TAB(0) does nothing, and TAB alone is TAB(1).
- **What `t212tab` shows:**
  - With tab positions 10, 30 and 45 in a line of 60, `PUT LIST('A',
    'B', 'C', 'D', 'E')` places A at 1, B at 10, C at 30, D at 45, and E
    at 10 of the next line.
  - An item ending in position 9 sends the next one to 30, since 10 - 1
    is not greater than 9.
  - `(A, TAB(5), A)` with three positions left starts a new line, at 10.

## In PL1C/370

- **parse.c:** LINESIZE, PAGESIZE and TAB on OPEN take expressions. The
  old encoding of a constant PAGESIZE in the attributes word is gone.
- **gen.c:** after PL1OPEN, `PL1OPTS(file, linesize, pagesize, tabs,
  n)`; 0 stands for an option not given.
- **rt/pl1io.c:**
  - `pl1_open_opts` sets the line width, the page size and the tab
    positions;
  - CLOSE resets them, for an implicit OPEN after it;
  - `place` follows the tab positions;
  - `pl1i_tab` is the TAB format item.
  - A number that does not fit goes to the next line. The flag
    `pl1i_place_num` marks it.

## Not done

- **LINESIZE on a file that is not PRINT:** taken, not measured.
- **A TAB option with more than 16 positions:** the rest are dropped.

## The suite

All 214 programs are identical: JOB01127 to JOB01131.
