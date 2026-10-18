# Tier 24 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probes (Q1ENDP,
JOB00368; Q2UNDF, JOB00371), which are also the tests
`tests/pli/t104endp` (JOB00370) and `t105undf` (JOB00373). With Tiers 0
to 23: 107 programs identical (JOB00375 to
JOB00377).

## ENDPAGE

- A PRINT file's page has 60 lines, or PAGESIZE(n) of the OPEN. ENDPAGE
  is raised when a SKIP would start the line after the last one.
- Without an on-unit (the system action) the record goes to a new page:
  with PAGESIZE(10), `'1OUT 10'` follows `' OUT 9'` (the page eject of
  OPEN is line 1).
- With an on-unit, a PUT PAGE in it first writes the line the SKIP had
  begun -- an empty record -- then starts the page; the interrupted
  statement then puts its item on the line after the on-unit's own:
  `' LINE 58'`, `''`, `'1HEADING 2'`, `' LINE 59'`. An on-unit that
  starts no page lets the output run on past the page.
- PUT PAGE at the very start of a file writes an empty `'1'` record
  first, as a SKIP does there (SIGNAL ENDPAGE before any output).
- ENDPAGE's ONCODE is 90.

## UNDEFINEDFILE and RECORD

- A file that cannot be opened raises UNDEFINEDFILE, ONCODE 84; ONFILE
  is its name. Without an on-unit: `IHE150I FILE NODD - CANNOT BE OPENED
  (INCORRECT OR NO DD CARD)`, then ERROR (JOB00361 too).
- READ INTO a variable smaller than the record raises RECORD, ONCODE 21;
  after a normal return the variable holds what fitted (`TWEN`). Without
  an on-unit: `IHE111I FILE R - RECORD VARIABLE SMALLER THAN RECORD
  SIZE`, then ERROR. A larger variable raises nothing and keeps the rest
  of its value (`TWENTY CHARACTERS.. ********************`).
- ONFILE is the name of the file of the last file condition.

## Not done

A normal return of an UNDEFINEDFILE on-unit (PL1C/370 then stops), the
KEY, TRANSMIT and NAME conditions in this form, ENDPAGE raised by an item
that does not fit the line.
