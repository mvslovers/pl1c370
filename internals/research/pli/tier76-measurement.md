# Tier 76: locator qualifiers, BASED without a pointer

ANSI X3.53-1976 lets any reference that yields a pointer qualify a
based variable. It also lets BASED stand without a pointer, with every
reference then qualified. PL/I (F) refuses all of it, as a probe showed (JOB01090, `ansi-comparison.md`):
- `GETP -> X`, where GETP is a function returning a pointer, gives
  IEM0646I;
- `ADDR(A) -> X` gives IEM0030I;
- `DCL X BASED;` gives IEM0154I.

Beyond the oracle. The test `t217loc` is a twin pair, identical in
JOB01165. Its twin assigns each qualifier to a pointer variable first.

## The rules taken

- **The qualifier** is any reference of type POINTER:
  - a pointer variable or an element of a pointer array, `PV(2) -> V`;
  - `ADDR(A) -> X`, also as a target: `ADDR(S) -> C = 'WXYZ'`;
  - a function reference, `GETP -> X`;
  - a based pointer, as in a chain `Q -> NXT -> V`.
- **A BASED variable without a pointer** needs a qualifier in every
  reference. ALLOCATE needs SET, and FREE a qualified reference.
- **An offset as the qualifier** is not taken: it must be converted to
  a pointer first.

## In PL1C/370

- **parse.c** reads a chain of qualifiers, and BASED without
  parentheses.
- **check.c** (`check_based`):
  - the qualifier must be a pointer, of any kind of reference;
  - an unqualified reference to a BASED variable without a pointer is
    refused.
- **gen.c** already computed the pointer of any expression (`gen_ptr`).

## The suite

All 219 programs are identical: JOB01167 to JOB01171.
