# Tier 101: structure arguments to procedures compiled apart

Issue #273, reported from a learning project (hellocp): a structure as
an argument to an external procedure compiled apart was refused both
ways -- a parameter descriptor with level numbers, `ENTRY(CHAR(*)
VARYING, 1, 2 FIXED BIN(15), 2 BIT(9))`, stopped the parse ("give the
attributes here"), and a structure argument to an ENTRY without
descriptors was "structure CMD cannot be used here". GC28-8201-4 (the
ENTRY attribute) and SC26-3114-01 ("Dummy Arguments") have both: a
structure descriptor is the level-1 number followed by its members'.

## In PL1C/370

- **parse.c:** a descriptor that is a level number followed by a
  comma or the closing parenthesis starts a structure descriptor; its
  members' descriptors (higher level numbers) are read and not kept.
- **check.c:** a structure (or an element of an array of structures)
  as the argument to an entry compiled apart, with a structure
  descriptor or without descriptors, is passed as it is, by its
  address, as PL1C/370 passes a structure to any procedure. Its
  members are not checked against the descriptor's.

## Test

`t258sar` (`.sep`: the main procedure and two procedures compiled
apart): a structure through a structure descriptor next to a CHAR(*)
VARYING one, then a structure and an element of an array of structures
without descriptors; the callees change members the caller then
prints. Identical in JOB02194; the full suite 259 of 259 in JOB02197
to JOB02215.
