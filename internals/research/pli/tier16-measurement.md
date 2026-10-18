# Tier 16 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probes (Q1LIKE,
Q2DEF; JOB00274 and the jobs before it). The test programs
`tests/pli/t84like` to `t86def2` print what PL/I (F) prints, with Tiers 0
to 15: 88 programs identical (JOB00285, JOB00286).

## LIKE

`DCL 1 B LIKE A` gives B the members of A, with their names, attributes
and dimensions; B's own dimensions and storage class are B's (`DCL 1 C(2)
LIKE A`, `DCL 1 E STATIC LIKE A`), and a member may be LIKE a
substructure (`2 Q LIKE A.Z`). PUT DATA writes the copied names
(`C.Z.U(1)=`), structure assignment works member by member.

PL/I (F) refuses (IEM0596I, "likened to an item which is not a valid
structure") a LIKE whose structure is itself declared LIKE (`E LIKE B`
with `B LIKE A`) or contains a LIKE member (`E LIKE D` with `2 Q LIKE
A.Z` in D). PL1C/370 refuses both, and a LIKE of members with INITIAL
(not measured).

The copy is made when names are resolved (`resolve.c`).

## DEFINED

- A CHARACTER or BIT scalar defined on a CHARACTER or BIT scalar shares
  the base's bytes from POSITION(n) on (1 by default): `T CHAR(4) DEFINED
  S`, `U CHAR(3) DEFINED S POSITION(5)`, `W CHAR(1) DEF S POS(10)`;
  assigning to them changes S (`WXYZ123HI*`). A BIT string keeps the
  base's bits after it: B all ones, then `C BIT(4) DEFINED B
  POSITION(9) = '0101'B` gives `'1111111101011111'B`. The base may be a
  structure member or STATIC.
- An array defined on an array with the same attributes: without
  subscripts on the base, element (i,j) is the base's (i,j), whatever the
  bounds (`K2(2,2) DEFINED M` with M(4,4)); with iSUB, element
  (i1,...,in) is the base reference with each kSUB replaced by ik:
  `M(1SUB,1SUB)` is the diagonal, `M(2SUB,1SUB)` the transpose, `M(2,1SUB)`
  a row, `M(2*1SUB,1SUB+1)` an expression.
- PL/I (F) refuses a DEFINED item in a data-directed list (IEM0692I) and
  calls a scalar defined on a subscripted base a "deferred feature"
  (IEM0609I, `M23 DEFINED M(2,3)`, JOB00281). PL1C/370 refuses both.

PL1C/370 makes an overlay a member of its base, without storage of its
own, so the generated code addresses it as any member; the BIT store
merges the base's bits after a string whose length is not a whole number
of bytes. Every other DEFINED item is rewritten after lowering: each
element reference becomes the base element (`lower.c`). Not supported:
a BIT string defined from a position inside a byte, a whole DEFINED array
as an argument, and DEFINED on a structure or on another DEFINED item.
