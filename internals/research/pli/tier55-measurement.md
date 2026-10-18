# Tier 55: string parameters of external procedures by descriptor

Tier 54 left this open: Q1SUC compiled apart (`t176sus`) gave '0'B after
`CALL SETSUC('1'B)` (JOB00877). The caller knows SETSUC only by
`DCL SETSUC ENTRY(BIT(1))` and passed the argument's address. SETSUC's
parameter is `BIT(*)` and expects a descriptor.

Measured 2026-10-13 on MVSTK5-BLD. The tests are identical:

| Test | Job |
|---|---|
| `t176sus`: Q1SUC compiled apart | JOB00888, JOB00898 |
| `t177str`: seven string routines compiled apart | JOB00898 |
| `t178vst`: Q2VSTAR, VARYING arguments to internal CHAR(*) | JOB00900 |

`t177str` covers:

- CHAR(6), CHAR(12) VARYING, BIT(4), CHAR(*) and BIT(*) parameters;
- arguments of the declared length, shorter ones and constants, which
  get dummies;
- an expression (`C || L`);
- a function returning CHAR(6);
- a procedure passing its own string parameters on to two others;
- assignments to the parameters, seen by the caller.

## The convention in PL1C/370

- **A string parameter of an external procedure** goes by a descriptor,
  as a CHAR(*) parameter always did: A(the data) and its length, of a
  VARYING one A(the variable) and its maximum. This holds for CHAR(n),
  CHAR(n) VARYING and BIT(n), but not for arrays or for the main
  procedure.
- **The caller of an external procedure** builds the descriptor around
  the argument, or around its dummy, with the parameter's length. That
  is the length in its ENTRY declaration when the procedure is compiled
  apart.
- **A caller that knows no parameter** passes a string argument by its
  descriptor too, with the argument's own length. These are a
  contextual CALL (`t162ctx`), an ENTRY without descriptors, and an
  empty descriptor.
- **Internal procedures** are called as before: the caller sees the
  parameter.
- **The parameters of an ENTRY declaration** now belong to the
  procedure they describe, which the convention needs. `BIT(*)` in an
  ENTRY declaration is a star parameter, as `CHAR(*)` was; it
  could not be declared before. The full suite ran before that last
  change (JOB00893 to JOB00896, 179 programs identical). The change
  touches only `BIT(*)` in a descriptor, which no earlier test could
  use, and `t177str` and `t176sus` ran again after it (JOB00898).

## A VARYING argument to a CHAR(*) parameter

The convention matches PL/I (F) only where the result is defined.

- **An internal procedure** gets the current length (Q2VSTAR,
  JOB00889): `SHOWS(V)` with V = 'VARY' gives 'VARY', length 4. So does
  PL1C/370: the test `t178vst` is the probe, identical in JOB00900.
- **An external procedure declared `ENTRY(CHAR(*))`,** called with a
  CHAR(12) VARYING variable holding 'VARY+', gets that variable's
  maximum length: 'VARY+' and seven blanks, length 12 (`t177str`'s
  first version, JOB00888). The seven bytes are storage beyond the
  current length.

PL1C/370 passes the current length, and `t177str` leaves that call out.

## Not done

- Arrays of fixed-length strings to external procedures still go by
  address, as before.
- A VARYING argument to an external CHAR(*) parameter (above).
