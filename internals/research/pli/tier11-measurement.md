# Tier 11 rules, measured against PL/I (F) V5.5

Measured 2026-10-12 on MVSTK5-BLD. Probes (JOB00197),
and what the test programs showed against PL/I (F) (JOB00201 to
JOB00207). The test programs `tests/pli/t58putd` to `t61gdat` print and
write what PL/I (F) does (JOB00209, together with Tiers 0 to 10: 63
programs).

## PUT DATA

- Each item is `name=value`, placed as a LIST item (tab positions on a
  PRINT file, one blank after it on other files and strings), so an item
  that does not fit starts a new line as a whole.
- The value is the LIST field: numbers with their leading blanks
  (`I=            42`), FLOAT with its sign position (`F= 1.25000E+00`).
  Strings are quoted, quotes doubled, on PRINT files too (`S='AB''CD'`);
  bit strings `'1010'B`; VARYING its current value.
- Arrays element by element, `A(1)=`; structure members by their full
  name, `R.X=`, also when written as `Y`.
- A semicolon follows the last item. On a PRINT file it is written right
  after it; elsewhere it takes the place of the blank after the item, and
  no blank follows it (PUT STRING(L) DATA(I) into a VARYING L gives length
  17: `I=            17;`, JOB00201).
- PUT DATA without a list writes every variable of the block, in the
  order declared. PL1C/370 does this in the main procedure only, and
  leaves out implicitly declared variables (whether PL/I (F) lists them is
  not measured).

## GET DATA

- `name=value` items, separated by blanks or commas, up to a semicolon;
  variables not named keep their values; an element `A(2)=7`.
- **The name must be fully qualified**: `Y=` for `R.Y` is unrecognised
  (JOB00205). An unrecognised name is the NAME condition: `IHE100I FILE
  SYSIN - UNRECOGNISABLE DATA NAME`, and the item is skipped; on a STRING it
  is `IHE027I GET STRING - UNRECOGNISABLE DATA NAME`, then ERROR (step code
  2000; the ONCODE an ERROR on-unit would see is not measured).
- The end of the file after `name=` is `IHE038I FILE SYSIN - ENDFILE FOUND
  UNEXPECTEDLY IN MIDDLE OF DATA ITEM`, then ERROR (JOB00207; ONCODE not
  measured). An end
  before a name is ENDFILE.

## The STRING option

A string is a file of one record that is not PRINT: PUT STRING LIST puts a
blank after every item, the last too (`   12 'AB' `, length 11 in a
VARYING string), and strings are quoted; EDIT writes no blanks. A CHAR
target is blank-padded. GET STRING reads LIST and EDIT items from it.

## Not done

ON NAME (only its system action), GET DATA of arrays of more than one
dimension or of arrays of structures, PUT DATA without a list outside the
main procedure, the end of a STRING during GET (PL1C/370 raises ERROR, not
measured), STRINGSIZE (PUT STRING beyond the string is cut).
