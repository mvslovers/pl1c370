# Tier 56: iteration factors by expression, long external names, contextual functions

Items from the backlog of earlier tiers, measured 2026-10-13 on
MVSTK5-BLD with the probes (all three in
JOB00901):

| Probe | Test | Job |
|---|---|---|
| Q1FMT | `t179fmt` (its iteration factors, more of them) | JOB00905 |
| Q2LONG | `t180lng`; `t181lns`, the same compiled apart | JOB00903 |
| Q3UND | `t182und` | JOB00903 |

## An iteration factor given by an expression

`(N) F(4)` repeats the item N times, and `(N - 1) (F(3), X(1))` repeats
the group, as `(3) F(4)` does with a constant. The factor is evaluated
when the statement runs. `t179fmt` shows:

| Format list | Output |
|---|---|
| `((N) F(4), X(2), (N - 1) F(3), F(5))`, N = 3, data 1 to 6 | `   1   2   3    4  5    6` |
| `((N) A(3), A)`, N = 2 | `AB CD EF` |
| `((N) F(5), F(3), (N) X(9), F(3))`, N = 0, data 7 and 8 | `  7  8`: an item with a factor of 0 is skipped |
| `((N) F(5), F(3))`, N = -1 | the same for a negative factor |
| `((J) (F(2), (J) X(1)))` for J = 1 to 3 | factors nested inside each other |

**In PL1C/370:**

- The factor is assigned to a variable before the statement, as a width
  given by an expression is (Tier 46).
- The format list gets an item F_REP: the count, and the number of items
  it repeats.
- The run time keeps the repetitions that are running on a small stack
  (`next_data`): at most eight nested, which the compiler checks.
- A group that contains an R item counts its items after R is replaced
  by its FORMAT statement (not tested).

## External names of more than eight characters

PL/I (F) cuts an external name of more than seven characters to its
first four and last three characters, with IEM2867I, an error (CC 8).
It links and runs all the same. The linkage editor's map shows:

| Name | Section |
|---|---|
| COMPUTEVALUE | COMPLUE |
| SHOWRESULTS | SHOWLTS |
| TOTALCOUNTER | TOTATER |

**PL1C/370** cuts names of more than eight characters the same way, for:

- external procedures and their entries (entries not tested);
- EXTERNAL variables;
- the procedures called through `=V(name)`.

It used to refuse such names. A name of eight characters is kept, where
PL/I (F) would cut it to seven: within PL1C/370's own objects only the
names have to agree. Two names that come out the same would be one
name to the linkage editor, for PL/I (F) as for PL1C/370 (not
measured).

## A function reference to a procedure the caller does not declare

`R = TWICE(2.5E0)` with TWICE an external procedure not declared in the
main procedure: PL/I (F) passes the argument as it is and returns 5.0
(the default FLOAT DECIMAL(6) of the name's first letter).

PL1C/370 sees TWICE in the same source. It refused a FLOAT DECIMAL(2)
argument for its FLOAT DECIMAL(6) parameter unless declared. It now
passes an argument of a FLOAT of the same length (short or long) as it
is, which has the same form in storage. Other mismatches remain an
error.

## Not done

Measured in Q1FMT, not implemented: PL1C/370's run time stops with a
message (CC 16) for each.

- **E and F format for a character string:** PL/I (F) converts the
  string to a number first. `E(12,3)` of `'1.5E2'` gives `1.500E+02`,
  and `F(8,2)` of `' -42.5'` gives `-42.50`.
- **B format for an arithmetic value:** PL/I (F) converts the number to
  a bit string first, of the length its precision gives:
  - `B(8)` of 5 is `0101` (FIXED DEC(1), four bits);
  - `B(6)` of 10 is `000101` (seven bits, cut to six);
  - `B(4)` of -3 is `0011`;
  - `B(8)` of 2.75 is `0010`.

  The run time does not have the value's precision at hand.
