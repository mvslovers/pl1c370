# Tier 50: BY NAME assignment, the BOOL built-in

Found with a third program of the kind real ones are (`t165rl3`: a
matrix times a vector, recursive FACT and FIB, BY NAME, BOOL, a
`$$$,$$9V.99CR` picture, `DO ... BY -3`, GET LIST from SYSIN).

Measured 2026-10-13 on MVSTK5-BLD with the probes:

| Probe | Job |
|---|---|
| Q4BYN | JOB00814 |
| Q3REAL | JOB00814 |
| Q5BYN | JOB00815 |

The tests are identical (JOB00817):

- `t163byn`: Q5BYN
- `t164by2`: Q4BYN without its last BY NAME expression
- `t165rl3`: Q3REAL

## BY NAME

`target = expression, BY NAME;` assigns the elementary items whose names
the target and every structure operand of the expression have in
common, at every level of the structures. Items that only some of them
have are left as they were.

- **`B = A, BY NAME`** (Q4BYN): B.Y, B.S.Q and B.X are assigned. B.W and
  B.S.R keep 99 and 5.
- **`A = B, BY NAME`** assigns A.X, A.Y and A.S.Q. A.S.P and A.Z keep
  123 and 1.5.
- **Arrays of structures** (`D = C, BY NAME`) are assigned element by
  element.
- **Expressions** (Q5BYN):
  - `B = A + B, BY NAME` gives B.Y = 105, B.S.Q = 10, B.X = 8; B.W and
    B.S.R are unchanged.
  - `B = A * 2, BY NAME` gives Y, Q and X doubled.
- **A name at another level does not match.** `C = A, BY NAME` with
  C.Q at level 2 and A.S.Q at level 3 assigns only C.X.
- **Q4BYN's `B = A + B, BY NAME` with CHARACTER members** raised
  CONVERSION, IHE604I: the expression is evaluated member by member as
  written.

PL1C/370: lower.c expands a BY NAME assignment like a structure
assignment, member by member of the target. For each member it takes
the expression with each structure operand replaced by its member of
that name (`named_member`), and skips the member where one of them has
none, or a structure where the target has an element.

## BOOL

`BOOL(x, y, z)` works bit by bit. z's four bits are the results for the
pairs (x, y) = 00, 01, 10, 11:

| Call | Result |
|---|---|
| `BOOL('1100'B, '1010'B, '0110')` (exclusive or) | `'0110'B` |
| `BOOL('1100'B, '1010'B, '0001')` (and) | `'1000'B` |
| `BOOL('1100'B, '1010'B, '0111')` (or) | `'1110'B` |
| `BOOL('1100'B, '10'B, '1001')` | `'1011'B`: the shorter operand padded with zeros |
| `BOOL('11001010'B, '10101100'B, '1110')` | `'01110111'B` |
| `BOOL('1'B, '0'B, '1101'B)` | `'0'B` |
| `BOOL('101'B, '011'B, '1000')` | `'000'B`; the length of BOOL with a one-bit and a three-bit operand is 3 |

A character string as z, as in `'0110'`, is converted to bits.

PL1C/370: the arguments are converted to bit strings as BIT() does. The
result is the longer operand's length. `PL1BBOL` in `rt/pl1rt.c`
computes it; the two operand lengths go in one word, since a call of
the run time takes at most six arguments.

## Not done

BY NAME with a structure operand that is a function result or has a
pointer qualifier.
