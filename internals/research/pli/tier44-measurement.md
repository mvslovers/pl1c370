# Tier 44: A(*) parameters as a whole

Measured 2026-10-13 on MVSTK5-BLD: a probe (JOB00720). The test `t146awh` is the same program, and it is identical
(JOB00722).

## The rules

A whole A(*) parameter can be used as any array can:

- **An assignment** sets each element. `A = 0` in ZERO clears X(0:4),
  and `A(2) = 5` after it sets the third element.
- **An expression** works element by element: `A = A * 2 + 1`, `B = -B`
  for B(*, *).
- **Two A(*) parameters** combine the same way: `T = S + 100`.
- **PUT LIST(A)** lists the elements in row order. Y(2:3, -1:1) gives 19
  20 21 29 30 31.
- **An array of CHAR(*)** works as a whole too: `C = 'Z'` pads each
  element to its length.

## In PL1C/370

lower.c turns an aggregate into loops over its bounds.

- **The loop limits.** For an A(*) parameter `chain_bounds` returns a
  placeholder, and the loops get LBOUND and HBOUND of the parameter as
  their limits (`bound_expr`). gen.c loads those from the descriptor
  (Tier 41).
- **Comparing bounds.** Where an operand's bounds are known only at run
  time, the check that they match the target's is skipped: they are
  taken to be equal.

## Not done

- When two arrays of different bounds meet at run time, PL1C/370 does
  not raise the error PL/I (F) would. This is not measured.
- A(*) arrays of structures.
