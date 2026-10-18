# Tier 41: BIT(*) and A(*) parameters, arrays of CHAR(*)

Measured 2026-10-13 on MVSTK5-BLD with probes (JOB00664) and `Q2STAR` (JOB00665). The tests `t141str` (from Q1STAR)
and `t142ast` (from Q2STAR) are identical (JOB00672).

## BIT(*)

`DCL S BIT(*)` takes its length from the argument, as CHAR(*) does:

| Argument | Length |
|---|---|
| a BIT(8) variable | 8 |
| a BIT(40) variable | 40 |
| a constant `'1'B` | 1 |
| an expression `B8 & '00001111'B` | 8 |

A variable is passed by reference, so `S = '1'B` in the procedure leaves
`'10000000'B` in the caller's BIT(8).

A BIT(12) VARYING variable holding `'101'B`, passed to BIT(*), came back
as `'100000000000'B` with length 12. PL/I (F) passed it as twelve bits
and changed its current length as well. PL1C/370 passes its current
value (three bits), and the test does not do this.

## Arrays with * bounds

`DCL A(*) FIXED BIN(31)` and `DCL B(*, *) FIXED DEC(5)` take their bounds
from the argument:

- LBOUND, HBOUND and DIM give them: X(0:4) is 0, 4 and 5; Y(2:3, -1:1)
  is 2, 3, -1, 1 and DIM 3.
- The elements are the argument's. `A(LBOUND(A, 1)) = 99` changes X(0),
  so the second sum is 199.
- An element outside the bounds raises SUBSCRIPTRANGE where it is
  enabled: `IHE500I SUBSCRIPTRANGE` in procedure BAD, then ERROR.
- A parameter passes the array on to another A(*) parameter.

## Arrays of CHAR(*)

- **`A(*) CHAR(*)` and `A(3) CHAR(*)`** take the element length of the
  argument: CA CHAR(4) gives 4 for each element.
- **Assigning to an element** pads or cuts to that length. 'NEW VALUE'
  becomes 'NEW ', and `A(1) || A(2)` becomes 'AAAA'.
- **`A(2) CHAR(*) VARYING`** gives each element's current length (XY:
  2, LONGER: 6).

## Arguments that PL/I (F) mishandles

- **FIXED BINARY(31) to CHAR(*):** PL/I (F) passes a null string.
- **FLOAT DECIMAL to CHAR(*):** the procedure ends with `IHE803I
  PROTECTION VIOLATION`.
- **In PL1C/370:** CHAR(*) takes character strings only, and BIT(*) bit
  strings only.

Q2STAR's first form passed the constant 2 to a FIXED BIN(15) parameter
and got nonsense (2.81600E+03). PL/I (F) passes a constant as a dummy of
its own type (the CLAUDE.md rule). The test passes a variable.

## In PL1C/370

For an argument to an A(*) parameter, or to an array of CHAR(*), the
caller builds a descriptor:

- A(the array)
- F(the element length: the declared length of a CHAR element, else the
  element's size)
- F(lb) and F(hb) for each dimension

A parameter of that kind passes its own descriptor on. The first two
words are those of the CHAR(*) descriptor, so `star_len` serves both.

The procedure works out an element's offset at run time (Horner over the
extents, `gen.c star_subscripts`), with the SUBSCRIPTRANGE check against
the descriptor. LBOUND, HBOUND and DIM of an A(*) parameter are loaded
from it; lower.c folds all others as before.

BIT(*) goes through the bit-string code in storage: the length is in the
descriptor, and an assignment pads or cuts to it.

## Not done

- An A(*) array as a whole in an expression, in an assignment or in PUT,
  which lower.c would turn into loops over its bounds.
- Arrays of BIT(*): PL/I (F) packs BIT arrays bit by bit, which is not
  measured here.
- Arrays of structures with * bounds, and array arguments that are
  members of structures.
