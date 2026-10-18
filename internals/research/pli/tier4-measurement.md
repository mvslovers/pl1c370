# Tier 4 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probes (with `P11IO.in` as SYSIN) and `P12EDIT.pli`, oracle output beside them
(JOB00101/102). The test programs `tests/pli/t19io` to `t22page` print byte
for byte what PL/I (F) prints, the second output file included (JOB00114,
together with Tiers 0 to 3).

## PUT EDIT

| Rule | Example |
|---|---|
| F(w,d) **rounds** half up | 12.345 in F(8,2) is `12.35`; in F(8) `12` |
| A field too narrow keeps the **rightmost** digits, with no message | 123456 in F(3) is `456` |
| A fraction has a digit before the point | 0.5 in F(5,2) is ` 0.50` |
| A takes the value's length; A(w) truncates or pads | `ABCDE` in A(3) is `ABC`, in A(8) `ABCDE   ` |
| X(w) writes blanks; COL(n) moves to column n, **on a new line** when the line is already past it | `BACK` at COL(30), then COL(5) puts `W` on the next line |
| SKIP(n) and PAGE inside the format list control the line | SKIP(2) gives `0` |
| The format list starts over when items remain | `PUT EDIT(T) (F(6))` for four elements |
| A string with an F format is an error | IHE604I, CONVERSION |
| `(n)'string'` repeats a string constant | `(3)'AB'` is `ABABAB` |
| The first PUT EDIT of a program writes on the first line, no blank line | `1F(8,2)       12.35` |

## Files that are not PRINT

A STREAM OUTPUT file without PRINT (here DD REPORT) has no control
characters and no tab positions: PUT LIST separates items by **one blank**
and writes **strings in quotes** (`'TO REPORT'              3`). PL/I (F)
needs a block size for such a file when it creates the data set (IHE153I
otherwise, JOB00101); the tests give the DD a DCB.

## PRINT files paginate

A PRINT file has 60 lines to a page: single-spaced output puts `1` on
every 60th record (records 0, 60, 120, ... in JOB00110). Lines are counted
by their control characters (blank 1, `0` 2, `-` 3); a record that would
print below line 60 starts a new page. `t22page` checks it across
SKIP(2), PAGE and SKIP(3).

## GET

- GET LIST reads items separated by blanks or commas, across records;
  strings in quotes, `'101'B` for a bit string.
- **An item without quotes is an arithmetic constant**: `WXYZ` read into a
  CHARACTER target is IHE604I, CONVERSION (JOB00106). Strings need quotes.
- GET EDIT reads fields of the widths its formats give; F(w,d) on a field
  without a point puts the point d digits from the right (`12345` in
  F(5,2) is 123.45). COL(1) goes on to the next record.
- ENDFILE: after the on-unit returns, execution continues after the GET;
  K keeps its last value. Each later GET raises ENDFILE again (JOB00110:
  the on-unit ran for every pass of the loop).
- The on-unit is the one established last: a second ON ENDFILE replaces
  the first.

## Not measured yet

- RECORD files (READ, WRITE); ENVIRONMENT; LINESIZE and PAGESIZE options.
- ENDPAGE on-units; other conditions (CONVERSION, ZERODIVIDE, ...).
- GET and PUT with the STRING option; PUT DATA.
- An on-unit established in a calling procedure (PL1C/370 takes on-units
  of the same procedure only).
