# Language levels of PL1C/370

PL1C/370 (pl1c370) compiles the language of IBM PL/I for MVS & VM,
measured on IBM PL/I (F), Version 5:
- **By default**, the language of PL/I for MVS & VM as its manuals define
  it (SC26-3114-01, Language Reference; SC26-3113-01, Programming Guide;
  Release 1.1, 1995), without what exists only in Language Environment
  (the CEE services, LE's message file) and without DBCS (GRAPHIC,
  MIXED). What the Optimizing Compiler had without LE stays in. Some extensions come from the standard
  ANSI X3.53-1976 (ISO 6160).
- **With `-lang=F`**, exactly the language of PL/I (F). Anything beyond
  it is an error, as PL/I (F) refuses it.

A program PL/I (F) accepts gives the same output and the same condition
codes as with PL/I (F), in both modes, except where the later IBM rules
differ (see "Deviations").

This page lists what does not come from PL/I (F), and every point where
PL1C/370 deliberately or knowingly behaves otherwise. It is the source
for the reference manual.

Levels, as they are marked here and in the compiler's table of built-in
functions (`enum level`):

| Mark | Level |
|---|---|
| **F** | IBM PL/I (F) Version 5 (GC28-8201-4): checked against PL/I (F) itself |
| **OPT** | IBM OS PL/I Optimizing Compiler (GC33-0009-4, Release 3, 1976): not in PL/I (F); PL/I for MVS & VM has it too |
| **MVS** | IBM PL/I for MVS & VM (SC26-3114-01, Release 1.1, 1995): in neither PL/I (F) nor GC33-0009-4. Some came with OS PL/I Version 2, whose manuals are not online, some with PL/I for MVS & VM itself (its "What Is New") |
| **ISO** | ANSI X3.53-1976 (ISO 6160) only: in none of the IBM compilers named above |

`-lang=F` refuses everything marked OPT, MVS or ISO. The message names
the level: "... is not PL/I (F) but the Optimizing Compiler's (-lang=F)".
Each OPT mark was checked against the GC33-0009-4 keyword list (Part II,
Section C) and the rules of the statement or attribute, each MVS mark
against SC26-3114-01.

## Beyond PL/I (F)

### Statements and options

| Item | Level | Since | Notes |
|---|---|---|---|
| Preprocessor: INDEX, LENGTH, COUNTER, COMPILETIME, PARMSET; omitted arguments; `%ACTIVATE x NORESCAN`/`RESCAN`; `%DECLARE f BUILTIN` | OPT | Tier 104 | GC33-0009-4, chapter 16; PL/I (F)'s preprocessor has SUBSTR only (IEM4130I) and no RESCAN option (IEM4265I) |
| SELECT, WHEN, OTHERWISE | OPT | Tier 33 | |
| LEAVE | OPT | Tier 33 | |
| DO ... UNTIL(e), DO ... REPEAT(e) | OPT | Tier 33 | |
| DEFAULT statement | OPT | Tier 34 | |
| REDUCIBLE, IRREDUCIBLE on a PROCEDURE statement | OPT | Tier 92 | SC33-0009-2, PROCEDURE statement; PL/I (F) takes them in an entry declaration only (IEM0182I on the PROCEDURE statement, JOB01832). No effect in PL1C/370 |
| %PROCESS | MVS | Tier 86 | the same as `*PROCESS`; the `%` in the first data position (column 2) |
| LANGLVL(SPROG) (`-langlvl=SPROG`, or LANGLVL in `%PROCESS`/`*PROCESS`) | MVS | Tier 88 | pointer arithmetic: `P + n`, `P - n`, `n + P` (n BIT or REAL FIXED (p,0)), `P2 - P1` (FIXED BIN(31)), pointers compared by `<`, `<=`, `>`, `>=`; and POINTERADD, POINTERVALUE, BINARYVALUE (SC26-3113-01, LANGLVL). The default is LANGLVL(OS,NOSPROG), as IBM's. `(P + 16) -> X` is not a locator qualifier: POINTERADD is |
| A null argument list, `F()`: a function reference with no arguments | OPT | Tier 100 | GC33-0009-4 ("If the entry expression argument appears with a null argument list, it is taken to be a function reference with no arguments"); PL/I (F) refuses it (IEM0031I, Q1EMP) |
| An internal function's result: the RETURNS of its PROCEDURE, with no declaration in the invoking block | OPT | Tier 100 | SC26-3114-01, RETURNS attribute: the default applies to an external entry constant or an entry variable only. PL/I (F) takes the invoking block's default (FLOAT DECIMAL, or FIXED BINARY(15) for I to N) |
| OPTIONS(MAIN NOEXECOPS) | MVS | Tier 82 | R1 to the main procedure unchanged, for more than one parameter or one that is not CHARACTER VARYING (SC26-3114-01, pp. 103 and 122): one CHARACTER VARYING parameter gets the PARM as it is, `/` included; other parameters get the list R1 points at, without descriptors. Not in GC33-0009-4 nor SC33-0006-7 (1985), whose form is the entry point PLICALLA. `-plist` gives the same in both modes, as PL/I (F)'s `ENTRY IHESAPB`/`IHESAPD` |

### Built-in functions

| Function | Level | Since | Notes |
|---|---|---|---|
| STORAGE (STG), CURRENTSTORAGE (CSTG) | OPT | Tier 103 | the bytes as mapped; CSTG of a VARYING string its prefix and current length, of an area its current extent. Not for packed bit strings, adjustable or REFER bounds, `*` parameters, members of arrays of structures without subscripts |
| ACOS(x), ASIN(x) | OPT | Tier 65 | see the deviations |
| OFFSET(p, a), POINTER(o, a), PTR(o, a) | OPT | Tier 65 | |
| SYSNULL, SYSNULL() | MVS | Tier 86 | the system null pointer, binary 0; NULL is X'FF000000', and the two do not compare equal |
| POINTERADD(p, n), PTRADD | MVS | Tier 86 | p + n, n BIT or REAL FIXED (p,0) as FIXED BIN(31); also as a locator qualifier; with LANGLVL(SPROG) only (Tier 88) |
| POINTERVALUE(n), PTRVALUE | MVS | Tier 86 | the pointer of the value n; with a constant in a STATIC pointer's INITIAL (Tier 87), and in BASED(POINTERVALUE(16)); with LANGLVL(SPROG) only (Tier 88) |
| ENTRYADDR(x), and ENTRYADDR(v) = p | MVS | Tier 87 | the address of an external entry or of an ENTRY variable's entry; as a pseudovariable it sets the variable's entry, with no parent frame |
| PLIRETC(n) (CALL), PLIRETV, PLIRETV() | OPT | Tier 87 | the return code, 0 to 999 (more is 999); the step ends with it, plus 1000 after STOP or EXIT and 2000 after ERROR (SC33-0006-7, "Return Codes"); a test that sets it names its code in `<name>.cc` |
| PLIDUMP [(options [, identification])] (CALL) | OPT | Tier 105 | a dump in PL/I terms on DD PLIDUMP or PL1DUMP: T trace of the active procedures, BEGIN blocks and on-units, F open files, H the active blocks' frames in hex, B save areas with T; C goes on, S (and E) ends without FINISH, 1000 plus PLIRETC's code. The layout is PL1C/370's own. Without the DD: IBM013I, no dump, and the options still apply. PL/I (F)'s CALL IHEDUMx is not recognised |
| PLISRTA to PLISRTD (CALL) | OPT | Tier 107 | the sort program: sort and record statements, storage, the return code variable, the exits E15 (a function returning the record) and E35 (a procedure taking it), prefix; LINK to SORT with PL/I (F)'s parameter list. The message level and the technique are accepted and not passed: the sort of MVS 3.8j takes neither. That sort abends (U0056, U0059, ...) where the Optimizer's manual gives 16 or 20. GO TO out of an exit, STOP and ERROR in one finish the sort first, as PL/I (F) does, and the return code variable is not set. A VARYING E15 result of TYPE=F is padded with blanks (PL/I (F) leaves two bytes of junk). The exits must be procedures of the compilation, not ENTRY variables, and an exit must not call PLISRTx itself (one sort at a time: the run time's sort state is static). PL/I (F)'s CALL IHESRTA to IHESRTD is the same, in both languages |
| PLICKPT [(ddname [, check-id [, org [, code]]])], PLIREST, PLICANC (CALL) | OPT | Tier 108 | checkpoint (CHKPT, SVC 63) on SYSCHK or the ddname given, PS or PO; the code is CHKPT's return code (8 for an org other than PS or PO); PLIREST ends with the user abend 4092 for an automatic restart, PLICANC cancels restart (CHKPT CANCEL). PL/I (F)'s IHECKPS, IHECKPT, IHEREST, IHERESN are the same, in both languages. On the test system no checkpoint can be taken: its disks are shared (IHJ000I 214), so only the codes without one are compared |
| BINARYVALUE(p), BINVALUE | MVS | Tier 86 | FIXED BIN(31) of the pointer's 32 bits: NULL gives -16777216; with LANGLVL(SPROG) only (Tier 88) |
| DATETIME, DATETIME() | MVS | Tier 86 | yyyymmddhhmmssttt, from one TIME macro; the milliseconds are the hundredths times ten, as for TIME |
| AFTER(s, c), BEFORE(s, c) | ISO | Tier 65 | bit strings since Tier 69 |
| COLLATE | ISO | Tier 65 | all 256 EBCDIC characters, in code order |
| COPY(s, n) | ISO | Tier 65 | n copies; REPEAT(s, n) gives n + 1 |
| DECAT(s, c, p) | ISO | Tier 65 | bit strings since Tier 69 |
| DOT(x, y [, p [, q]]) | ISO | Tier 65 | see the deviations |
| EVERY(x), SOME(x) | ISO | Tier 65 | one bit; unlike ALL and ANY, which give a bit string |
| ONFIELD | ISO | Tier 65 | the same as DATAFIELD |
| PAGENO(f), and PAGENO(f) = n | ISO | Tier 65 | see the deviations |
| REVERSE(s) | ISO | Tier 65 | bit strings since Tier 69 |
| SUBTRACT(x, y, p [, q]) | ISO | Tier 65 | as ADD(x, -y, p, q) |
| VALID(x) | ISO | Tier 65 | see the deviations |

### Constants and format items

| Item | Level | Since | Notes |
|---|---|---|---|
| hexadecimal bit string constants `'F0'B4`, `'F0'BX` | MVS | Tier 71, BX Tier 86 | each digit 4 bits |
| hexadecimal character constants `'C1C2'X` | MVS | Tier 86 | two digits a character, its EBCDIC code; an even number of digits |
| bit string constants with the radix factors B3, B2, B1: `'17'B3`, `'13'B2`, `'101'B1` | ISO | Tier 71 | each digit 3, 2 or 1 bits (CL22, CL23) |
| format items B1, B2, B3, B4, with or without a width | ISO | Tier 71 | the bits as digits of the radix, zero bits added to a whole digit; more digits than the width raise STRINGSIZE, then are cut (8.7.2.4) |

### Attributes and declarations

| Item | Level | Since | Notes |
|---|---|---|---|
| CONNECTED (CONN) | OPT | Tier 103 | taken on a parameter and in a descriptor, no effect: a cross section is passed as a copy anyway |
| INITIAL CALL p(args) | OPT | Tier 102 | GC33-0009-4 and SC26-3114-01, INITIAL attribute: p called when the variable is allocated -- AUTOMATIC at the block's entry, CONTROLLED at each ALLOCATE; not for STATIC (the rule) nor, here, BASED or factored names. The order among the block's AUTOMATIC initializations is that of the declarations |
| ENTRY ... VARIABLE, FILE VARIABLE | OPT | Tier 75 | AUTOMATIC scalars; assigned, invoked, used in FILE(...); not compared |
| CONDITION, COND | OPT | Tier 74 | declares a condition name; see the deviations |
| BASED without a pointer | OPT | Tier 76 | every reference needs a locator qualifier |
| BASED(p) with a subscripted or qualified pointer: `BASED(P(I))` | OPT | Tier 82 | "element-locator-expression"; PL/I (F) refuses it (IEM0603I) |
| UNALIGNED on a pointer, or on a structure with pointers | OPT | Tier 82 | PL/I (F) refuses it (IEM3181I) |
| INITIAL((expression)), INITIAL(reference) | OPT | Tier 78 | AUTOMATIC, BASED and CONTROLLED variables; STATIC takes constants, as in PL/I (F) |
| OPTIONS(ASSEMBLER), ASM, of an ENTRY; with RETCODE, INTER, BYADDR | OPT | Tier 89 | CALL only; R1 at a list of the data's addresses (a VARYING string's length prefix, an array's first element), the last with its high-order bit on, no descriptors; a dummy where a descriptor's type differs; up to as many arguments as descriptors. RETCODE: R15 on return is the PL/I return code (PLIRETV, and the step's code) (SC33-0006-7, chapter 11) |
| OPTIONAL in an OPTIONS(ASSEMBLER) entry's descriptor; `*` for an omitted argument | MVS | Tier 90 | a word of zeros, its high-order bit on when it is the last |
| OPTIONS(ASSEMBLER BYVALUE) of an ENTRY | MVS | Tier 90 | the list holds the values themselves, POINTER or FIXED BINARY(31); no high-order bit on the last word (a value) |
| OPTIONS(MAIN BYVALUE) | MVS | Tier 90 | each parameter, POINTER or FIXED BINARY(31), is a word R1 points at, as it is; implies NOEXECOPS. BYVALUE of other procedures is not supported yet |
| FETCH entry, ...; RELEASE entry, ...; | OPT | Tier 91 | for OPTIONS(ASSEMBLER) entries: the module is LOADed at the FETCH, or at the first CALL of an entry a FETCH or RELEASE names anywhere, and DELETEd at the RELEASE; a module not found ends in S806. A PL/I procedure is not fetched yet |
| EXTERNAL('name') | MVS | Tier 89 | the name outside, for an OPTIONS(ASSEMBLER) entry: 1 to 8 characters, as written |
| a STATIC pointer with INITIAL(NULL) | OPT | Tier 87 | GC33-0009-4, INITIAL rule 7; PL/I (F) refuses it (IEM0512I); SYSNULL and POINTERVALUE(constant) there are MVS. An OFFSET's INITIAL (NULLO) is still refused |
| GENERIC(name WHEN (descriptions), ...) | OPT | Tier 77 | the first member that matches, left to right, as GC33-0009-4 says; asterisk bounds `(*)`, `*` for any argument |
| in a generic description: precision ranges `FIXED DEC(1:5)` | ISO | Tier 77 | the Optimizing Compiler takes one precision (4.4.5.11 to 4.4.5.15) |
| a GENERIC member without WHEN | ISO | Tier 77 | for a reference without arguments (4.4.5.11, Case 1.1) |
| VARIABLE for data and labels | ISO | Tier 74 | GC33-0009-4 has VARIABLE for entries and files only |
| CONSTANT | ISO | Tier 74 | for a file or an entry, which are constants anyway |
| PARAMETER, PARM | ISO | Tier 74 | the name must be a parameter of its procedure; also in an ENTRY descriptor |
| LOCAL | ISO | Tier 74 | for a label variable; not checked (see the deviations) |
| STRUCTURE, MEMBER | ISO | Tier 74 | must agree with the levels (4.3.3.1) |
| NONVARYING, NONVAR | ISO | Tier 74 | for CHARACTER and BIT; also in an ENTRY descriptor and a generic description |
| DIMENSION(bounds), DIM | ISO | Tier 74 | the same as the bounds after the name |

An argument to an **internal procedure** whose attributes differ from
its parameter's becomes a dummy of the parameter's attributes (OPT,
Tier 98; GC33-0009-4 and SC26-3114-01, "Dummy Arguments"). PL/I (F)
passes it unconverted; `-lang=F` refuses it rather than reproduce that.

### References

| Item | Level | Since | Notes |
|---|---|---|---|
| any pointer expression as the locator qualifier: `ADDR(A) -> X`, `F(I) -> X`, `PV(2) -> X` | OPT | Tier 76 | "a pointer variable, which can be qualified or subscripted, or a function reference"; PL/I (F) takes a pointer variable only (IEM0646I, IEM0030I); `-lang=F` refuses a function reference without arguments, `GETP -> X`, too (Tier 92) |
| chained qualifiers: `Q -> NXT -> V` | OPT | Tier 76 | "Multiple Locator Qualification" |
| a fixed subscript between two asterisks: `K(*,3,*)` | OPT | Tier 61 | PL/I (F) refuses it (IEM0881I) |

### Input and output

| Item | Level | Since | Notes |
|---|---|---|---|
| COPY(file) of GET | OPT | Tier 73 | the fields to the file named, not to SYSPRINT; see the deviations |
| TAB(t1, t2, ...) option of OPEN | ISO | Tier 72 | the tab positions of a PRINT file, for LIST and DATA items and the TAB format item (8.5.1.2); at most 16 positions |
| format item TAB(w), TAB | ISO | Tier 72 | w tab positions on, or a new line and its first tab position (8.7.2.10); without a TAB option the positions are 1, 25, 49, ...; no effect in GET EDIT |

### Conditions

| Condition | Level | Since | Notes |
|---|---|---|---|
| STRINGSIZE, STRZ; prefixes STRINGSIZE, NOSTRINGSIZE, NOSTRZ | OPT | Tier 70 | disabled by default since Tier 84, as both IBM compilers have it (ANSI 6.4.1.2 enables it); raised when a string is cut to its target's length; system action a message (Tier 84); ONCODE 150 |
| ON/SIGNAL/REVERT ATTENTION (ATTN) | OPT | Tier 106 | ONCODE 1400; always enabled (not in a condition prefix); a SIGNAL with no on-unit does nothing. With `-interrupt` (or `*PROCESS INTERRUPT;`) a terminal attention under TSO raises it, polled at GET/PUT and, in procedures compiled so, at loop ends and GO TO (STAX exit, `rt/pl1attn.asm`); measured with PA1 at a TSO terminal (`test/tso/attn.lua`) |
| STORAGE | ISO | Tier 70 | raised when ALLOCATE finds no storage, the allocation tried again after a normal return; system action a message and ERROR; ONCODE 454. GC33-0009-4 has STORAGE as a built-in function only |

Not implemented from ANSI: ENTRY and FILE variables in aggregates or
as STATIC, BASED or CONTROLLED variables
(`internals/research/pli/ansi-comparison.md`).

## Excluded from the default

What PL/I for MVS & VM (SC26-3114-01) has and PL1C/370 leaves out on
purpose (maintainer, 2026-10-15). PL1C/370 does not support them: a
keyword is refused, a CEE service called by name is an unresolved
external entry. The last column says what might be worth taking up
later.

| Item | Why it is out | Later? |
|---|---|---|
| GRAPHIC, MIXED; G and GX constants; GRAPHIC, MPSTR and the other DBCS built-ins; the DBCS parts of CHARACTER (shift-out/shift-in) | DBCS (double-byte character sets) has no use on MVS 3.8j: no DBCS terminals, printers or code pages | Unlikely. GX constants without DBCS data would be cheap, but have no use either |
| The CEE callable services (CEEDATE, CEEHDLR, CEEMSG, CEE3DMP, ...) | Language Environment's own interface; MVS 3.8j has no LE | Some are useful without LE: date arithmetic (CEEDAYS, CEEDATE) and a condition handler interface. A small set of our own under PL/I names is conceivable (`internals/research/le-light-study.md`, stage 3) |
| LE's message file: MSGFILE, the IBMnnnnx messages, run-time messages apart from SYSPRINT | LE's own; PL1C/370 writes PL/I (F)'s messages to SYSPRINT | A separate message data set, for programs whose SYSPRINT is their output, could be an option of our own |
| CEEUOPT, the LE form of run-time options | LE's own | The Optimizer's PLIXOPT is the form to follow, if run-time options come (they are not excluded) |
| LE condition tokens; ILC with other LE languages (COBOL for MVS & VM, C/370 under LE) | needs LE in all languages involved, and LE's R12 convention, which cc370 does not keep | ILC with our own compilers (cc370, cobc370, fort370) is a separate question (the LE study, stage 2) |
| PLITEST, INSPECT | IBM's debugging tools under LE | A debugger of our own is a project of its own |
| PLITDLI, ASMTDLI, CEETDLI; SYSTEM(IMS), SYSTEM(CICS), EXEC DLI | IMS and CICS, which MVS 3.8j does not have | Only if one of them comes to MVS 3.8j |
| Storage above the 16 MB line | MVS 3.8j addresses 24 bits | No |

**Not excluded,** though PL/I for MVS & VM hands it to LE: what the
Optimizing Compiler had without LE -- the run-time options before `/`
in the PARM, PLIXOPT, ISASIZE, STAE/SPIE, REPORT, FETCH and RELEASE.
They are part of the default language; which of them PL1C/370 has is in
`internals/research/pli/mvsvm-inventory.md`.

## Deviations

### From PL/I for MVS & VM and the Optimizing Compiler

Where the later IBM rules differ from PL/I (F), the default follows them
(Tier 84); `-lang=F` keeps PL/I (F)'s:

- **STRINGSIZE is disabled by default,** enabled by a STRINGSIZE prefix
  (GC33-0009-4, Part II, Section H; SC26-3114-01, chapter 15). Its
  system action writes a message and the program goes on: the
  Optimizer's IBM441I `'STRINGSIZE' CONDITION RAISED`, or IBM440I
  `... RAISED BY 'SIGNAL' STATEMENT` for SIGNAL (SC33-0027-5), in the
  form of PL1C/370's other messages (`AT OFFSET +nnnnn FROM ENTRY POINT
  name`). ONCODE 150, as both manuals give it.
- **The PARM of a main procedure** with one CHARACTER VARYING parameter
  and no NOEXECOPS is run-time options, `/`, the argument (SC33-0006-7,
  "Specifying Execution-Time Options and Main Procedure Parameters in
  the EXEC Statement"; SC26-3113-01): the parameter is
  what follows the first `/`, and it is empty when there is no `/`.
  PL1C/370 has no run-time options; what stands before the `/` is not
  looked at. PL/I (F) passes the whole PARM (`-lang=F`).
- **Not followed:** a main procedure with another parameter and no
  NOEXECOPS, which SC26-3114-01 does not provide for (one VARYING
  string "must" be declared), gets the PARM as PL/I (F) passes it.

- **Sterling data stays in the default.** The Optimizing Compiler
  dropped sterling constants (`2.4.6L`) and sterling pictures (G, M, H,
  P, 6, 7, 8); GC33-0009-4 and SC26-3114-01 do not have them.
  PL1C/370 keeps PL/I (F)'s in both languages: no program of the later
  language can mean anything else by them (Tier 96).

- **An internal entry constant may be declared** with ENTRY and
  RETURNS, as PL/I (F) needs it to be (its default otherwise);
  SC26-3114-01 (p. 19, note 1 to Table 12) forbids declaring an internal
  entry constant. PL1C/370 takes the declaration, and the RETURNS must
  be the procedure's (Tier 100).

### From ANSI X3.53-1976, in the ISO extensions and the ANSI forms

- **AFTER, BEFORE, DECAT, REVERSE, COPY** take character and bit
  strings, at most 4096 bits; an arithmetic argument, which ANSI converts
  to a string, is refused.
- **COPY with a count that is not a constant** gives at most 256
  characters.
- **COLLATE:** the standard leaves the extralingual characters and their
  order to the implementation. PL1C/370 returns all 256 EBCDIC
  characters, in the order of their codes.
- **ASIN and ACOS** are computed as `ATAN(y, SQRT(1 - y*y))` and
  `ATAN(SQRT(1 - y*y), y)`, y being the argument as FLOAT:
  - the precision is that of PL/I (F)'s ATAN of these;
  - an argument beyond -1 to 1 ends in SQRT's error, IHE200I and ERROR.
- **DOT with a precision** keeps each product at (p, q), `MULTIPLY(x(i),
  y(i), p, q)`, and the sum as well. ANSI forms the exact sum first and
  converts it once.
- **PAGENO:** the first page of a PRINT file is 1, and PAGE starts the
  next. PAGENO counts the page the next item goes to, as LINENO counts
  lines. The standard numbers the pages, but where the numbering starts
  was not taken from it.
- **VALID of a numeric picture** is '1'B when the characters, read as
  the picture's value and edited by the picture again, are the same
  characters. That is the field PL/I (F)'s editing writes. ANSI
  (9.5.2.3) checks the characters position by position. The two have
  not been compared on unusual fields.
- **OFFSET(p, a):** the area a must be a level-1 BASED area, as PL/I (F)
  requires of OFFSET(a) declarations.
- **STRINGSIZE is disabled by default,** as in both IBM compilers;
  ANSI (6.4.1.2) enables it, and gives it no system action (6.4.4).
- **ONCODE of STRINGSIZE and STORAGE:** the standard leaves it to the
  implementation; PL1C/370 gives 150 (as the IBM compilers) and 454.
- **STRINGSIZE is raised** by every assignment that cuts a string,
  CHARACTER or BIT, VARYING or not, the SUBSTR, STRING and UNSPEC
  pseudo-variables, GET into a string and CHAR(x, n) included. Not
  measured against an ANSI implementation.
- **The TAB option:** a LIST item at the start of a line goes to
  position 1, whatever the first tab position; ANSI is not clear on
  that. An item at a new line taken for want of a tab position goes to
  the first one (8.7.2.10, Case 3.2).
- **COPY and COPY(file)** write each field read on a line of its own,
  as PL/I (F)'s COPY does (Tier 68). ANSI (8.7.1.7) copies the input
  stream character by character, with a new line at each record's end.
  PL/I (F) decides.
- **LOCAL** is accepted and not checked: a GO TO through a LOCAL label
  variable to another block is not refused. ANSI says such a GO TO
  must not happen (6.3.7, Step 2.2.1); PL1C/370 does not check it.
- **A CONDITION name** has a single scope in a program: an INTERNAL
  declaration in an inner block names the same condition as one
  outside it. A condition name used without a declaration is accepted,
  as PL/I (F) accepts it.
- **A locator qualifier is evaluated for each reference.** Where an
  array or a structure is assigned element by element, a qualifier that
  calls a function is evaluated for each element. ANSI does not say
  how often (not measured against an ANSI implementation).
- **GENERIC with WHEN:** a generic description may not have levels
  (STRUCTURE, MEMBER), and a structure argument matches no description.
  ALIGNED and UNALIGNED in a description are not compared. A GENERIC
  with one WHEN member selects all its members by ANSI's rule, the
  F-style ones by their exact match among them.
- **STORAGE's system action**, implementation-defined, writes `PL1C/370
  STORAGE CONDITION RAISED` and raises ERROR.

### From PL/I (F), in the language PL/I (F) has

Each is described in the measurement of its tier
(`internals/research/pli/tierNN-measurement.md`):

- **The mathematical built-in functions and `x ** y` with a FLOAT
  exponent** are correctly rounded, or within a fraction of a unit of
  the last digit; PL/I (F)'s library is often a few units off, so the
  last digit of a result can differ from PL/I (F)'s (Tiers 13 and 14;
  the accuracy of both is measured in Tier 14's note). The errors, the
  messages and the ONCODEs are PL/I (F)'s. Those of complex values and
  `z1 ** z2` (Tier 115) are computed in long precision from them,
  within a few units of the last digit.
- **COMPLEX arithmetic** (Tier 114,
  `internals/research/pli/c2-measurement.md`): ABS of a COMPLEX FIXED
  value is the square root truncated exactly; PL/I (F)'s is sometimes
  one unit of the last place high (1414.20 for 1414.1994). MULTIPLY of
  COMPLEX FIXED BINARY values gives their product; PL/I (F) gives a
  wrong value (2-14I for (3+4I) and (1-2I), where `*` gives 11-2I). In
  COMPLEX FLOAT `*`, `/`, ABS and `**`, UNDERFLOW is raised as often as
  in PL/I (F), but after the routine's other operations: where an
  OVERFLOW or ZERODIVIDE comes later in the same operation, the
  on-units run in another order.
- **A complex constant read by GET, or a string holding one, into a
  real target** gives its real part only in a program whose source has
  COMPLEX data; elsewhere it raises CONVERSION at the central sign. The
  imaginary part is checked, as in PL/I (F); PL/I for MVS & VM does not
  check it (SC26-3114-01, "Character to Arithmetic") (Tier 116).
- **A VARYING argument to an external CHAR(\*) parameter** gets its
  current length. PL/I (F) passes the maximum length and reads storage
  beyond the value (Tier 55).
- **A cross section passed as an argument** is copied in and back. A
  procedure that also reaches the array by another name may see the
  difference (Tier 61).
- **REFER with halfword (FIXED BINARY(15)) variables** works. PL/I (F)
  ends with IHE803I there (Tier 63).
- **SIGNAL CHECK(name) of a name declared nowhere** treats it as a
  label (ONCODE 510). PL/I (F) declares it as an implicit variable
  (511) (Tier 64).
- **The 48-character set (`-char48`, PL/I (F)'s CHAR48):** a reserved
  word such as GT is translated wherever it stands as a word. PL/I (F)
  requires a blank or a comment on either side and leaves the rest
  undefined (Tier 79).
- **UNBUFFERED** is accepted and does what BUFFERED does. PL/I (F)
  refuses it at OPEN for a file of V-format records (IHE156I) (Tier 68).
- **EXCLUSIVE, READ ... NOLOCK and UNLOCK** lock nothing: PL1C/370 has
  one task, and within one task PL/I (F)'s locks show no effect either
  (Q1EXC). The key of an UNLOCK is not evaluated, so a function in it is
  not called (Tier 93).
- **EVENT on a READ, WRITE or REWRITE** (Tier 111) transmits at the
  statement; the event stays incomplete and the file's conditions wait
  for the WAIT, as PL/I (F) shows them. EVENT on DELETE is not
  supported yet.
- **Events (#3 tier 0, Tier 110):** a name in WAIT must be declared
  EVENT; PL/I (F) declares it contextually. `IF E THEN` with an event E
  is refused; PL/I (F) compiles it without a message (t228 Q4E7).
  CONTROLLED and BASED events are not cleared at their allocation.
- **ENVIRONMENT:** the record format options (F, V, U, VS, VBS, CTLASA,
  CTL360) give the DCB; BUFFERS, NCP, INDEXAREA, NOWRITE, GENKEY,
  TRKOFL, LEAVE, REWIND, COBOL and CONSECUTIVE are accepted and have no
  effect (Tier 95).
- **TRANSMIT** is raised only by SIGNAL: PL1C/370's I/O does not detect
  transmission errors as such (Tier 68).
- **ONLOC** in a BEGIN block or on-unit names the procedure the block is
  in (not measured, Tier 60).
- **Separately compiled procedures:**
  - COUNT counts only in procedures that use it (Tier 60);
  - an external name of eight characters is kept, where PL/I (F) cuts it
    to seven (Tier 56).

Accepted beyond PL/I (F) without being a later language level: a
CHARACTER string DEFINED on a numeric picture variable. PL/I (F)
accepts it too (`t202ans`, JOB01052), but its rule was not measured
beyond that test.

Not yet: record I/O (INTO, FROM, LOCATE) of a structure with REFER is
refused (Tier 63).

A CHAR(\*) parameter's expressions use temporaries sized by the
longest argument the compilation passes it, and by 256 for a procedure
also called through an ENTRY variable or parameter. A longer argument
through the variable overflows them (#284).

COMPLEX is in part (#2, Tier 109; `internals/design/complex.md` tier
C1). There are:
- the attribute, imaginary constants, INITIAL with complex constants;
- assignment with mode conversion, `+`, `-`, prefix `-`, `=` and `¬=`;
- REAL, IMAG, CONJG, COMPLEX and CPLX as functions, REAL, IMAG and
  COMPLEX as pseudo-variables;
- complex to character, PUT LIST and PUT DATA, and UNSPEC of a
  variable.

Refused for now:
- `*`, `/` and `**` of complex values, ABS and the mathematical
  functions (C2, C3);
- PUT EDIT and the C format item, GET (C4);
- complex arguments, parameters and RETURNS, PICTURE COMPLEX (C5);
- REAL or IMAG of an array as a pseudo-variable, UNSPEC of a complex
  expression.
