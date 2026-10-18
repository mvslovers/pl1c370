# Tier 68: READ IGNORE, GET COPY, TRANSMIT, UNBUFFERED

Four I/O features of PL/I (F) that PL1C/370 lacked
(`ansi-comparison.md`). Measured 2026-10-14 on MVSTK5-BLD with the probe
its input and a DD for a file of fixed-length
records (JOB01081). The test `t207io` is the probe, identical in
JOB01083, condition code 2000 included.

## The rules

- **READ FILE(f) IGNORE(n)** reads n records and drops them:
  - after IGNORE(2), the next READ INTO gets REC3;
  - `READ FILE(f);` with no INTO and no SET is IGNORE(1), so REC5
    follows REC3;
  - IGNORE(0) skips nothing;
  - IGNORE past the last record raises ENDFILE, ONCODE 70, and the INTO
    target keeps its value.
- **GET ... COPY** writes each field, as it stands in the input, on a
  line of its own on SYSPRINT. The line being built is written out
  first, so a following `PUT SKIP` leaves an empty line:
  - `GET LIST(A, B) COPY` with input `12  345` gives the lines `12` and
    `345`;
  - `GET DATA(X) COPY` gives the field with its delimiter, `X=7;`;
  - `GET EDIT(C) (A(3)) COPY` gives the three characters read.
- **TRANSMIT(f):**
  - SIGNAL TRANSMIT gives ONCODE 40 in the on-unit, and ONFILE is the
    file.
  - Without an on-unit the message is `IHE122I FILE SYSIN - TRANSMIT
    CONDITION SIGNALED`, then ERROR (step code 2000).
- **UNBUFFERED** works for a file of fixed-length records. For REPORT,
  which is RECFM=VB, PL/I (F) ends at the OPEN with `IHE156I FILE REPORT
  - CONFLICTING ATTRIBUTE AND ENVIRONMENT PARAMETERS`. That was the
  probe's first run; its job output was not kept.

## In PL1C/370

- **READ IGNORE** (`pl1_read_ignore`) reads the records. A READ without
  INTO or SET takes IGNORE(1).
- **COPY** sets a flag for the GET running. The GET routines pass each
  field to `pl1i_copyline`:
  - LIST its characters with the quotes;
  - EDIT the field;
  - DATA the field with its delimiter.
- **TRANSMIT** is a file condition like NAME, its SIGNAL code 40. PL1C/370
  does not detect transmission errors, so only SIGNAL raises it.
- **UNBUFFERED** (and UNBUF) is accepted and does what BUFFERED does. That
  includes V-format files, where PL/I (F) refuses it. This deviation is
  listed in `docs/language-levels.md`.

## Not done

- **COPY(file):** ANSI's form, with a file other than SYSPRINT.
- **BACKWARDS and EXCLUSIVE:** a tape option and a multitasking one.

## The suite

All 209 programs are identical: JOB01085 to JOB01089.
