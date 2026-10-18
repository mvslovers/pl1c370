# Tier 12 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probes with their
oracle output (JOB00210, JOB00211). The test programs `tests/pli/t62subr`
to `t65no` print what PL/I (F) prints and end with the same condition code
(JOB00222 and JOB00223, together with Tiers 0 to 11: 67 programs).

## Prefixes

- `(name, ...):` before a statement's labels applies to that statement;
  before a PROCEDURE or BEGIN, to the whole block and the blocks inside
  it. `P: (SIZE): PROC;` -- the prefix after the label -- is ignored by
  PL/I (F) with IEM0116I; it must come first: `(SIZE): P: PROC;`.
  PL1C/370 refuses the other order (`statement expected`).
- A prefix on DO or IF covers that statement, not the group or the THEN
  unit, nor a procedure the statement calls: `(NOCONVERSION): DO ...;
  J = C; END;` and `(NOCONVERSION): J = G(1);` with a conversion inside G
  both raise CONVERSION (JOB00220).
- SIZE, SUBSCRIPTRANGE (SUBRG) and STRINGRANGE (STRG) are disabled unless
  a prefix enables them; ZERODIVIDE, FIXEDOVERFLOW, CONVERSION and
  UNDERFLOW are enabled unless a NO prefix disables them. Disabled, nothing
  is raised and the result is undefined (NOUNDERFLOW: 0).

## SIZE (ONCODE 340)

A value assigned beyond the target's precision raises SIZE; after a
normal return of the on-unit the target keeps p digits (12345 into DEC(3)
is 345, into DEC(2) 45) or p bits (100000 into FIXED BIN(15) is 1696,
the low 15 bits). Without an on-unit: `IHE310I SIZE`, then ERROR (step
code 2000). 999 into DEC(3) and 12345/100 into DEC(3) raise nothing.

## SUBSCRIPTRANGE (ONCODE 520)

Each subscript is checked against its bounds (B(1,4) with B(2,3) raises
it). Without an on-unit: `IHE500I SUBSCRIPTRANGE`, then ERROR. A PUT SKIP
whose item raises it has made its SKIP already, as PL1C/370's order of
work does too. What a normal return of the on-unit does is not measured
(the probes leave it by GO TO); PL1C/370 raises ERROR then.

## STRINGRANGE (ONCODE 350)

SUBSTR(s,i,j) outside s raises it, and the result is the part inside s:
`SUBSTR('ABCDEF',5,4)` is `EF`, `(0,2)` is `A`, `(7)` and `(3,-1)` are
null. SUBSTR(s,i) raises it for i beyond the length (also i = length+1).
Without an on-unit: `IHE350I STRINGRANGE`, and the program goes on.

## Test runs

A job has at most 255 steps (IEF602I, the job not run): with three steps
per PL/I (F) program and one per PL1C/370 program, `tests/run_mvs.py`
now sends 50 programs per job.

## Not done

OVERFLOW (refused), CHECK, SIZE on conversions to FIXED from FLOAT beyond
what PL1FTOB returns, prefixes on format items; FIXED DECIMAL to FIXED
BINARY(31) beyond 2**31 still abends 0C9 (Tier 6), under SIZE too, where
PL/I (F) presumably raises SIZE. Not measured: SIGNAL of a disabled
condition, a prefix on an ON statement reaching its on-unit.
