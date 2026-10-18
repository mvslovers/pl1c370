# Tier 65: the built-in functions of ANSI X3.53-1976

Every built-in function the standard has and PL/I (F) does not
(`ansi-comparison.md`):
- AFTER, BEFORE, DECAT, REVERSE, COPY, COLLATE;
- ASIN, ACOS, SUBTRACT, DOT, EVERY, SOME;
- OFFSET, POINTER (and PTR), ONFIELD;
- PAGENO and its pseudo-variable;
- VALID.

They are beyond the oracle. Each test is a pair:
- PL1C/370 compiles `<name>.opt.pli`, which uses the ANSI functions.
- PL/I (F) runs `<name>.pli`, its twin, which gets the same results with
  PL/I (F)'s means. AFTER becomes INDEX and SUBSTR, ASIN(x) becomes
  `ATAN(x, SQRT(1 - x*x))`, OFFSET(p, a) becomes the assignment of p to
  an offset of a.
- Where PL/I (F) has nothing to compute with (PAGENO, VALID, EVERY,
  SOME), the twin prints the values that follow from the definition in
  the standard.

Every function and its markings are listed in `docs/language-levels.md`,
with every deviation from the standard. The table of built-in functions
in `src/resolve.c` carries the level of each (`L_F`, `L_ANSI`).

| Test | Functions | Job |
|---|---|---|
| `t200ans` | AFTER, BEFORE, DECAT, REVERSE, COPY, COLLATE | JOB01047 |
| `t201ans` | ASIN, ACOS, SUBTRACT, DOT, EVERY, SOME | JOB01050 |
| `t202ans` | OFFSET, POINTER, ONFIELD, PAGENO, VALID | JOB01052 |

## The definitions taken (ANSI 9.4.4)

- **AFTER(s, c)** (9.4.4.5): what follows the leftmost c.
  - A null s gives a null string; a null c gives s.
  - A c that does not occur gives a null string.
  - `AFTER('ABCDEFCD', 'CD')` is `EFCD`.
- **BEFORE(s, c)** (9.4.4.11): what precedes the leftmost c.
  - A null c gives a null string.
  - A c that does not occur gives s.
- **DECAT(s, c, p)** (9.4.4.25): s is split into three parts, the part
  before the leftmost c, c, and the part after.
  - p, as BIT(3), selects the parts to concatenate.
  - With no c in s, s is the first part.
  - With a null c, s is the third part.
- **REVERSE(s)** (9.4.4.67).
- **COPY(s, n)** (9.4.4.20): n copies.
  - `COPY('12', 3)` is `121212`.
  - REPEAT('12', 3) has four copies (GC28-8201-2: "(i+1) occurrences").
- **COLLATE** (9.4.4.17): every symbol once, in the collating sequence.
  - The symbols are the 57 language characters and the extralingual
    characters, which the implementation defines (2.6).
  - PL1C/370 takes all 256 EBCDIC characters in the order of their
    codes: `SUBSTR(COLLATE, 194, 1)` is `A`, and `INDEX(COLLATE, 'Z')`
    is 234.
- **ASIN, ACOS** (9.4.4.7, 9.4.4.2): FLOAT of the argument's base and
  precision.
- **SUBTRACT(x, y, p [, q])** (9.4.4.77): the difference, at precision
  (p, q).
- **DOT(x, y [, p [, q]])** (9.4.4.29): the sum of products of two
  vectors. Without p it is FLOAT of the common base and the greater
  precision.
- **EVERY(x), SOME(x)** (9.4.4.33, 9.4.4.73): one bit.
  - EVERY is '1'B if every bit of every element, as a bit string, is 1;
    a null element counts as true.
  - SOME is '1'B if any bit is 1.
  - x may be an element.
  - `EVERY(B)` of `'1111'B, '1011'B, '1111'B` is `'0'B`; F's ALL of
    them is `'1011'B`.
- **OFFSET(p, a), POINTER(o, a)** (9.4.4.54, 9.4.4.63): the conversions
  between pointer and offset, through the area a.
- **ONFIELD** (9.4.4.57): the field of the NAME condition, DATAFIELD.
- **PAGENO(f)** (9.4.4.62) and **PAGENO(f) = n** (7.5.4.4): the page
  number of a PRINT file.
  - The page number is 1 when the file is opened, and one more at each
    new page (8.5.1.2 Execute-single-opening, 8.7.2.8
    Output-string-item).
  - `t202ans` shows 1, 2 after PAGE, 10 after `PAGENO(SYSPRINT) = 10`,
    11 after the next PAGE.
- **VALID(x)** (9.4.4.86): whether the characters of a picture variable
  are a valid value of its picture.

## In PL1C/370

- **rt/pl1ansi.c:** the string functions, COLLATE and VALID.
- **check.c** (`ansi_rewrite`) turns these into what PL/I (F) has:
  - ASIN, ACOS into ATAN and SQRT;
  - SUBTRACT into ADD;
  - OFFSET, POINTER into the casts the area converts by.
- **lower.c:** DOT, EVERY and SOME are loops over the elements, as SUM is
  (Tier 59).
- **PAGENO** is a page counter on the file (`rt/pl1io.c`). Like LINENO,
  it counts a PAGE already asked for.
- **VALID of a numeric picture:** the characters are read as the
  picture's value, then edited by the picture again; equal characters
  are valid.
  - A character picture checks each position: A a letter or a blank, 9
    a digit or a blank, X anything.
  - `'012'` is not valid for `ZZ9`, `' 12'` is.
- **Found on the way:**
  - A CHARACTER string DEFINED on a numeric picture was refused. PL/I
    (F) accepts it, as `t202ans`'s twin shows. It is now allowed.
  - Runtime calls take at most six arguments, so DECAT passes its
    pattern as BIT(3).

The deviations from the standard are in `docs/language-levels.md`:
- character strings only;
- COPY with a variable count at most 256 characters;
- ASIN and ACOS outside -1 to 1;
- DOT's precision;
- PAGENO's first page;
- VALID's method.

## Also found, not done

- **The UNSPEC pseudo-variable** is refused, though PL/I (F) has it
  (`t200ans`'s twin uses it). A gap against the oracle.
- **A function result as a pointer qualifier** (`POINTER(o, a) -> X`)
  is refused.

## The suite

All 204 programs are identical: JOB01054 to JOB01058.
