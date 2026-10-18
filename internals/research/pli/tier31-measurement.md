# Tier 31 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probes: Q1DO
(JOB00445, test `t119do`) and Q2FILL (JOB00451, test `t121fill`).
With Tiers 0 to 30: 123 programs identical (JOB00491 to JOB00495).

## The iterative DO

GC28-8201-4, section J ("The DO Statement"), gives the expansion, and
PL/I (F) follows it:

```
DO v = s1, s2, ...;   where s is  e1 [TO e2] [BY e3] [WHILE(w)]
                                 (TO and BY in either order)
  per specification:  e2 and e3 evaluated once, then v = e1
  test:               (e3 >= 0) & (v > e2)  |  (e3 < 0) & (v < e2)  ends it
                      WHILE(w) false ends it -- this specification only
  after the body:     v = v + e3 (1 without BY), back to the test
```

Q1DO shows each part:

- `DO I = 0, 2, 5, 3`: a specification without TO and BY runs once,
  and v keeps its value (`I` is 3 after the loop).
- `1 TO 3, 7, 10 TO 6 BY -2, 20 BY 5 TO 30` gives 1 2 3 7 10 8 6 20 25
  30, then `I` is 35.
- `1 TO 10 WHILE(I*I < 20), 50 WHILE(I > 100), 60`: a false WHILE ends
  only its own specification. `50` is skipped and `60` runs, so `I`
  ends at 60.
- `1 BY 3 WHILE(N < 4)`: no TO; the loop ends by WHILE alone (`I` 13).
- BY and TO are evaluated once, at the start of their specification.
  `DO I = 1 TO N` with `N = 3` set to 10 in the body still makes three
  passes (Q1DO). `DO H = 10 TO -5 BY B` with `B = -3` set to 100 in the
  body still steps down by 3 (`t120do2`, identical to PL/I (F),
  JOB00449). `DO I = 5 TO 1 BY B` with `B = -1` ends with `I` at 0.
- `DO I = J TO J + 2, J * 10` with `J = 0` set in the body: the second
  specification evaluates `J * 10` when it starts, giving 0.
- `DO I = 5 TO 1` makes no pass; `I` is 5.
- FLOAT and FIXED DECIMAL control variables:
  - `DO X = 0 TO 1 BY 0.25` makes five passes, and `X` ends at 1.25.
  - `DO D = 1.5 TO 0 BY -0.75, 9.99` gives 1.50 0.75 0.00 9.99.
- `BY 0` with WHILE repeats until WHILE turns false.
- GO TO out of a specification list leaves the control variable as it
  is.

PL1C/370 keeps its old code for the simple form: one specification
with TO, a FIXED BINARY integer control variable, BY an integer constant
or none, and no WHILE. Every other DO follows the expansion
(`check.c prepare_iter`, `gen.c gen_iter`). TO and BY go into
temporaries of their own types; the comparisons and the increment are
PL/I expressions with PL/I's conversions; the body is generated once.
A frame word records which specification entered the body.

## PUT LIST: items at the end of a line

Q2FILL, on a PRINT file of 120 positions with tab positions 1, 25, 49,
73 and 97:

- An item starts at the first tab position **after** the end of the
  previous item. An item that ends exactly on a tab boundary therefore
  pushes the next one a whole tab further: after a 24-character tail
  in positions 1 to 24 the next item starts at 49, not 25.
- The positions run on past the end of the line. An item that ends in
  position 120 puts the next one at position 25 of the next line (A1,
  A2, A3, A6). After one that ends in position 119, the next starts at
  position 1 (A7, A8). The earlier rule, "an item that would start at
  121 starts a new line", is the same rule.
- An item longer than the rest of the line is never moved. It is
  written from its tab position and goes on at position 1 of the next
  line, as often as needed (A4: 120 characters from position 25; A5
  and `t120do2`: 144 and 180 characters).

## Not done

A subscripted control variable, a pseudovariable or a label as
control variable, and PUT LIST of long items on files that are not
PRINT and on STRING targets.
