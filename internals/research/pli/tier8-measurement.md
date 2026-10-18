# Tier 8 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probes with their
oracle output (JOB00171, JOB00172, JOB00179). The test programs `tests/pli/t48beg`
to `t52list` print what PL/I (F) prints, end with the same condition code
and write the same records (JOB00181, together with Tiers 0 to 7: 54
programs).

## BEGIN blocks

- A BEGIN block has its own declarations: an inner `I` hides the outer
  one, an outer variable not redeclared is the same storage (an assignment
  inside is seen after the block).
- GO TO out of a nested block to a label of the procedure leaves both
  blocks.
- An ON statement in a block establishes for the block only: ZERODIVIDE
  inside the block takes the block's on-unit, after it the procedure's.

PL1C/370 makes a BEGIN block an internal procedure of its block, called
where it stands, as it does for an on-unit. RETURN out of a BEGIN block
(which ends the procedure) is not supported yet.

## STOP and EXIT

Both raise FINISH (ONCODE 4) and end the program; **the step ends with
condition code 1000** (JOB00171), also when STOP is executed in a called
procedure.

## BASED and POINTER

- PL/I (F) needs the pointer of BASED: `DCL B CHAR(3) BASED;` is a severe
  error (IEM0154I), and so is a function as a pointer qualifier,
  `ADDR(C) -> B` (IEM0030I, IEM0655I).
- A based structure is mapped as any structure; ALLOCATE without SET sets
  the declared pointer; P -> X and the implicit pointer give the same
  storage; ADDR of an array, a structure or a scalar; a BASED array over a
  parameter pointer; whole-array assignment to a BASED array.
- `WRITE FROM(x)` with x a member of a structure is a severe error
  (IEM0699I, JOB00174): FROM and INTO take a level-1 name. PL1C/370
  refuses it too.

NULL is X'FF000000' (UNSPEC of NULL, JOB00179), in PL1C/370 too, so a
record that holds a NULL pointer is the same. ALLOCATE takes zeroed
storage from LIBC/370's calloc, which is GETMAIN storage behind an 8-byte
header and so on a doubleword (`libc370 src/mvs/storage/heapsp.c`); a
structure is placed at its phase from there.

## Not done

The entry point name of a message for a condition raised inside a BEGIN
block (PL1C/370 names the procedure the block is written in, as for an
on-unit). RETURN out of a BEGIN block, INITIAL on BASED variables, a pointer
qualifier on an array or structure assigned or transmitted as a whole,
pointer qualifiers in a chain (`P -> Q -> X`), AREA, OFFSET, CONTROLLED,
and the STORAGE condition (an ALLOCATE that fails ends the program).
