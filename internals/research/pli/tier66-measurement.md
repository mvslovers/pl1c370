# Tier 66: STRING, and the STRING and UNSPEC pseudo-variables

PL/I (F) has STRING(x) as a function and a pseudo-variable, and UNSPEC
as a pseudo-variable. PL1C/370 refused all three:
- `ansi-comparison.md` found STRING;
- `t200ans`'s twin needed UNSPEC as a target.

Measured 2026-10-14 on MVSTK5-BLD with the probes
(Q1STR and Q2UNS, both in JOB01059). The tests `t203str` and `t204uns`
are the probes, identical in JOB01061.

## The rules

- **STRING(x)** is the elements of x in one string, in storage order:
  - `ABCDE` for a structure of CHAR(2) 'AB' and CHAR(3) 'CDE';
  - `XYZPQ` for an array member and a scalar member;
  - `R1R2R3` for an array.
  - For a structure of BIT(3) '101'B and BIT(5) '11000'B, it is the
    bits as they lie packed: `'10111000'B`.
  - For a scalar it is the scalar.
- **STRING(x) = v** makes the value as long as STRING(x), padded with
  blanks (or zero bits) or cut, and it then fills the elements in order:
  - `STRING(S) = 'QR'` gives A = 'QR' and B = three blanks;
  - `STRING(T) = '1234567'` gives X = 1, 2, 3 and Y = '45', the 67 cut.
- **UNSPEC(x) = b** stores the bits b into x's storage, padded on the
  right with zero bits or cut:
  - `UNSPEC(J) = '1'B` for FIXED BIN(31) J is -2147483648;
  - 19 one bits into FIXED BIN(15) give -1;
  - `UNSPEC(D) = '0000000100101100'B` for FIXED DEC(3) gives 12
    (X'012C');
  - `UNSPEC(B) = '1010'B` for BIT(8) gives `'10100000'B`.

## In PL1C/370

- **STRING(x) of an aggregate** is a view of its storage: the address
  ADDR gives, and the length of all its elements. That is possible
  because character elements have no padding between them.
- **STRING of bit strings** needs them packed together without gaps;
  the layout's bit count says so (`tbits`).
- **STRING(x) = v** assigns to that view as to a string. Bits go
  through PL1BPUT.
- **UNSPEC(x) = b** stores b through PL1BPUT into x's bytes: all of them,
  or the bits of a bit string.

## Not done

- **STRING of a cross section,** or of a member of an array of
  structures: the elements do not lie together.
- **STRING of bit strings that are not packed together,** or of mixed
  types.
- **UNSPEC as a target for a VARYING string or an aggregate.**

## The suite

All 206 programs are identical: JOB01063 to JOB01067, and JOB01069 for
`t135loc`, whose PL/I (F) compile step ended with condition code 16 in
JOB01063, as seen before on this stand.
