# Tier 103: STORAGE, CURRENTSTORAGE, CONNECTED

Issue #223, block C. GC33-0009-4 and SC26-3114-01: STORAGE(x) (STG)
and CURRENTSTORAGE(x) (CSTG), FIXED BINARY(31,0), the bytes of a
variable; CSTG of a varying string counts its prefix and current
length, of an area its control bytes and current extent, of an
aggregate the maximum lengths. CONNECTED (CONN), a parameter's
attribute: it refers to connected storage, else a dummy in connected
storage is made. None of them is PL/I (F)'s: OPT.

## In PL1C/370

- **STORAGE, CSTG** (gen.c): a constant from the mapping -- an
  element's type size, an array's or structure's total as laid out
  (the padding after the last element of an array of structures left
  out); CSTG of a VARYING string 2 + its current length (bytes for BIT
  VARYING), of an area its current extent (rt/pl1area.c: the end of
  what was ever allocated, 16 while nothing was).
- Refused, "not supported yet": packed bit strings, adjustable or
  REFER bounds, `*` parameters, members of arrays of structures
  without their subscripts, and the exception for a structure whose
  last element is an area.
- **CONNECTED** is taken in a DECLARE and in a parameter descriptor and
  has no effect: PL1C/370 passes a cross section as a copy anyway.

## Test

`t260stg` (`.opt.pli`). PL/I (F) has neither function, so its twin
holds the values written out by hand from PL/I (F)'s mapping of the
items (GC28-6594-7) and its area layout (Tier 29): the comparison
checks PL1C/370 against that derivation, not against a value PL/I (F)
computed. Identical in JOB02250; the full suite 261 of 261 in JOB02253
to JOB02271.
