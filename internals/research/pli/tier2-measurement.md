# Tier 2 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probes
`P8STRUC.pli`, oracle output beside them (JOB00078). The test programs
`tests/pli/t12arr`, `t13struc` and `t14agg` print byte for byte what PL/I (F)
prints (JOB00087, together with Tiers 0 and 1).

## Output of aggregates

- PUT LIST of an array writes one item per element, rows first: `B(0:2,3)`
  gives B(0,1), B(0,2), B(0,3), B(1,1), ...
- PUT LIST of a structure writes its elementary items in order, arrays
  among them element by element: `S` gives X, Y.Z, Y.W, T(1), T(2), T(3).
- An array of structures goes **element by element**: `R(2)` with members
  P and Q gives R(1).P, R(1).Q, R(2).P, R(2).Q.
- A CHARACTER element is written at its declared length (`'ZZ'` in a
  CHAR(4) takes four positions).

## Bounds, subscripts, INITIAL

- Lower bounds other than 1 work, negative ones too: `C(-2:2)`;
  `LBOUND(C,1)` = -2. DIM, LBOUND and HBOUND are FIXED BINARY(15).
- Subscripts are expressions, nested ones included: `A(A(2) - 5) = 4`.
- **A reference takes all its subscripts or none.** A structure name takes
  none, or exactly those of its own level, and then names one element
  (`R(K)`). PL/I (F) refuses anything between, e.g. `R(1).T` for a
  `2 T(3)` in `1 R(2)`: IEM0258I "number of subscripts ... conflicts with
  dimensionality" (JOB00083). PL1C/370 refuses it the same way.
- Subscripts may be written at any level of a qualified name: `R(K).P` and
  `R.P(K)` are the same element.
- INITIAL lists fill elements in row order; iteration factors repeat a
  value: `INIT((2)7,(2)-1)`.

## Aggregate assignment

- A scalar assigned to an array sets every element (`A = 0`); to a
  structure, every member, converted to each member's type (`Z = 3`).
- Array expressions work element by element when the arrays have the same
  bounds: `D = D * 2 + 1`, `A = B * C + C`.
- A structure assigned to a structure of the same shape is assigned member
  by member, by position (`U = S`).

## Not measured yet

- Cross-sections with `*` subscripts; BY NAME assignment; LIKE.
- Structure mapping as PL/I (F) lays it out (alignment and padding):
  invisible to these programs, which never look at storage, but it matters
  for record I/O and DEFINED.
- SUBSCRIPTRANGE.
