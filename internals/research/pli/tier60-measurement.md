# Tier 60: COUNT, LINENO, DATE, TIME, ONLOC, ONCOUNT

Six more functions from Tier 59's comparison with Section G of
GC28-8201-2. Measured 2026-10-14 on MVSTK5-BLD with a probe and its input (JOB00960, and JOB00961 with
SIGNAL in place of a division by zero). The test `t192msc` is the probe,
identical in JOB00969.

## The rules

- **COUNT(file)** is the number of data items the last GET or PUT on the
  file transmitted:
  - `PUT LIST(1, 2, 3)` gives 3;
  - `PUT LIST(A)` of five elements gives 5;
  - `PUT EDIT('X', 7)` gives 2;
  - `PUT DATA(I, J)` gives 2;
  - `PUT SKIP` alone gives 0;
  - GET LIST of three and of five items gives 3 and 5.
- **LINENO(file)** is the line the next item of the PRINT file goes to:
  - 10 after nine lines;
  - 13 after `SKIP(3)`;
  - 1 after `PAGE`;
  - 2 after the next SKIP, still 2 without one and after `SKIP(0)`.
- **DATE** is six digits, yymmdd. **TIME** is nine digits, hhmmssttt.
- **ONLOC** names the entry point by which the block that raised the
  condition was entered:
  - `Q1MISC` in the main procedure;
  - `SUBP` in an internal procedure;
  - `ALT` in the same procedure entered at its `ALT: ENTRY`.
  - Out of an on-unit it is a null string.
- **ONCOUNT** is 0 in an on-unit and outside one.
- A division of FIXED DECIMAL by zero went on, after a normal return of
  the ZERODIVIDE on-unit, to `IHE806I DATA INTERRUPT` and ERROR (step
  code 2000, JOB00960). The test raises ZERODIVIDE by SIGNAL instead.
- **The argument of LENGTH:** PL/I (F) refuses `LENGTH(DATE)` and
  `LENGTH(ONLOC)`. An argument without parentheses is the entry name
  (IEM0807I, JOB00959).

## The time of day

A throwaway test printed DATE and TIME from both programs in one job.
The final run (JOB00967) gave `261014 084458760` from PL/I (F) and
`261014 084458900` from PL1C/370, a fraction of a second later.

The first attempt issued SVC 11 with R1 = 0. That is TIME TU, timer
units, and the TIME was not digits. The TIME macro in the MVS 3.8j
source sets R1 = 2 for DEC (`mvs38-ibmsrc/macros/maclib/TIME.asm`).

The test prints only the lengths, that every character is a digit, and
that the month is at most 12 and the hour at most 23.

## In PL1C/370

- **COUNT:** when a program uses COUNT, the code of each item adds 1 to
  a counter (`PL1ICNT`). Each GET or PUT keeps the count on its file
  when the next one starts. GET DATA counts the items it assigns.
- **LINENO** is the line of the last record printed plus the advance of
  the record being built: 1 after PAGE (`pl1_lineno`).
- **DATE and TIME** come from SVC 11 (`rt/pl1time.asm`, `rt/pl1date.c`).
  The date comes as 0CYYDDDF; the milliseconds are the hundredths
  followed by a zero.
- **ONLOC:** each ENTRY statement gets a procedure descriptor of its own,
  with its name, which the prologue stores when entered there. The run
  time keeps the descriptor of the block that raised the condition. The
  system-action messages name that entry point too (not measured for
  PL/I (F)).
- **ONCOUNT** is the constant 0, since PL1C/370 has no event I/O.

## The suite

All 194 programs are identical: JOB00971, JOB00972, JOB00976 and JOB00977.

## Not done

- **COUNT across separately compiled procedures:** a procedure counts
  its items only if it uses COUNT itself.
- **COUNT of a STRING option:** none (the option has no file).
- **ONLOC in a BEGIN block or an on-unit:** it names the procedure the
  block is written in, never a secondary entry of it (not measured).
- **DATAFIELD:** needs the NAME condition, which PL1C/370 does not have.
