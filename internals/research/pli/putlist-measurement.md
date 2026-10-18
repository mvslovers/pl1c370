# PUT LIST as PL/I (F) V5.5 writes it

Measured 2026-10-12 on MVSTK5-BLD: JOB00054 (SYSPRINT to the spool) and
JOB00055 (SYSPRINT to the data set `HERC01.PL1C.PRINT`). The probe is
`tests/host/P3LIST.pli`, the oracle output with control characters
`tests/host/P3LIST.oracle`. This is the specification `rt/pl1rt.c`
implements; `tests/host/t_putlist.c` replays the probe against the run time
and reproduces the oracle byte for byte.

## How the output was captured

mvsMF returns spool files as text lines in every mode (`mode=binary`,
`mode=record`, `X-IBM-Data-Type: binary|record` all give the same 1,365
bytes) and drops the ASA control byte, so SKIP(2), SKIP(0) and PAGE are
invisible in the spool: 31 records, no blank lines. Read as binary, a data
set comes back as the records concatenated with no record descriptor words,
which makes the boundaries ambiguous. **Read as text, a data set keeps the
control byte as the first character of each line.** So the GO step's
SYSPRINT goes to a data set (`submit.py --print-ds`), and the data set is
read as text.

PL/I (F) opens such a new SYSPRINT data set as `RECFM=VA`, `LRECL=125`,
`BLKSIZE=129`.

## The rules

| Rule | Evidence (oracle line) |
|---|---|
| The file starts on a new page: the first record carries `1`. | 0 |
| A line holds 120 print positions. | 26–28 |
| Items start at tab positions 1, 25, 49, 73, 97. | 0–3 |
| An item starts at the first tab position that is not before the end of the previous item and lies after its start; an empty string still takes a tab position. | 2 (`VE=` empty, `END` at 97), 3 (30-character string at 25, next at 73) |
| An item that does not fit on the rest of the line starts a new line. | 26→27 (the sixth item would start at 121) |
| Strings are written without quotes; `''` in a constant is one quote. | 23 (`IT'S`) |
| Arithmetic items are right-justified in a field of p+3 characters. | 14–23 |
| FIXED BINARY(p) counts as FIXED DECIMAL(1+CEIL(p/3.32)): BIN(15) takes 9 positions, BIN(31) 14. | 4–13 |
| A fraction has a digit before the point (`-0.05`, `-0.125`, `0.00`). | 18, 19, 21 |
| Constants carry their own precision: `42` is DEC(2,0), `-3` DEC(1,0), `1.50` DEC(3,2). | 23 |
| SKIP writes the current line, even an empty one; the next record carries ` ` (SKIP), `0` (SKIP(2)), `+` (SKIP(0)). PAGE gives `1`. | 24, 25, 29 |
| SKIP(3) gives `-`; SKIP(4) writes an empty record with `-` and gives the next one ` ` (JOB00067, `tests/pli/t6misc.pli`). | — |

## Not measured yet

- SKIP(n) for n > 4: the run time assumes the pattern of SKIP(4) (empty
  records with `-` for the lines beyond three).
- An item longer than the rest of the line, or longer than 120.
- FLOAT, BIT, COMPLEX, PICTURE items; PUT EDIT and PUT DATA.
- Negative FIXED BINARY of the most negative value (-32768, -2147483648).
- Trailing blanks: the oracle lines are compared with trailing blanks
  removed; whether PL/I (F) writes them is not visible in a text read.
