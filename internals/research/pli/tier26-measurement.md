# Tier 26 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probe (JOB00387); the tests `tests/pli/t107ent` (JOB00389) and `t108ent2`
(JOB00391). With Tiers 0 to 25: 110 programs identical (JOB00393 to
JOB00395).

## Secondary entry points

- `name: ENTRY [(parameters)] [RETURNS(attributes)];` in a procedure is
  a further entry point: CALL name(...) or name(...) as a function starts
  the procedure there, with that entry's parameters; flowing into the
  ENTRY statement from above does nothing.
- Entries share the procedure's variables (a STATIC counter kept by
  STORE, BUMP, RESET and SHOW), and one parameter may stand at different
  positions in different entries (`P3(A, B, T)`, `Q2: ENTRY(T, C)`,
  `R1: ENTRY(B)`).
- The caller's ENTRY declaration decides the conversion of the
  arguments, as for a procedure: without `DCL STORE ENTRY(FIXED BIN(31))`
  the constant 3 went in as FIXED DECIMAL and STORE read 1007339516 (the
  probe; the tests declare it).

PL1C/370's caller puts the entry number in R0 (0 for the PROCEDURE
statement); the prologue copies the arguments into a canonical list in
the frame, every parameter of every entry at a fixed place, and after
the prologue goes on at the ENTRY statement.

## Not done

Entries with RETURNS other than the procedure's, an ENTRY statement in
the main procedure, a secondary entry passed as an argument.
