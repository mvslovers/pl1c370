# PL/I Standards and Language Levels: PL/I (F) V5, OS PL/I Optimizer, ANSI X3.53-1976, ANSI X3.74 Subset G

Research date 2026-10-11. Most findings come from primary IBM, Digital Research and Prime manuals,
downloaded from bitsavers / sysovl.info and searched as text (pdftotext). Page numbers are the
printed page numbers of the manual. OCR noise in the scans is marked where it matters.

Short source keys used below (all URLs open):

- **[F-LRM]** IBM, *OS PL/I (F) Language Reference Manual*, GC28-8201-4, 5th ed., Dec 1972 (OS Release 19, F compiler Version 5) — https://bitsavers.org/pdf/ibm/360/pli/GC28-8201-4_PLI_F_Language_Reference_Dec72.pdf
- **[OPT-LRM76]** IBM, *OS PL/I Checkout and Optimizing Compilers: Language Reference Manual*, GC33-0009-4, Oct 1976 (V1 R3.0) — https://bitsavers.org/pdf/ibm/360/pli/GC33-0009-4_PLI_Checkout_And_Opt_Compiler_Lang_Ref_Oct76.pdf
- **[OPT-LRM74]** same manual, GC33-0009-3, Jul 1974 (V1 R2.0) — https://bitsavers.org/pdf/ibm/370/pli/GC33-0009-3_OS_PLI_Language_Reference_Jul74.pdf
- **[OPT-LRM72]** same manual, SC33-0009-2, Sep 1972 (R1.0) — https://bitsavers.org/pdf/ibm/370/pli/SC33-0009-2_PLIoptRef_Sep72.pdf
- **[OPT-GI]** IBM, *OS PL/I Optimizing Compiler: General Information*, GC33-0001-2, Jun 1973 (TNL GN33-6095). Chapter 3 and Appendix B, "Compatibility with (F) Compiler" — https://bitsavers.org/pdf/ibm/370/pli/GC33-0001-2_PLIgenInf_Jun73.pdf
- **[OPT-PG85]** IBM, *OS PL/I Optimizing Compiler: Programmer's Guide*, SC33-0006-7, Sep 1985 (Releases 4.0, 5.0 and 5.1) — https://bitsavers.org/pdf/ibm/370/pli/SC33-0006-7_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep85.pdf
- **[S1]** IBM, *Series/1 PL/I Introduction*, GC34-0084-0, Feb 1977. Appendix B, "Comparison of Series/1 PL/I Subset to ANS PL/I" — https://bitsavers.trailing-edge.com/pdf/ibm/series1/GC34-0084-0_PL_I_Introduction_Feb77.pdf
- **[PRIME]** Prime, *The PL/I Subset G Reference Guide*, IDR4031, Rev. 17, April 1980. Appendix F, "Differences between Full PL/I and PL/I Subset G" — https://sysovl.info/pages/blobs/prime/devel/PL1%20Subset%20G%20Ref%20Guide%20Rev%2017%20IDR4031%201980.pdf
- **[DR80]** Digital Research, *PL/I-80 Language Manual*, Dec 1980 — https://bitsavers.org/pdf/digitalResearch/pl1/PL1-80_Language_Manual_Dec80.pdf
- **[DR82]** Digital Research, *PL/I Language Reference Manual*, Oct 1982, Appendix D "Implementation Notes" — https://bitsavers.org/pdf/digitalResearch/pl1/PL1_Language_Reference_Manual_Oct82.pdf
- **[DRPG82]** Digital Research, *PL/I Language Programmer's Guide*, Dec 1982 — https://bitsavers.org/pdf/digitalResearch/pl1/PL1_Language_Programmers_Guide_Dec82.pdf
- **[ECMA50]** https://ecma-international.org/publications-and-standards/standards/ecma-50/ (PDF: https://ecma-international.org/wp-content/uploads/ECMA-50-1st-edition-December-1976-1.pdf)
- **[ISO6160]** https://www.iso.org/standard/12406.html ; https://webstore.ansi.org/standards/iso/iso61601979
- **[ANSI53]** https://webstore.ansi.org/standards/incits/ansiincits531976r1998
- **[ANSI74]** https://webstore.ansi.org/standards/incits/ansiincits741987r1998 ; https://infostore.saiglobal.com/en-us/Standards/ANSI-INCITS-74-1987-614655_SAIG_ITI_ITI_1414104/
- **[ABRAHAMS]** P. W. Abrahams, "Subset/G PL/I and the PL/I Standard", ACM — https://dl.acm.org/doi/abs/10.1145/800173.809714 (only the abstract and search snippets were readable; ACM answered 403)
- **[IRON]** Iron Spring PL/I FAQ — http://www.iron-spring.com/faq.html
- **[WP]** Wikipedia, "PL/I" — https://en.wikipedia.org/wiki/PL/I (used as a lead only)

---

## Q1. Standards: names, numbers, dates, who drafted them, and where to get the text

### Takeaway
Full PL/I was standardised in 1976 in parallel by ECMA (ECMA-50, Dec 1976) and ANSI (X3.53-1976). ISO 6160:1979 is an endorsement of X3.53-1976, not a separate text. The General-Purpose Subset ("Subset G") is ANSI X3.74-1981, revised as X3.74-1987 (later ISO/IEC 6522:1992). **ECMA-50 is the only full-standard text that is free today**, and it is an image scan of 417 pages. The ANSI and ISO texts are sold through the ANSI webstore.

### Cited Findings
- **ECMA-50**, "PL/I", 1st edition, **December 1976**, status **Withdrawn**, produced by ECMA TC10 (no longer active). It is a free PDF download. Ecma describes it as "an authoritative reference rather than a tutorial introduction". — [ECMA50]
- The ECMA-50 PDF has **417 pages** (pdfinfo: Title "ECMA-50, 1st edition, December 1976"). pdftotext extracts only 417 bytes from it, so it is an **image-only scan with no text layer**: it can be read, but needs OCR before it can be searched. — measured on the PDF from [ECMA50]
- ECMA-50 defines the language "by specifying a conceptual PL/I machine which translates and interprets intended PL/I programs". — search snippet of the ECMA page, via https://ecma-international.org/publications-and-standards/standards/ecma-50/
- **ISO 6160:1979**, "Programming languages — PL/I (Endorsement of ANSI standard X3.53-1976)". It adopts ANSI X3.53-1976 for the English version and the French standard **NF Z 65-500** for the French version. Published July 1979, last reviewed and confirmed 2024, still current. — [ISO6160]
- **ANSI X3.53-1976**, "Programming Language PL/I", now carried as **ANSI INCITS 53-1976 (R1998)** and sold on the ANSI webstore. Per its abstract it defines "the form and interpretation of computer programs written in PL/I" for "a high degree of machine independence". — [ANSI53] (search snippet; the webstore page itself answered 403, so price and page count are unverified)
- **ANSI X3.74-1987**, "Information Systems – Programming Language – PL/I General-Purpose Subset", now **ANSI INCITS 74-1987 (R1998)**, with a later INCITS 74-1987 (S2008) listing. — [ANSI74]
- X3.74-1987 contains **Appendix A** (differences from previous PL/I standards; "substantially upward compatible"), **Appendix C** "Clarifications and Restrictions from X3.74-1981" and **Appendix E** "Incompatible Changes from X3.74-1981". — SAI Global listing via search snippet, [ANSI74]
- "ANSI standard X3.53-1976 (ISO 6160:1979) specifies the 'full' PL/I language. ANSI standard X3.74-1987 (ISO/IEC 6522:1992) specifies the 'PL/I General-Purpose Subset', better known as 'Subset G'." — [IRON]
- A "PL/I General-Purpose Subset" standard was issued by ANSI in 1981, with a revision in 1987. — [WP]
- History (secondary source): standardisation began **April 1966 in ECMA TC10**. ANSI formed the "Composite Language Development Committee" ("Kludge", later **X3J1**) in 1969. IBM's language specification GY33-6003 was the base document. **Michael Marcotty (General Motors)** and **C.A.R. Hoare (ICL)** are named as committee chairs. The standard was "the first programming language standard to be written as a semi-formal definition", using a "PL/I Machine". — [WP]
- Prime's April 1980 manual calls the subset "the draft proposed ANSI standard **BSR X3.74 Revision 8**", with "the process of official adoption … not yet complete". — [PRIME] p. 1-1 area (glossary/introduction)
- Digital Research names the body behind Subset G as "the ANSI PL/I Standardization Committee X3J1". — [DR82] §1; [DR80] Introduction

### Inferences
- For the maintainer, **ECMA-50 is the practical reference text for full PL/I**: free, technically equivalent in intent to X3.53-1976, and in the same "PL/I machine" style. It needs OCR to be searchable. Its exact textual equivalence to X3.53 is inferred from the parallel ECMA/ANSI history and is not verified.
- The Subset G text (X3.74-1981 or -1987) has to be bought from ANSI/INCITS (or ISO/IEC 6522). No free copy turned up.

### Gaps
- No copy of X3.53-1976, X3.74-1981 or X3.74-1987 was found on archive.org or bitsavers. These searches were not exhaustive: archive.org was not queried directly.
- Price, page count and availability format at the ANSI webstore are unverified (403).
- The BASIS/1 drafting documents and named editors of X3.53 and X3.74 were not found. Freiburghouse's role in Subset G is unverified. Committee chairs come from Wikipedia only.
- The ISO 6522 publication year (1992) rests on the Iron Spring FAQ (and the task brief) only.
- ECMA-50 contents were not read (image scan).

---

## Q2. What Subset G removes from full PL/I, and what it adds or changes

### Takeaway
Subset G keeps PL/I's block structure, FIXED BINARY/DECIMAL, FLOAT, CHARACTER/BIT (VARYING for character), BASED/POINTER with ALLOCATE … SET, ENTRY/LABEL/FILE variables, ON-units, stream (LIST/EDIT) and record I/O incl. KEYED. It also keeps %INCLUDE. It **drops** condition prefixes, CONTROLLED, COMPLEX, AREA/OFFSET, GENERIC, LIKE, DEFAULT, iSUB, BIT VARYING, data-directed I/O, aggregate expressions, BY NAME, multiple assignment, implicit conversion between arithmetic, bit and character, implicit declaration, and all IBM extensions (multitasking, preprocessor beyond %INCLUDE, SELECT/LEAVE, CHECK/FLOW). It **tightens** several rules: everything declared, INITIAL only on STATIC, `()` required to call a parameterless function, defaults always FIXED BINARY. The detailed list below comes from Prime's April 1980 Appendix F, written against draft BSR X3.74 Rev. 8. The final 1981 standard text was not accessed.

### Cited Findings
**Provenance.** Every list here is vendor text of the draft era: Prime (April 1980, BSR X3.74 Rev. 8) and Digital Research (Dec 1980 / Oct 1982). In Prime's list, items marked `*` are "non-standard extensions to ANS PL/I", i.e. IBM features that are not in X3.53-1976 at all. — [PRIME] App. F p. F-1

Design goals of Subset G (Prime, paraphrasing ANSI): suited to commercial, scientific and systems programming. "small enough to achieve widespread implementation yet large enough to achieve widespread usage". "substantially less expensive to implement". More portable. Less error-prone because "simpler and less permissive". — [PRIME] §1 "The PL/I Subset G Language"

Abrahams: Subset/G was meant "to preserve the most useful properties of PL/I while deleting features that were either little used, uneconomic to implement, or inappropriate to what we now know about good programming practice". Snippets name complex arithmetic, implicit declarations, user-definable defaults and aggregate operations as features left out. — [ABRAHAMS] (abstract and snippets only)

**Not in Subset G** (Prime App. F, grouped as in the source):
- *Program elements*: IBM 48-character set*. Blanks inside compound operators*. Multiple label prefixes. A label on DECLARE. More than one subscript on a label prefix. Subscripted labels on PROCEDURE/FORMAT. Labels on THEN/ELSE clauses. **Condition prefixes** (e.g. `(SIZE):`). — [PRIME] App. F
- *Data elements*: binary constants. Replication factors. Sterling constants*. P/F scale characters in constants. `*`-length RETURNS. Functions returning arrays or structures. STATIC bounds and lengths must be integer constants. Parameter bounds and lengths must be `*` or constants. — [PRIME] App. F
- *Program structure*: OPTIONS on BEGIN. Multiple closure (an END label closing several blocks). PROCEDURE options ORDER*, IRREDUCIBLE*, **RECURSIVE***, REDUCIBLE*, REORDER*. OPTIONS(COBOL*, FORTRAN*, NOMAP*, NOMAPIN*, NOMAPOUT*, REENTRANT*, TASK*). — [PRIME] App. F
- *Attributes not supported*: **AREA**, BACKWARDS*, BUFFERED*, **COMPLEX**, CONDITION, CONNECTED*, **CONTROLLED**, EVENT*, EXCLUSIVE*, FORMAT, **GENERIC**, IRREDUCIBLE*, **LIKE**, LOCAL, **OFFSET**, POSITION, REDUCIBLE*, TASK*, TRANSIENT*, UNBUFFERED*. Also: CONSTANT, DIMENSION, MEMBER, NONVARYING, PRECISION, PARAMETER, REAL, STRUCTURE and UNALIGNED are supported as concepts but are not keywords. — [PRIME] App. F
- *Declaration rules*: INITIAL only on STATIC, with constants or NULL only (no CALL option*). Non-zero scale only on FIXED DECIMAL, never negative. **BIT VARYING not supported**. **iSUB defining not supported**. PICTURE restricted to a subset of fixed-point decimal pictures, with no repetition factors, no sterling, and characters A, E, G*, H*, I, K, M*, P*, R, T, X, Y, 6*, 7*, 8* excluded. **All names must be explicitly declared**. Default arithmetic attributes are always FIXED BINARY, not IBM's I–N rule* (so `FIXED(8,3)` must be written `FIXED DECIMAL(8,3)`). The ENTRY declaration of an EXTERNAL procedure is mandatory. The parameter descriptor list may not be omitted. ALIGNED only on CHARACTER/BIT, never on a structure. — [PRIME] App. F
- *ENVIRONMENT/OPTIONS*: all IBM OS options (F/FB/V/VB…, CONSECUTIVE, INDEXED, REGIONAL(1–3), TP, VSAM-era options, CTLASA, ASCII …) are implementation-defined and starred. A file constant may not be dimensioned. **No DEFAULT statement** (its VALUE and DESCRIPTORS options* are not in ANS PL/I either). — [PRIME] App. F
- *Expressions*: **no aggregate-valued expressions**. Aggregate assignment only between identically described connected aggregates, plus `array = scalar`. **No implicit conversion between arithmetic/picture, bit and character data**. No multiple-target assignment. No BY NAME. Restricted overlapping string assignment. — [PRIME] App. F
- *Built-in functions not supported*: ADD, AFTER, ALL*, ALLOCATION, ANY*, BEFORE, COMPILETIME*, COMPLETION*, COMPLEX, CONJG, COUNT*, COUNTER*, CURRENTSTORAGE*, DATAFIELD*, DECAT, DOT, EMPTY, ERF, ERFC, EVERY, HIGH, IMAG, LOW, MULTIPLY, NULLO*, OFFSET, ONCHAR, ONCOUNT*, ONFIELD, ONSOURCE, PARMSET*, PLIRETV*, POINTER, POLY*, PRECISION, PRIORITY*, PROD, REAL, REPEAT*, REVERSE, SAMEKEY*, SOME, STATUS*, STORAGE*, SUBTRACT, SUM. Pseudovariables COMPLEX*, IMAG, ONCHAR, ONSOURCE and REAL are also out. Pseudovariables are allowed only on the left of an assignment. MAX/MIN take exactly 2 arguments. FIXED/FLOAT require a precision argument. UNSPEC requires a reference. — [PRIME] App. F
- *Storage control*: ALLOCATE/FREE of one item only. **SET required** (no CONTROLLED). No IN option. — [PRIME] App. F
- *Program control*: no comma-list DO. No UNTIL*. No EXIT*, HALT*, **LEAVE***, **SELECT***. — [PRIME] App. F
- *Conditions not supported*: programmer-named CONDITION, AREA, ATTENTION*, CHECK*, CONVERSION, FINISH, NAME, PENDING*, RECORD, SIZE, STORAGE, STRINGRANGE, STRINGSIZE, SUBSCRIPTRANGE, TRANSMIT. One condition per ON/REVERT. No RETURN inside an on-unit. No SNAP/SYSTEM options. — [PRIME] App. F
- *Procedures*: one argument list per reference. A parameterless function reference without `()` yields an ENTRY value, whereas IBM invokes it implicitly*. — [PRIME] App. F
- *I/O*: no BACKWARDS*/BUFFERED*/EXCLUSIVE*/TRANSIENT*/UNBUFFERED*. No DISPLAY*, **LOCATE**, UNLOCK*. One file per OPEN/CLOSE. No TAB option. No CLOSE ENVIRONMENT/LEAVE/REREAD/REWIND. One I/O list and one format list per GET/PUT. **No DATA (data-directed) I/O**. No COPY, SNAP*, FLOW*, ALL*. INTO/FROM may not be bit-aligned. FROM required on REWRITE. No IGNORE, no EVENT* on READ/WRITE. No expressions or variables in format lists. No third E/F parameter. **No C format item**. — [PRIME] App. F
- *Preprocessor*: "The preprocessor facilities* of IBM PL/I are not supported": %ACTIVATE, %ASSIGN, %DEACTIVATE, %DECLARE, %DO, %END, %GOTO, %IF, %NOTE, %NULL, %PROCEDURE, %RETURN. Also no %CONTROL, %NOPRINT, %PRINT, %PAGE, %SKIP. — [PRIME] App. F
- *Multitasking**: DELAY, EXIT, WAIT. CALL TASK/EVENT/PRIORITY. EVENT on READ/WRITE. TASK and EVENT data. COMPLETION, PRIORITY and STATUS. — [PRIME] App. F
- *Diagnostics**: CHECK prefix and condition. CHECK, FLOW, NOCHECK, NOFLOW statements. — [PRIME] App. F

**Late additions in BSR X3.74 Rev. 8** (in the final draft, not yet implemented by Prime): the assignment `array = scalar`. Conditions **FIXEDOVERFLOW, OVERFLOW, UNDERFLOW, ZERODIVIDE, UNDEFINEDFILE**. The **STRING option on GET/PUT**. **READ … SET**. — [PRIME] §1 "PL1G Exclusions from PL/I Subset G"

**%INCLUDE** is described by Prime as part of the language, while %REPLACE is listed as a Prime extension. — [PRIME] §2 "Text replacement and insertion" and §1 "PL1G Extensions". Digital Research likewise *added* %REPLACE to its Subset G implementation. — [DR80] Introduction

**Digital Research's deviations from Subset G** (useful as a measure of what a small team leaves out): no DEFINED, no FLOAT DECIMAL ("FIXED DECIMAL is retained"), no PICTURE (only as an output edit format item in the 1982 product), no asterisk extents or dynamic arrays. No ATANH, DATE, STRING, TIME, VALID. PL/I-80 1980 additionally had no LIKE and FILE only in OPEN. Additions: %REPLACE, RANK, ASCII and variable-length ASCII record I/O. — [DR80] Introduction; [DR82] App. D

From this list one can read off what Subset G *contains* (the DR list removes these from it): DEFINED, FLOAT DECIMAL, PICTURE, asterisk extents and dynamic arrays, LIKE (DR80), and the builtins ATANH, DATE, STRING, TIME, VALID. — [DR80]; [DR82] App. D

### Inferences
- Subset G **contains LIKE**: DR lists dropping LIKE as a deviation, while Prime lists LIKE as unsupported in Subset G. This conflict may be draft revision (Rev. 8 vs. the final text) or a Prime reading. **Unverified — needs the X3.74 text.**
- Two "IBM-isms" a target-level decision touches directly are *not* in ANSI full PL/I either, per Prime's starring: SELECT/LEAVE/UNTIL and the macro preprocessor. Subset G therefore does not lose them relative to the standard; it never had them.
- Subset G fits S/370 well: FIXED DECIMAL plus a fixed-point PICTURE subset maps onto packed and zoned decimal. No CONTROLLED, AREA or tasking means a much smaller runtime.

### Gaps
- The final X3.74-1981 text and the 1987 changes (Appendices A, C, E) were not read. Which Rev. 8 items survived, whether LIKE is in, and what 1987 added (possibly SELECT, LEAVE etc.) are **unverified**.
- The Abrahams paper (ACM 403) could not be read in full.

---

## Q3. PL/I (F) V5 vs. the OS PL/I Optimizing Compiler: feature differences

### Takeaway
The Optimizer is *nearly* a source superset of F Version 5: "In general, a PL/I source program written for version 5 of the PL/I (F) Compiler will produce identical results" — but not quite. Several F features were dropped: sterling data, the REWIND option, the IHE* service entry points, some preprocessor details and NULLO. Limits changed in both directions, and object modules cannot be mixed. The Optimizer added, by release:
- **R1 (1971/72)**: DEFAULT statement, FETCH/RELEASE, OPTIONS(COBOL/FORTRAN/ASSEMBLER) interlanguage communication, entry variables and GENERIC…WHEN, extended float precision, CONNECTED, INITIAL with expressions, %PAGE/%SKIP, RESCAN/NORESCAN, PLIxxx service CALLs, STRINGSIZE.
- **R2 (1974)**: VSAM.
- **R3 (1976)**: SELECT/WHEN/OTHERWISE, the LEAVE statement, DO … UNTIL.

### Cited Findings
**F Version 5 baseline** ([F-LRM], 5th ed. Dec 1972, applies to OS Release 19):
- New in F Version 5 per the preface: teleprocessing (TRANSIENT attribute, PENDING condition, G and R ENVIRONMENT options). TRANSLATE and VERIFY builtins. STRING pseudovariable. Halfword storage for FIXED BINARY(p<16). Relaxed REFER restriction. ORDER/REORDER options. TRKOFL and NCP. Also mandatory RETURNS on function PROCEDURE/%PROCEDURE/ENTRY, and removal of ABNORMAL, NORMAL, USES and SETS. — [F-LRM] Preface
- F has the **macro preprocessor** (%PROCEDURE, %DECLARE, %INCLUDE, %ACTIVATE etc.), **CONTROLLED**, BASED/POINTER/**AREA/OFFSET** with REFER, **multitasking** (TASK, EVENT, WAIT, PRIORITY, COMPLETION), the **48-character set**, and **sterling** constants and pictures. — [F-LRM] (keyword search: %PROCEDURE 15, CONTROLLED 38, AREA 97, EVENT 168, WAIT 108, 48-character 15, sterling present; Contents "Sterling pictures" p. 259; sterling fixed-point data p. 28)
- F precision maxima: FIXED DECIMAL **15** digits. FIXED BINARY **31**. FLOAT DECIMAL **16**. FLOAT BINARY **53** (default 21). — [F-LRM] pp. 28–29
- F interlanguage: only **ENVIRONMENT(COBOL)** data interchange ("structures … mapped according to the COBOL(F) algorithm") is present. There is no OPTIONS(COBOL)/OPTIONS(FORTRAN) procedure linkage (no hits). — [F-LRM] p. 127, Fig. 10-4
- F has no FETCH/RELEASE, no DEFAULT statement, no SELECT and no VSAM (0 hits each in [F-LRM]). The Optimizer GI lists DEFAULT among features "only partially implemented or not implemented at all by version 5 of the PL/I (F) Compiler". — [F-LRM] search; [OPT-GI] Ch. 3 "Language Extensions"

**Optimizer language extensions over F V5** ([OPT-GI] Ch. 3, Jun 1973):
- **DEFAULT statement** (RANGE, parameter descriptors, RETURNS defaults). — [OPT-GI] p. 15
- Preprocessor: **RESCAN/NORESCAN** on %ACTIVATE. LENGTH and INDEX in preprocessor statements. **%PAGE/%SKIP** listing control. — [OPT-GI] pp. 15–16
- Subscripted or based locators. **REFER used more than once**, with a non-FIXED-BINARY REFER object. — [OPT-GI] p. 16
- **FETCH and RELEASE** dynamic loading of external procedures ("the new PL/I statements, FETCH and RELEASE"). — [OPT-GI] p. 16
- ENVIRONMENT: BLKSIZE/RECSIZE syntax (old F forms converted with a message). TP(M)/TP(R) replacing G/R. Variables in options. REREAD, ASCII, BUFOFF, SCALARVARYING. — [OPT-GI] pp. 16–17
- File-name expressions. File variables. **CONNECTED** attribute. INITIAL from expressions or function references for non-STATIC. **STRINGSIZE** condition. VARYING on BASED/DEFINED. — [OPT-GI] pp. 17–18
- **Extended precision**: FLOAT BINARY **109**, FLOAT DECIMAL **33** (defaults unchanged, 21 and 6). — [OPT-GI] p. 19; confirmed in [OPT-LRM76] Ch. 3 ("maximum precision … (33)", "(109)")
- **Entry variables**, **GENERIC with WHEN clauses**, ASIN/ACOS. `=`/`¬=` on most types. — [OPT-GI] pp. 19–20
- **Interlanguage communication**: OPTIONS(COBOL) / OPTIONS(FORTRAN) on PROCEDURE/ENTRY/DECLARE. OPTIONS(ASSEMBLER) on DECLARE. NOMAP/NOMAPIN/NOMAPOUT. INTER. — [OPT-GI] p. 20
- The manual lists task and event data and "multitasking facilities" among the language features (retained from F). — [OPT-GI] p. 13
- FIXED DECIMAL stays at **15** digits, and numeric character data at 15 digits. — [OPT-LRM76] Ch. 3 ("The maximum number of decimal digits allowed is 15"); Section F ("15 for FIXED DECIMAL")

**Implementation limits, F V5 vs. Optimizer** ([OPT-GI] Fig. 1, p. 13. The scan's OCR is noisy, so values are given as read, with the obvious reading in brackets):
- AREA size "32,168" [32,768] vs. "16,111,216" [16,777,216]. Array dimensions **32 vs 15**. Structure levels **63 vs 15**. Float: short/long vs. short/long/extended. Factored attributes: no limit vs. 16. Attribute factorisation nesting 13 vs 16. Block nesting 50 vs 50. STATIC INTERNAL+AUTOMATIC 16 MB vs. no limit. Descriptor-list nesting 49 vs "2". Check-list identifiers 510 vs. statement-size-limited. Data-list elements 320 vs 250. DISPLAY REPLY length 100 vs "12" [72; App. B states "The maximum length of the reply is 72"]. — [OPT-GI] Fig. 1, App. B p. 31

**Incompatibilities, F V5 → Optimizer** ([OPT-GI] App. B "Compatibility with (F) Compiler", pp. 31–34):
- An AREA's length is held in a descriptor outside the area (F: in its first 16 bytes). Max 15 dimensions and depth 15. Builtins are recognised by context only: TIME/DATE need BUILTIN or `()`, and external procedures must be declared. ADDR argument restrictions. ALLOCATION counts per task. **NULLO not implemented** (NULL serves both). PROD/SUM keep the scale. SUBSTR returns a non-varying string. — App. B p. 31
- Entry-name argument rules. External entries must be declared. RETURNS extents must be constants. Descriptor-list nesting 2. LIKE restriction (no LIKE substructure when another substructure is a LIKE object). — App. B pp. 31–32
- "Programs compiled by the optimizing compiler **cannot be link-edited with object modules produced by the PL/I (F) compiler**." — App. B p. 32
- Service routines are now CALL PLISRTx / PLICKPT / PLIREST / PLICANC / PLIRETC / PLIDUMP. The F entry names (IHESRTx, IHESARC, IHETSAC, IHEDUMx, IHECKPx, IHERESx) are not recognised. — App. B p. 32
- "**Sterling picture data is not implemented.** Therefore the following picture characters are not allowed: G, H, M, P, 6, 7, 8." — App. B p. 33 (consistent with 0 hits for "sterling" in [OPT-LRM76])
- Preprocessor: replaced text no longer gets blanks appended. No parameter descriptor list on a preprocessor ENTRY. No RETURNS in a preprocessor DECLARE. — App. B p. 33
- UNSPEC of VARYING includes the 2-byte length. COMPLETION/COMPLEX/PRIORITY/STRING are not allowed as DO control variables. READ/REWRITE/DELETE invalid for REGIONAL DIRECT OUTPUT. No default RECFM for RECORD files (UNDEFINEDFILE instead). Common-expression elimination can skip evaluating `(C=D)` in `IF (A=D)|(C=D)`. PLIRETC max 999. **REWIND option not implemented**. ~10,000 statements max. Label on DECLARE treated as on a null statement. Statement-length limits tied to region size. — App. B pp. 33–34
- The F compiler initialises VARYING strings to null on allocation, the Optimizer does not. GET LIST positioning and data-directed input ERROR/NAME differences. — App. B p. 34

**Optimizer feature arrival by manual edition** (keyword presence in the three LRM editions. These are edition-dated observations, not release-dated facts):
- Sep 1972, R1.0 [OPT-LRM72]: FETCH 35, DEFAULT 113, COBOL 185, FORTRAN 177, STRINGRANGE 25, %INCLUDE 15. **SELECT 0, VSAM 0, OTHERWISE 0**.
- Jul 1974, V1 R2.0 [OPT-LRM74]: **VSAM 81**. SELECT 0.
- Oct 1976, V1 R3.0 [OPT-LRM76]: **SELECT 37, OTHERWISE 18, UNTIL 37**. Contents list "SELECT Statement", "LEAVE Statement", "FETCH and RELEASE Statements", "DEFAULT Statement", "VSAM Organization". ITERATE 0 in all three. — keyword counts on the text of each PDF
- Wikipedia credits IBM with introducing SELECT/WHEN/OTHERWISE and ITERATE/LEAVE in the Optimizer family (no release given). — [WP]
- Releases 4.0, 5.0 and 5.1 (to Sep 1985): the Programmer's Guide amendments list only environment items. Sep 1981: GRAPHIC compiler option (DBCS), with the LRM replaced by the new "OS and DOS PL/I Language Reference Manual" GC26-3977. Oct 1984: MVS/XA support, ISAINC/HEAP/TASKHEAP run-time options, Release 4 last for VS1, Release 5.0 compiler size 128K. Sep 1985: 31-bit under CICS, VM support for 5.1. The Jul 1979 edition removed the F-compatibility appendix and pointed to the General Information manual. — [OPT-PG85] Summary of Amendments pp. vii–ix
- The Optimizer compiler runs in a 50K partition (MFT), 52K region (MVT) or 72K under TSO (1973). — [OPT-GI] Ch. 4

### Inferences
- **The Optimizer language is a near-superset of F V5.** Losses: sterling data, NULLO, REWIND, the IHE* entry names, F's larger dimension/structure-depth limits, and some preprocessor details. Gains: everything listed above.
- For an MVS 3.8j target that has PL/I (F) V5.5 as the reference implementation, "F V5 language plus selected Optimizer R1–R3 statements" (DEFAULT, SELECT, LEAVE, UNTIL; perhaps FETCH) is a coherent level. All are source constructs; none needs VSAM or MVS/XA.

### Gaps
- **PL/I (F) Version 5.5 vs Version 5.0**: no source in hand documents the 5.5 delta. The F-LRM edition used applies to "Version 5"/OS Release 19.
- The language delta of Optimizer R4 to R5.1 is in GC26-3977, which was not checked. No language additions are visible in the Programmer's Guide amendments, so the delta is marked **unverified**.
- Exact release dates of R1, R2 and R3 are not verified here. Edition dates are 1972, 1974 and 1976. Wikipedia says the first ship was for DOS in August 1971.
- ITERATE does not appear by 1976. When it arrived is unverified (possibly OS PL/I V2, 1987, outside this scope).

---

## Q4. How the IBM implementations relate to ANSI X3.53-1976

### Takeaway
No IBM OS PL/I manual examined (F 1972, Optimizer LRM 1972/74/76, GI 1973, PG 1985) contains an ANSI-conformance appendix. IBM's own 1977 comparison exists only for the Series/1 product. That table and Prime's `*` markings give a consistent picture. IBM's mainframe PL/I is **not a superset of ANS PL/I**: it adds a large set of extensions (multitasking, the macro preprocessor, SELECT/LEAVE, DISPLAY, TRANSIENT/teleprocessing, CONNECTED, sterling, the 48-character set, OS ENVIRONMENT options), and differs from the standard in defaults, aggregate promotion and the implicit invocation of parameterless functions.

### Cited Findings
- Searches of [F-LRM], [OPT-LRM76] and [OPT-PG85] for "ANS", "ANSI", "X3.53" and "American National Standard" found only ASCII and print-control references, and no conformance statement. — keyword searches on the PDFs
- IBM Series/1 PL/I (1977) "is a subset of ANS PL/I plus extensions". The reader "is assumed to know American National Standard (ANS) Programming Language PL/I (ANSI X3.53-1976)". — [S1] Preface and Ch. 1
- [S1] App. B is a comparison **against ANS PL/I** (not Subset G). Its "ANS PL/I" column says the following:
  - **In ANS**: CONTROLLED, AREA, OFFSET, COMPLEX, GENERIC, LIKE, PICTURE, DEFINED (simple and iSUB), POSITION, LOCAL, CONSTANT/VARIABLE, DEFAULT statement, assignment BY NAME, DO … REPEAT, GET/PUT DATA, COPY option, LOCATE, READ SET, ALLOCATE/FREE, format items C and TAB, conditions AREA, CHECK, NAME, STORAGE, STRINGRANGE, STRINGSIZE, SUBSCRIPTRANGE, plus the builtins ADD, AFTER, BEFORE, COLLATE, COPY, DECAT, DIVIDE, TRANSLATE, VERIFY, VALID.
  - **Not in ANS**: EVENT, TASK, CONNECTED, TRANSIENT, DISPLAY, DELAY, WAIT, the EVENT option on READ/WRITE/REWRITE, the PENDING condition, the COMPLETION/PRIORITY/STATUS/COUNT/ONCOUNT/DAYNO builtins, and OPTIONS(**MAIN**).
  - **Macro facility**: "INCLUDE — ANS YES". PAGE, SKIP and PROCESS — ANS NO.
  - Note 18: "COPY built-in function replaces the IBM REPEAT function."
  - ENVIRONMENT options are "allowed by ANS PL/I, but are implementation defined".
  — [S1] App. B pp. 26–33
- Prime marks the following as "non-standard extensions to ANS PL/I": the 48-character set, sterling, ORDER/REORDER/(IR)REDUCIBLE/RECURSIVE, OPTIONS(COBOL/FORTRAN/NOMAP*/TASK/REENTRANT), BUFFERED/UNBUFFERED/EXCLUSIVE/BACKWARDS/TRANSIENT/CONNECTED/EVENT/TASK, the DEFAULT options VALUE and DESCRIPTORS, the I–N default rule, UNTIL, EXIT/HALT/LEAVE/SELECT, ATTENTION/CHECK/PENDING, DISPLAY/UNLOCK, PUT SNAP/FLOW/ALL, the whole IBM preprocessor and listing control, multitasking, and CHECK/FLOW diagnostics. It also notes that "the rules for the promotion of one aggregate type to another differ between IBM PL/I and ANS PL/I", and that IBM implicitly invokes parameterless functions where ANS yields an ENTRY value. — [PRIME] App. F
- "IBM defaults differ from ANS defaults, where ANS defaults are FIXED and BINARY rather than FLOAT and DECIMAL." — search summary citing [S1]/[WP]. Prime states the same for Subset G ("always FIXED BINARY … rather than … the IJKLMN rule*"). — [PRIME] App. F

### Inferences
- Because ANS PL/I already excludes the IBM preprocessor (only INCLUDE), SELECT/LEAVE and tasking, **"ANSI full" is not a natural target for a compiler meant to run existing MVS PL/I (F) programs**. It would add features F lacks (C format, TAB, BY NAME semantics, COLLATE/COPY…) while rejecting IBM features F programs use (I–N defaults, implicit function invocation, preprocessor).
- RECURSIVE being starred suggests that in ANS all procedures are recursive by definition. Inferred, unverified.

### Gaps
- No IBM mainframe document listing "differences from ANS X3.53-1976" was found for the 1970s–1985. Later IBM manuals (OS PL/I V2 SC26-4308, 1987; PL/I for MVS & VM, 1995) were not checked and are outside the era.
- The [S1] App. B scan has OCR column-alignment noise in a few rows (FIXED/FLOAT/FLOOR, STATUS PV/STRING, the SUBSTR rows). Rows cited above are the cleanly aligned ones.

---

## Q5. Realistic small-team scope: Subset G vs. Optimizer level

### Takeaway
Subset G was explicitly designed to be "substantially less expensive to implement", and the commercial small-team compilers of 1980–82 implemented it: Digital Research PL/I-80/86 and Prime PL1G. Both still trimmed or extended it. Iron Spring PL/I, a modern one-person/small-team compiler, states its goal as "the Subset G standard with extensions from IBM", and reports that it compiles F and Optimizer programs "with only minimal changes".

### Cited Findings
- Subset G is "small enough to achieve widespread implementation", "substantially less expensive to implement, and utilizes computer resources more economically than full PL/I". — [PRIME] §1
- Digital Research PL/I (CP/M, 8080/Z80/8086) "is formally based on the ANSI General Purpose Subset (Subset G)". It omits DEFINED, FLOAT DECIMAL, PICTURE (output edit format only), asterisk extents and dynamic arrays, and ATANH/DATE/STRING/TIME/VALID. It adds %REPLACE, RANK, ASCII and ASCII record I/O. Constraints: a 16-level condition stack, no unconnected partial aggregate references, and 6-character external names in the Microsoft object format. — [DR82] App. D; [DR80] Introduction
- Subset G "contains all the necessary application programming constructs of full PL/I, and discards seldom-used or redundant forms … while simplifying the compilation task". — [DRPG82] Preface
- Prime PL1G "differs only minimally from standard PL/I Subset G". Its extensions are %REPLACE, RANK/BYTE, stream READ/WRITE, lower case, %LIST/%NOLIST etc. — [PRIME] §1
- Iron Spring: "goal is to implement the Subset G standard with extensions from IBM and other popular PL/I compilers". It does "not yet completely implement either standard". It has compiled programs "written for PL/I for MVS and VM, the Optimizer, and PL/I(F) with only minimal changes". "Many features of IBM's newer 'Enterprise' PL/I compilers are not supported". — [IRON]
- PL/C (Cornell teaching subset) omitted the preprocessor entirely and BASED and CONTROLLED storage. — http://teampli.net/plifamily.html

### Inferences
- **Recommended framing for the cobc370-style project** (an inference from the sources above): take **Subset G as the core** and add a measured set of **IBM F/Optimizer extensions** that MVS 3.8j programs actually use.
  - The core is FIXED DECIMAL(15) mapped to packed decimal, FIXED BIN(15/31), FLOAT short/long, CHAR/CHAR VARYING, BIT, BASED/POINTER/ALLOCATE SET, ENTRY/LABEL/FILE variables, ON-units, LIST/EDIT stream I/O, and SEQUENTIAL/KEYED record I/O.
  - Candidate extensions in rough cost order: IBM I–N defaults and implicit declaration; implicit invocation of parameterless functions; condition prefixes; %INCLUDE (already in Subset G); SELECT/LEAVE/DO UNTIL (Optimizer R3); BIT VARYING; LIKE; DEFINED/iSUB; PICTURE beyond the subset; data-directed I/O; CONTROLLED; AREA/OFFSET; the macro preprocessor.
  - Leave out multitasking, COMPLEX, sterling, the 48-character set, teleprocessing and VSAM. That keeps the runtime small, which matters on a 24-bit, memory-constrained system.
- This mirrors Iron Spring's positioning and DR's practice, and keeps the language a strict subset of what PL/I (F) V5.5 accepts (apart from Optimizer-only additions such as SELECT/LEAVE/DEFAULT), so F can serve as the reference implementation for testing.
- The Optimizer level in full (R3, 1976) adds the DEFAULT statement, FETCH/RELEASE, OPTIONS(COBOL/FORTRAN/ASSEMBLER) ILC with NOMAP, GENERIC…WHEN, extended float (33/109) and VSAM. VSAM matters little on MVS 3.8j batch, and ILC needs runtime work. These belong late in a roadmap, if anywhere.

### Gaps
- No source quantified implementation effort (person-years) for Subset G vs. full PL/I. Abrahams' paper may contain such remarks, but could not be read.
- Iron Spring's feature matrix (which IBM extensions exactly) was not retrieved beyond the FAQ.
