# Tier 81: adjustable bounds, multiple assignment

Two PL/I (F) features PL1C/370 refused. RXPI (`~/repos/mvs/foo`,
TODO.md item 1) uses both:
- `DECLARE A(LEN) FIXED BINARY(31);` in a BEGIN block;
- `NINES, PREDIGIT = 0;`.

Measured 2026-10-14 on MVSTK5-BLD
(JOB01248). The test `t225adj` is the probe, identical in JOB01251.

## The rules

- **Bounds from expressions** belong to an AUTOMATIC array. They are
  evaluated when the block is entered, and the array keeps them:
  - `N = 9;` inside the BEGIN block leaves `X(N)` at 5 elements;
  - `Y(M:N, 2)` has the bounds 2:5 and 1:2;
  - `C(N) CHAR(3)` is an array of strings;
  - `Z(K + 1)` in a procedure takes its parameter.
  - DIM, LBOUND, HBOUND, whole-array assignment, subscripts and SUM work
    on them.
- **`A, B = e`** evaluates e once. With A = 1, `A, B = A + 1;` gives 2
  and 2, and so does `B, A = A + 1;`.

## In PL1C/370

- **parse.c:**
  - a bound may be an expression (`bound_value`); `M REFER(N)` and
    numbers are as before;
  - an array with such a bound is `adjust`, with every bound kept as an
    expression (`lbx`, `hbx`).
- **The array is reached like an `A(*)` parameter,** through a
  descriptor: A(data), the element length, then lb and hb per dimension.
  - The descriptor is the array's storage in the frame, placed among
    the first variables, so it is reachable without a base register.
  - `star_desc` and the subscripting of gen.c take it from there.
- **lower.c** puts an `S_ADJUST` statement at the start of the block,
  before the INITIAL assignments, in declaration order.
- **gen.c** generates it:
  - it computes the bounds and the element count (no elements when a
    bound pair is reversed);
  - it takes the data from the stack past the frame. 76(13) is the next
    free byte PDPPRLG keeps; it is advanced by the size, rounded to a
    doubleword.
  - The caller's frame keeps its own 76, so the storage is given back
    whenever the block ends, by its END, by RETURN, or by a GO TO out of
    it.
- **`A, B = e`** (`multi_assign`, lower.c) becomes single assignments
  with e evaluated once:
  - e into each target, when e reads no target's storage and calls no
    function;
  - otherwise e into the first target and the first into the others.
    That is exact when the targets have one set of attributes, and is
    refused otherwise.

## Not done

- **Adjustable bounds** in STATIC, BASED or CONTROLLED storage, in
  structures, with INITIAL, and adjustable string lengths `CHAR(N)`:
  refused.
- **PUT DATA and GET DATA of an adjustable array:** not tested.
- **The stack's end is not checked.** An array larger than what is left
  of the stack (libc370's @@CRT0: about 256 KB unless `@@STKLEN` sets
  it; PL1C/370 does not) overwrites what follows.
- **A multiple assignment** to targets of different attributes whose
  expression reads one of them: refused.

## The suite

All 227 programs are identical: JOB01254 to JOB01262, and JOB01265.
- In JOB01256 PL/I (F)'s compiler itself (IEMAA, step O75) ended with
  ABEND U3865, and the job's remaining 25 programs were not run. This is
  the stand effect seen before.
- JOB01265 ran those 25 again, all identical.
