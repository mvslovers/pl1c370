# Tier 35: factored declarations, CHAR(*) parameters, END label

Three gaps the earlier tiers had worked around, each checked against
PL/I (F) V5.5 on MVSTK5-BLD: `t129fac`, `t130star` (JOB00575) and
`t131end` (JOB00577), identical. With all earlier tiers: 133 programs
identical (JOB00580 to JOB00584).

## Factored declarations

GC28-8201-4 (DECLARE) allows attributes inside a factored list, in
nested lists, and with factored level numbers:

    DCL (A FIXED BIN(31), B FLOAT DEC(6), C CHAR(5)) STATIC;
    DCL ((D, E) FIXED DEC(5,2), F FIXED BIN(15)) INIT(0);
    DCL 1 S, 2 (X, Y) FIXED BIN(15), 2 (Z1 CHAR(2), Z2 CHAR(3));
    DCL ((K1 INIT(11), K2 INIT(22)) FIXED BIN(15),
        K3 CHAR(3) INIT('K3')) AUTOMATIC;

PL1C/370 writes such a DECLARE out before parsing it
(`parse.c unfactor_declare`). Each name gets its own attributes, then
those of every list around it, and the level number around it if it
has none of its own. `DCL ((A, B) FIXED, C FLOAT) STATIC` becomes
`DCL A FIXED STATIC, B FIXED STATIC, C FLOAT STATIC`. A list of names
only, `(A, B) FIXED`, is left to `declare()`, as before.

## CHAR(*) parameters

`DCL S CHAR(*)` and `CHAR(*) VARYING` take their length from the
argument. Every call reaches an internal procedure whose parameters the
compiler knows, so the caller passes such an argument through a
descriptor of two words: A(the data) and its length. For a VARYING
parameter that is A(the variable) and its maximum. `gen_place` reaches
the data through it, and `star_len` the length, for the value of the
parameter, for an assignment to it and for LENGTH. A variable is passed
by reference, so the callee's assignment changes it (`FILL(A, 'XY')`
leaves A as `'XY   '`). Constants and expressions are passed as their
values. `GROW(W)` with W declared `CHAR(8) VARYING` stops at its
maximum (`'AB+MORE+'`).

The declared length of such a parameter is a bound: the longest
argument written at any call. That is a variable's declared length, a
constant's length, or 256 for an expression. The bound sizes the
temporaries of expressions with the parameter. It is found before any
body is checked (`check.c star_pass`), and a longer argument is
refused.

## END label

`END X` closes the nearest unclosed group or block whose DO, BEGIN or
PROCEDURE statement has the label X, and every one inside it
(GC28-8201-4, END). The parser keeps a stack of the open groups with
their labels. An `END X` for an outer group ends the inner ones without
being read, until the group labelled X reads it. `END P` of a
procedure closes the groups still open inside it.

## Not done

CHAR(*) arrays, BIT(*), CHAR(*) in ENTRY declarations used for
anything but documentation, CHAR(*) arguments that are not character
strings, and END with a label that names no open group: that END is
read as a plain END.
