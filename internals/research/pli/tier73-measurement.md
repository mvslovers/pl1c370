# Tier 73: GET ... COPY(file)

PL/I (F) has GET ... COPY, which writes the fields read to SYSPRINT
(Tier 68). ANSI X3.53-1976 lets the option name the file: `COPY
[(reference)]` (CM112). Without the reference it is COPY(SYSPRINT), as
the standard completes it (4.3.1.1).

Beyond the oracle. The test `t213cpy` is a twin pair, identical in
JOB01133 with its REPORT data set:
- PL1C/370 runs `GET LIST(A, B) COPY(REPORT)`, a GET EDIT and a GET
  DATA with COPY(REPORT), then a COPY without a file;
- the twin reads the same input and writes the lines with PUT
  FILE(REPORT) EDIT. It keeps the COPY without a file, which PL/I (F)
  has.

## The rules taken

- **The file named** receives the fields; it is opened as STREAM
  OUTPUT if it is not open (8.7.1.2, Step 6).
- **What a field looks like** is PL/I (F)'s. That is one line for each
  field, as it stands in the input:
  - `12` and `345` for GET LIST;
  - `XYZ` for A(3);
  - `D=9;` for GET DATA.
- ANSI (8.7.1.7) copies the input character by character instead, and
  starts a new line at each record's end. That would put `12  345` on
  one line. PL/I (F) decides, and the deviation is in
  `docs/language-levels.md`.

## In PL1C/370

- **parse.c:** COPY takes a reference in parentheses.
- **check.c:** that reference must be a file that can take output.
- **gen.c:** PL1GCPY receives the file's descriptor, or 0 for SYSPRINT.
- **rt/pl1io.c:** `pl1i_copyline` writes to that file.

## Not done

- **COPY in GET STRING**, which ANSI allows: accepted, not tested.

## The suite

All 215 programs are identical: JOB01135 to JOB01139.
