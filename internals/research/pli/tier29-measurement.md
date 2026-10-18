# Tier 29 rules, measured against PL/I (F) V5.5

Measured 2026-10-13 on MVSTK5-BLD. Probes:
Q2AFIT (JOB00416), Q1AREA (JOB00417), Q3ALAY (JOB00418), Q4AERR
(JOB00419), Q5AFRE (JOB00420). The layout and the allocation rules are
those GY28-6801-6 describes ("The Area Variable"), as these probes
measure them. `rt/pl1area.c` implements them; `tests/host/t_area.c`
replays Q5AFRE, Q2AFIT and Q3ALAY against it. Tests `t113area`,
`t114afre`, `t115aerr` (JOB00422); with Tiers 0 to 28: 117 programs
identical (JOB00424 to JOB00426).

## Declarations

- `AREA(n)`, 1000 bytes without `(n)`; at most 32767. Static, automatic,
  based and controlled areas all work. An area's storage is 16 bytes of
  control information, then the n bytes, doubleword aligned.
- `OFFSET(a)` needs `a` to be an unsubscripted level-1 **BASED** area.
  An automatic or static area there gets `IEM0606I OFFSET VARIABLE O1
  HAS BEEN DECLARED ... RELATIVE TO AN IDENTIFIER WHICH IS ...`
  (JOB00416, the first form of Q1AREA and Q3ALAY). Allocation `IN(a)`
  works with any area. To use offsets with an automatic area, base a
  second area on its address (`AP = ADDR(AA)`), as Q1AREA does.
- `NULLO` assigned to an offset whose declaration is invalid gets
  `IEM1797I CONVERSION OF NULL VALUES IN POINTER/OFFSET ASSIGNMENTS IS
  INVALID` and `IEM1850I`, and NULL takes its place.

## The control information

Read through a based array over the area (Q3ALAY, Q5AFRE):

| Word | Contents |
|---|---|
| +0 | flag byte (X'80': a free list exists), then the size, 3 bytes |
| +4 | the end of the extent, an offset from the start of the area; 0 while nothing was ever allocated |
| +8 | the offset of the largest free element |
| +12 | zero: a free element of length 0 that ends the list |

- Offsets count from the start of the area, control information
  included. The first allocation is at offset 16. `UNSPEC(O)` of an
  offset is its word: `'00000000000000000000000000011000'B` for the
  second element of 8 bytes (24); NULLO is X'FF000000'.
- An element is the based variable's length plus its offset from a
  doubleword, rounded up to a multiple of 8. There is no per-element
  header. In an AREA(200), allocations of 1 to 8 bytes fit 25 times,
  9 to 16 bytes 12 times, 17 to 24 bytes 8 times, 25 bytes 6 times and
  100 bytes once (Q2AFIT). The extent may reach the size plus 16.
- A free element holds its length and the offset of the next smaller
  free element. The last one points to +12.

## Allocation and freeing (IHELSPA, IHELSPB)

- **Allocate.** An empty area (extent 0) takes the element at 16.
  Otherwise, if there is a free list, it is searched from the largest
  element down. An element of exactly the size is taken whole. If none
  fits exactly, the smallest larger element is split: the allocation
  takes its start, and the rest is a free element sorted back in by
  size. If the largest free element is too small, the allocation comes
  from the end of the extent. If that does not fit either, AREA is
  raised.
- **Free.** If the element is the last of the extent, the extent ends
  before it. If a free element ended where the freed one began, the
  extent ends before that one too, and it leaves the list. If that free
  element started at 16, the area is empty again: flag 0, extent 0.
  Any other element becomes a free element. It is merged with a free
  element right after it and one right before it, then sorted in by
  size.
- **A quirk of FREE.** When the free element absorbed by the extent was
  the only one, the flag stays X'80' with the head at 12, though the
  list is empty (Q5AFRE, "FREE 4": `-2147483448 40 12 0`). PL1C/370
  does the same. The flag is visible through a based overlay and travels with
  area assignment.

Q5AFRE: six allocations (8, 16, 24, 8, 32 and 8 bytes) at offsets 16,
24, 40, 64, 72 and 104, extent 112. Then frees and reallocations:

| Step | Words +0, +4, +8 | What happened |
|---|---|---|
| FREE 2 | X'80'+200, 112, 24 | free list 24(16) |
| FREE 4 | same | 24(16), 64(8) |
| ALLOC 8 | same | exact fit at 64 |
| FREE 3 | same | merged with 24: 24(40) |
| ALLOC 12 | 112, 40 | split: 16 bytes at 24, rest 40(24) |
| FREE 6, FREE 5 | 104, then 72 | extent shrinks |
| FREE 4 | 40, 12 | extent shrinks past 40(24); flag stays |
| FREE 3, FREE 1 | 24, then 16 | |
| ALLOC 8 | 24, 12 | from the end (the list is empty) |

## Area assignment and EMPTY (IHELSPC)

- `B = A` copies A's extent, control words included. B keeps its own
  size and takes A's flag byte. If B's size plus 16 is less than A's
  extent, AREA is raised (ONCODE 361) and nothing is copied. On a
  normal return from the on-unit, execution goes on after the statement
  (Q1AREA: `ASSIGNED 1`).
- Offsets keep their meaning in the copy. Converted by B's base, they
  address the copies of the elements (Q1AREA: `COPY 1`, `NEXT 2`).
- `A = EMPTY` clears the flag and words +4 to +12. The next allocation
  is at 16 again (Q1AREA: `EMPTY '1'B`, `BEMPTY '1'B`).

## Offsets and pointers

- Pointer to offset: the pointer minus the address of the offset's area.
  Offset to pointer: the offset plus that address. The address is taken
  from the area's BASED pointer at the time of the conversion.
- Offsets are compared with offsets, and with pointers (by converting
  the offset), for = and ¬= only (Q1AREA: `CHAIN '1'B '1'B '1'B`).
- An offset in a structure member chains a list (Q1AREA: `ITEM 1`,
  `ITEM 2`).

## The AREA condition

| ONCODE | Raised by | Message without an on-unit, then ERROR |
|---|---|---|
| 360 | ALLOCATE | `IHE360I AREA CONDITION RAISED IN ALLOCATE STATEMENT AT OFFSET +nnnnn FROM ENTRY POINT name` |
| 361 | area assignment | `IHE361I AREA CONDITION RAISED IN ASSIGNMENT STATEMENT AT OFFSET ...` |
| 362 | SIGNAL AREA | `IHE362I AREA SIGNALED AT OFFSET ...` |

The ERROR on-unit sees the same ONCODE (Q4AERR).

After a normal return from an on-unit raised by ALLOCATE, the allocation
is attempted again. It goes into the area that the reference now
designates: an on-unit that sets `AP = BP` makes the allocation land in
B, at offset 16 (Q4AERR: `SWITCH 360`, `IN B AT 16`). An on-unit that
corrects nothing repeats: Q1AREA's counts six entries, then leaves by GO
TO.

## Not done

Areas as arguments and parameters (`AREA(*)`), arrays of areas and areas
in structures, areas in record I/O, and offset defining.
