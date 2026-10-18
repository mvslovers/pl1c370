# Tier 99: factored names in a structure

Issue #267, a gap against PL/I (F): `2 (A, B) FIXED BIN(31)`, names
factored at a level below 1 (GC28-8201-4, Section I, "Factoring of
Attributes": "DECLARE 1 A, 2(B,C,D) (3,2) BINARY FIXED (15)", the
dimension factored as well). PL1C/370 refused it ("factored names in a
structure are not supported yet"); a factored list with more than names
in it, `(2 A, 2 B) FIXED`, was already written out by `unfactor_declare`.

## In PL1C/370

parse.c takes the names side by side, each a member with the
attributes, INITIAL included. Factored names with members of their own
below them (`2 (A, B), 3 C`) are refused, not supported.

## Test

`t255fac`: factored members at levels 2 and 3, with INITIAL and with a
factored dimension, in an array of structures, PUT LIST and PUT DATA of the structure, assignment to a
factored member. Identical in JOB02159; the full suite 255 of 256 in
JOB02138 to JOB02156, the one left `t255fac` itself, edited during
that run (PL/I (F) compiled the new source, PL1C/370 the old).
