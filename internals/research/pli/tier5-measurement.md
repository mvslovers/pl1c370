# Tier 5 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probes with their
oracle output (JOB00115, JOB00128). The test programs `tests/pli/t23rec` to
`t26npr` print what PL/I (F) prints, and write the same records **byte for
byte**: the test runner now compares the second output file (DD REPORT) in
its bytes as well as its text (JOB00136, together with Tiers 0 to 4).

## Structure mapping

PL/I (F) does not lay out a structure member after member from a
doubleword. It maps the members pairwise so that each is aligned with the
least padding, and the structure as a whole then begins at a phase from a
doubleword boundary. The record written FROM a structure starts at its
first member:

| Structure | Offsets (bytes) | Length |
|---|---|---|
| A CHAR(3), H BIN(15), B CHAR(1), F BIN(31), D DEC(3) | 0, 3, 5, 7, 11 (phase 1) | 13 |
| A CHAR(1), B BIN(31), C BIN(15), D CHAR(3) | 0, 1, 5, 7 | 10 |
| X CHAR(1), M (H BIN(15), Y CHAR(1)), F BIN(31) | 0, 1, 3, 5 | 9 |
| T(3) BIN(15), Z CHAR(1), W BIN(31) | 0, 6, 8 | 12 |
| (2) of P CHAR(1), Q BIN(31) | 0, 1; 8, 9 | 13 |

The rule PL1C/370 implements (`src/lower.c`, `layout`): members are added
one at a time; before each, the unit mapped so far may be shifted by
multiples of its largest alignment within a doubleword, to need the least
padding before the new member (the smallest phase on a tie). An element
of an array of structures is padded to keep the phase, **except after the
last element** (13 bytes for two, not 16). Padding bytes are zero in the
records PL/I (F) writes; PL1C/370 clears AUTOMATIC structures at block
entry to match.

A structure with VARYING strings in record I/O is a severe error in
PL/I (F) (IEM1090I, JOB00121), and refused by PL1C/370.

## Record I/O

READ FILE(f) INTO(v) and WRITE FILE(f) FROM(v) move the whole item --
scalar, array or structure -- as one record; ON ENDFILE works for READ as
for GET. A RECORD file on SYSIN reads the in-stream cards.

## Files that are not PRINT, record by record

Measured by reading the records back into a VARYING string with PL/I (F)
itself (P15LEN on the output of P16NPR, JOB00128):

- every LIST item is followed by one blank, the last one too:
  `'A'` and 3 make a record of 3 + 1 + 14 + 1 = 19 bytes;
- a record is at least 10 bytes, blank-padded: `'B'`, `C`, `D  E`, an
  empty SKIP record, `    3` are all 10 long;
- EDIT items get no trailing blank.

## Character to arithmetic

| Context | Rule | Example |
|---|---|---|
| assignment | to the target's attributes, truncated | `'1.236'` into DEC(7,2) is 1.23; `'  123.4 '` into BIN(31) is 123 |
| an operand of + - * / | FIXED DECIMAL(15,0) | `'1.5' * 2` is **2**; `'5' + 2` is 7 |
| a comparison with a number | the string as a number | `'10' > 9` is true |
| not a number | CONVERSION (IHE604I) | |

## Not measured yet

- REWRITE and UPDATE files (LIBC/370 record streams cannot be opened for
  update), keyed and VSAM files, ENVIRONMENT.
- The RECORD condition (record and variable of different lengths).
- Structure mapping with BIT strings (unaligned bits in PL/I (F)) and with
  members wider than a doubleword's alignment.
