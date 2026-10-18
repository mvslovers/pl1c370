# Tier 36: areas in aggregates, as parameters, in record I/O

Measured 2026-10-13 on MVSTK5-BLD: a probe (JOB00585), which is also the test `t132are2`. With it `t133arec`
(JOB00589) and the twin `t134dfta` (JOB00591) are identical, as are the
earlier `t113area` and `t114afre` (JOB00587).

## Areas in structures and arrays

- An AREA member of a structure (`2 A AREA(100)`) and every element of
  an array of areas (`AA(3) AREA(40)`) are initialized on allocation:
  the size in the first word, then no extent and no free list (Q1AREA2:
  `S.A 100 0 0 0`, `AA 1..3 40 0`).
- ALLOCATE ... IN(S.A) and IN(AA(2)) allocate in that member or
  element, and area assignment works between elements
  (`AA(3) = AA(2)`).
- PL1C/370 finds every area in a variable (`gen.c collect_areas`) and
  initializes each one at the start of the block (AUTOMATIC), with a DC
  (STATIC), or after the ALLOCATE (BASED, CONTROLLED). At most 256 areas
  per variable.

## Area parameters

- `AREA(*)` takes its size from the argument, and so does `AREA(n)`
  with an argument of the same size. Both are passed by reference:
  allocations in the procedure land in the caller's area. After
  `USE(S.A, 1)` the extent of S.A is 40 instead of 24, and after
  `USE2(B)` B's is 56.
- A size is read from the area's own control word, so `AREA(*)` needs
  no descriptor. An argument of another size than an `AREA(n)`
  parameter would need a dummy area (GC28-8201-4) and is refused.
- ENTRY descriptors take `AREA`, `AREA(n)` and `AREA(*)`.

## Record I/O and the rest

- `WRITE FROM(A)` writes the area with its 16 bytes of control
  information (size + 16 bytes), and `READ INTO(B)` brings it back. The
  offsets stay valid in B (`t133arec`; the REPORT bytes are identical).
- `UNSPEC` of an offset is its word.
- `DEFAULT ... VALUE(AREA(n))` gives n to areas declared without a
  size (`t134dfta`, a twin as in Tier 34; 25 allocations of 8 bytes fit
  in the AREA(200) it gives).

## Not done

An argument of another size for an AREA(n) parameter (the dummy area),
and more than 256 areas in one variable.
