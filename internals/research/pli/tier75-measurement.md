# Tier 75: ENTRY and FILE variables

ANSI X3.53-1976 has ENTRY and FILE variables: `DCL E ENTRY(...)
VARIABLE`, `DCL F FILE VARIABLE` (CM24). PL/I (F) has ENTRY and FILE
parameters only, and Tier 74 left the two refused.

- **Beyond the oracle:** the test `t215evr` is a twin pair, identical in
  JOB01151. Its twin makes the same calls and writes to the same files
  directly.
- **Found on the way, against the oracle:** a call through an ENTRY
  parameter declared with descriptors passed its arguments as they
  were. PL/I (F) converts them to the descriptors, as a probe shows (JOB01152): through `ENTRY(FIXED
  BIN(15))`, P(2.6) is P(2) and gives 4. The test `t216eds` is the
  probe.

## The rules taken

- **An ENTRY variable** holds an entry value. That is the procedure and
  the frame of the block it is in, eight bytes, as an ENTRY
  parameter's value.
  - `E = TWICE;` assigns the procedure, not a call of it.
  - `E(5)` and `CALL P;` invoke it.
  - `E2 = E;` copies the value (not tested).
- **A FILE variable** holds a file value, the address of the file's
  descriptor:
  - `F = REPORT;` assigns it;
  - `PUT FILE(F)` writes to REPORT;
  - F can be passed for a FILE parameter.
- **The descriptors of an ENTRY variable or parameter** decide the
  arguments' attributes: an argument that differs is passed as a dummy
  of the descriptor's attributes (Q1EDSC). Without descriptors the
  arguments go as they are.

## In PL1C/370

- **parse.c:** VARIABLE with ENTRY or FILE marks the name (`var_ef`). An
  ENTRY variable has the type of an ENTRY parameter, TY_ENTRY of eight
  bytes.
- **resolve.c:** a reference to an ENTRY variable is its value; with
  arguments, or in CALL, it is a call through it.
- **check.c:**
  - an assignment to an ENTRY variable takes a procedure's name as the
    procedure;
  - a call through a variable or parameter converts its arguments to
    the descriptors.
- **gen.c:**
  - both variables get storage like any data;
  - `file_desc` loads a FILE variable's value;
  - assigning to a FILE variable stores the descriptor's address;
  - an external procedure's entry value has no frame, so word 4 is 0.

## Not done

- **ENTRY and FILE variables** in structures or arrays, or STATIC,
  BASED or CONTROLLED: refused.
- **The entry value of a secondary entry point** or of a procedure
  compiled apart.
- **Comparing entry or file values** (ANSI allows `=` and `¬=`).
- **The lifetime of an entry value** is not checked: a call through a
  variable whose block has ended reaches a stale frame.

## The suite

All 218 programs are identical: JOB01159 to JOB01163. A first run ended
with a timeout of the HTTP connection while polling JOB01157; the rerun
went through.
