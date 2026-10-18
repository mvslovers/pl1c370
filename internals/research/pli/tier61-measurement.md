# Tier 61: cross sections of arrays

A subscript written as `*` selects every element of its dimension:
`M(2,*)` is the second row of M, `M(*,3)` its third column. PL1C/370
refused them ("give all subscripts or none"), and Tier 59's comparison
listed them.

Measured 2026-10-14 on MVSTK5-BLD with the probes:
Q2XSIN in JOB00978, Q1XSEC in JOB00979, after dropping one statement
PL/I (F) refuses (below). The tests are identical in JOB00981:

| Test | Covers |
|---|---|
| `t193xsc` | Q1XSEC, and a procedure that changes the section passed to it |
| `t194xsi` | Q2XSIN: GET into sections, sections of structure members |

## The rules

- **A cross section** is an array of the dimensions given as `*`, in
  their order. Its elements go in row order: `K(1,*,*)` of K(0:1,2:3,2)
  is K(1,2,1), K(1,2,2), K(1,3,1), K(1,3,2).
- **It is used as an array is:**
  - assigned to or from: `V = M(3,*)`, `M(1,*) = 0`;
  - in expressions: `W = M(*,1) + M(*,4)`, `M(*,2) = M(*,2) * 2`;
  - the other subscripts may be expressions: `M(I,*) = V`;
  - in PUT LIST and EDIT, GET LIST and EDIT;
  - as the argument of SUM (`SUM(M(I + 1,*))`).
- **Of a structure array:** `S(*).Y` is the same as `S.Y`.
- **As an argument** to an `A(*)` parameter, the parameter takes the
  section's bounds:
  - for M(3,4), `M(2,*)` is A(1:4) and `M(*,4)` is A(1:3);
  - the procedure's assignments to it change the section.
- **PL/I (F) refuses `K(*,3,*)`,** a fixed subscript between two `*`s:
  IEM0881I, "NUMBER OF DIMENSIONS IN REFERENCE TO K IS NOT SAME AS THAT
  OF EXPRESSION OR ASSIGNMENT" (CC 12, JOB00978). `K(1,*,*)` and
  `K(*,*,2)` are accepted. PL1C/370 accepts all three.

## In PL1C/370

- **parse.c:** a `*` subscript is a node E_STAR.
- **lower.c:** the `*`s of a reference take the loop variables in order,
  where a whole array has them appended:
  - a cross section as target gets a loop for each `*`, the source taken
    by element;
  - a PUT or GET item gets the same loops around the element.
- **As an argument:** the section is copied into a temporary array of its
  shape before the statement, and back after it. That gives the
  procedure's changes to the caller, as the test shows. How PL/I (F)
  passes a section was not measured. If it passes the storage itself, the
  two differ where the procedure also reaches the array by another name.

## The suite

All 196 programs are identical: 195 in JOB00983 to JOB00986. In JOB00986 the
PL/I (F) compile step of `t66sqrt` ended with condition code 16, a
failure of the stand seen before. It was identical in JOB00988.

## Not done

- **A cross section of a structure** (`S(*)` with S a structure) as a
  target, an I/O item or an argument.
- **A cross section of an A(*) parameter** as an argument.
- **In a WHILE or UNTIL condition:** a cross section as an argument
  there.
- **The bound functions:** DIM, LBOUND and HBOUND of a cross section.
