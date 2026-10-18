# PL/I (F), ANSI X3.53-1976 and PL1C/370

PL1C/370 follows PL/I (F) first and ANSI X3.53-1976 second (ROADMAP,
"Compatibility order"). This note says where the two differ, what
PL1C/370 does in each case, and which gaps it found in PL1C/370's PL/I
(F) coverage. Written 2026-10-14, after Tier 64.

## Sources and method

- **ANSI X3.53-1976 (R1998).** The maintainer's copy is licensed to one
  user, so this note cites its rules and sections and quotes nothing. The
  inventory comes from:
  - the concrete syntax, rules CM1 to CM159 of chapter 2 (statements,
    attributes, conditions, options, formats);
  - the list of built-in functions, 9.4.4.1 to 9.4.4.87;
  - the pseudo-variables, 7.5.4.
- **PL/I (F):** GC28-8201-2 (October 1969), through its OCR text.
  "Not in F" below means that the term does not occur in that text, or
  that the manual's own lists (Section G overview, Section C) do not name
  it. The OCR is not complete, so such a statement is strong evidence,
  not proof. Where it matters, PL/I (F) itself decides: a probe on
  MVSTK5-BLD.
- **PL1C/370:** the built-in table in `src/resolve.c`, the condition
  table and the statement parser in `src/parse.c`, and the test suite.

## 1. Built-in functions

### In ANSI, not in PL/I (F)

| Function (ANSI 9.4.4) | Purpose | PL1C/370 |
|---|---|---|
| ACOS, ASIN | inverse cosine and sine | no |
| AFTER, BEFORE | the substring after or before the first occurrence of another | no |
| COLLATE | the collating sequence as a string | no |
| COPY | n copies of a string (F: REPEAT, with a different count) | no (REPEAT yes) |
| DECAT | three arguments, a string result (9.4.4.25; not examined further) | no |
| DOT | inner product of two vectors | no |
| EVERY, SOME | one bit: every bit of every element is 1, or some bit is (F's ALL and ANY instead give the bitwise & and \| of the elements) | no (ALL, ANY yes) |
| OFFSET(p, a), POINTER(o, a) | conversion between offset and pointer | no (by assignment only, as in F) |
| ONFIELD | the field of a NAME condition (F: DATAFIELD) | no (DATAFIELD yes) |
| PAGENO, PAGENO pseudo-variable | the page number of a PRINT file | no |
| REVERSE | the string reversed | no |
| SUBTRACT | subtraction at a given precision (as ADD) | no |
| VALID | whether a PICTURE variable holds a valid value | no |

These are beyond the oracle: a PL1C/370 version would be tested against
the ANSI definition. COPY and REPEAT differ in their count: COPY(s, n)
gives n copies (9.4.4.20), REPEAT(s, n) gives n + 1 (GC28-8201-2: "(i+1)
occurrences"). A program in one dialect
misreads in the other.

### In PL/I (F), not in ANSI

ALL, ANY, POLY, REPEAT, DATAFIELD, COUNT, ONCOUNT, NULLO, and the
multitasking functions COMPLETION, PRIORITY and STATUS. All but the
multitasking functions are in PL1C/370 (Tiers 59, 60, 62). Multitasking is
planned (ROADMAP, "Once out of scope, now planned").

### In both

ABS, ADD, ADDR, ALLOCATION, ATAN, ATAND, ATANH, BINARY, BIT, BOOL, CEIL,
CHAR, COMPLEX, CONJG, COS, COSD, COSH, DATE, DECIMAL, DIM, DIVIDE,
EMPTY, ERF, ERFC, EXP, FIXED, FLOAT, FLOOR, HBOUND, HIGH, IMAG, INDEX,
LBOUND, LENGTH, LINENO, LOG, LOG10, LOG2, LOW, MAX, MIN, MOD, MULTIPLY,
NULL, ONCHAR, ONCODE, ONFILE, ONKEY, ONLOC, ONSOURCE, PRECISION, PROD,
REAL, ROUND, SIGN, SIN, SIND, SINH, SQRT, STRING, SUBSTR, SUM, TAN,
TAND, TANH, TIME, TRANSLATE, TRUNC, UNSPEC, VERIFY.

PL1C/370 has all of them except:
- COMPLEX, CONJG, IMAG and REAL: COMPLEX is planned (ROADMAP).
- **STRING, function and pseudo-variable.** PL/I (F) has both:
  - GC28-8201-2 has the function in Section G;
  - the Version 5 additions name the pseudo-variable;
  - the oracle measured the pseudo-variable (JOB00053, `v55-measurement.md`).

  PL1C/370 refuses `STRING(S)`: "neither an array nor a known function".
  **A gap against the oracle.**

## 2. Conditions

| Condition | ANSI (CM9, CM77, CM79) | PL/I (F) | PL1C/370 |
|---|---|---|---|
| AREA, CONVERSION, ENDFILE, ENDPAGE, ERROR, FINISH, FIXEDOVERFLOW, KEY, NAME, OVERFLOW, RECORD, SIZE, STRINGRANGE, SUBSCRIPTRANGE, UNDEFINEDFILE, UNDERFLOW, ZERODIVIDE, CONDITION(name) | yes | yes | yes |
| **TRANSMIT** | yes | yes | **no: a gap against the oracle** |
| CHECK | no | yes | yes (Tier 64) |
| PENDING | no | yes | not yet (teleprocessing, planned) |
| STORAGE | yes | no | yes (Tier 70) |
| STRINGSIZE | yes, a computational condition | no | yes (Tier 70) |

TRANSMIT is raised by an uncorrectable transmission error. It cannot be
provoked on demand on MVS, but SIGNAL TRANSMIT(f), ON and REVERT can be
measured, and an I/O error then has somewhere to go.

**The condition prefixes** of ANSI (CM8 to CM10) are the computational
conditions and their NO forms, STRINGSIZE included. PL/I (F) adds
CHECK(name-list) and NOCHECK. PL1C/370 has F's set without NOCHECK.

## 3. Statements and options

| Item | ANSI | PL/I (F) | PL1C/370 |
|---|---|---|---|
| DEFAULT statement (CM47) | yes | no | yes, beyond the oracle (Tier 34) |
| DO ... REPEAT (CM69) | yes | no | yes, beyond the oracle (Tier 33) |
| SELECT, LEAVE, DO UNTIL | no | no | yes, beyond the oracle (Optimizer, Tier 33) |
| RECURSIVE (CM57) | yes | yes | accepted |
| `%INCLUDE` | yes (2.5.7) | yes, with the whole preprocessor | yes (Tiers 19, 27, 28) |
| the other % statements | no | yes | yes |
| DISPLAY, DELAY, EXIT, WAIT, UNLOCK | no | yes | no (DISPLAY not measurable in batch; the others multitasking or teleprocessing) |
| ON ... SNAP | yes | yes | yes |
| **READ ... IGNORE(n)** (CM103) | yes | yes | **no: a gap against the oracle** |
| **GET ... COPY** (CM112) | yes | yes | **no: a gap against the oracle** |
| OPEN ... TAB(...) (CM92) | yes | no | no |
| TAB format item (CM147) | yes | no | no |
| LINE option and format | yes | yes | yes |
| BY NAME | yes | yes | yes (Tier 50) |
| `GO TO` and `GOTO` | yes | yes | yes |

## 4. Attributes

| Item | ANSI (CM23, CM24) | PL/I (F) | PL1C/370 |
|---|---|---|---|
| **GENERIC** (CM36 to CM41) | yes | yes | **no: a gap against the oracle** |
| CONSTANT, VARIABLE, PARAMETER, LOCAL, MEMBER, STRUCTURE, NONVARYING, DIMENSION (as a keyword) | yes | no | no |
| REFER (CM22) | in any extent expression | one per structure, in its last element | as F (Tier 63) |
| AREA(*), the area size | yes | yes | yes |
| BUFFERED | no | yes | taken and ignored |
| **UNBUFFERED, BACKWARDS, EXCLUSIVE** | no | yes | **no**: refused; UNBUFFERED could be taken as BUFFERED is |
| TASK, EVENT, TRANSIENT | no | yes | not yet (multitasking, teleprocessing: planned) |
| the ENVIRONMENT options | open (CM33) | OS-specific | F's options PL1C/370 needs for its files |

ANSI's CONSTANT, VARIABLE and PARAMETER only spell out what F implies:
the attributes of a label or entry constant, a variable and a parameter.
A program that writes them is beyond the oracle and rare.

## 5. Defaults and semantics

Settled in the earlier research (`report.md`): where the two differ in
semantics, PL1C/370 does what PL/I (F) does. These are the measured
cases:
- **The default precision and base by first letter:** F's I to N rule;
  ANSI says FIXED BINARY.
- **Promotion of aggregates.**
- **The implicit call of a function without parameters.**
- **COPY and REPEAT** (above).
- **SUM and PROD** of FIXED elements: F returns FLOAT (Tier 59). This was
  not checked against ANSI 9.4.4.65 and 9.4.4.78.

## Locator qualifiers

ANSI allows any reference as the locator qualifier of a based variable.
PL/I (F) does not:
- `F->X`, where the function F returns a pointer, is refused
  (IEM0646I, "IDENTIFIER 'GETP' PRECEDING POINTER QUALIFIER ... IS NOT A
  NON-BASED POINTER VARIABLE");
- `ADDR(A)->X` is refused too (IEM0030I);
- `DCL X BASED;` without a pointer is refused (IEM0154I).

All three come from the probe `t69/Q1PQ`, JOB01090. They are no gap
against the oracle. PL1C/370 refuses them as well.

## What this means for the backlog

Gaps against the oracle, to be measured and built, most useful first:

1. **The STRING function and pseudo-variable.**
2. **GENERIC.**
3. **READ ... IGNORE(n).**
4. **GET ... COPY.**
5. **The TRANSMIT condition:** ON, SIGNAL, REVERT.
6. **UNBUFFERED** (and BACKWARDS, EXCLUSIVE if a program needs them).

Beyond the oracle, only on demand: the ANSI-only built-in functions, the
conditions STORAGE and STRINGSIZE, TAB, and the attributes ANSI adds.
