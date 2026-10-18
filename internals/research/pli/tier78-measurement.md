# Tier 78: INITIAL with '*', expressions as factors, ANSI's values

Measured 2026-10-14 on MVSTK5-BLD with the probes:

| Probe | Job | Shows |
|---|---|---|
| Q1INIA | JOB01192 | `INIT(1, *, 3, *)`: the elements with `*` are left as they are |
| Q4INIX | JOB01192 | `INIT((2) *, (3) 9)` |
| Q2INIE | JOB01192 | `INIT((M * 2))` refused: IEM0139I, "TEXT IMMEDIATELY FOLLOWING ' * 2)' IN INITIAL ATTRIBUTE IS ILLEGAL"; `INIT(M)` refused: IEM0512I, M "IS NOT A KNOWN LABEL CONSTANT" |
| Q3INIS | JOB01192 | STATIC `INIT((2 * 3))` refused: IEM0139I |
| Q6INIG | JOB01199 | groups: `INIT((2)(3, 4), 5, 6)`, nested `INIT((2)(1, (2)(7, 8)), 9)`, STATIC `INIT((2)(1, 2))` |
| Q5INIF | JOB01194 | `INIT((M) 7, 8)` in AUTOMATIC storage: M, a parameter, counts at the block's entry; `INIT(-5)`, `INIT(+2.5)` |

PL1C/370 refused three things that PL/I (F) takes, all gaps against the
oracle:
- `*`;
- iteration factors that are expressions;
- groups.

The test `t219ini` is made of the probes' accepted parts, identical in
JOB01197, and `t221ing` is Q6INIG.

ANSI X3.53-1976 also takes parenthesized expressions and references as
INITIAL values (CM43 to CM45, 4.4.3.9). PL/I (F) refuses them, so they
are beyond the oracle. The test `t220ini` is a twin pair, identical in
JOB01197; its twin assigns the values.

## The rules

- **`*`** leaves the element as it is. In STATIC storage nothing is
  written there.
- **An iteration factor that is an expression** is evaluated when the
  block is entered, in `t219ini` with `INIT((M) 7, (4) 8)`:
  - M = 3 gives 7, 7, 7, 8;
  - M = 1 gives 7, 8, 8, 8;
  - M = 6 gives four 7s, and the 8s find no element;
  - M = 0 gives four 8s.
- **A group** `(n)(item, ...)` is its items n times, nested groups
  included. Q6INIG's B has eight elements: 1, 7, 8, 7, 8, 1, 7, 8, and
  the 9 finds none.
- **STATIC** takes constants only, for the values and for the factors
  (PL/I (F): IEM0139I). ANSI would take constant expressions there.
- **ANSI's values** `(M * 2)`, `M`, `('AB' || 'CD')` are evaluated
  when the block is entered, like any AUTOMATIC INITIAL.

## In PL1C/370

- **parse.c:**
  - `*` is a value of NULL;
  - `(expression)` before an item is an iteration factor (`repx`), and
    without an item after it a parenthesized value;
  - `(n)'..'` and `(n)'..'B` stay replicated strings;
  - a group `(n)(...)` is expanded in place (`init_list`).
- **lower.c** (`init_stmts`): with an expression factor, the elements
  are counted at run time, as `DO J = 1 TO f; IF K <= hb THEN DO;
  x(K) = v; K = K + 1; END; END;` per item.
- **gen.c** writes no DC for a `*` element in STATIC.
- **check.c** refuses a STATIC value that is not a constant.

## Not done

- **An expression iteration factor for an array of more than one
  dimension:** refused.
- **A group after an expression factor,** `(M)(1, 2)`: refused.

## The suite

All 223 programs are identical: JOB01202 to JOB01210. Before it, the
assembler of every earlier test was compared with main's compiler: only
the three new tests differ.
