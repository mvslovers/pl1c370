# Local corpus of IBM PL/S compiler output (MVS 3.8j source tree)

Scope: read-only measurement of `~/repos/mvs/mvs38-ibmsrc` (base), with side checks of
`~/repos/MVSSRC` (Kreiss tree, mirrors, `WORK/IKJ/*.pli`) and `~/repos/mvs/mvs38ptf`.
Sources are cited as `file:line` (paths relative to `~/repos/mvs/` unless absolute) or as
the scripts that produced a number. Scripts and per-file results:
`plscorpus/`, beside this file
(`scan.py` → `scan.tsv`, one row per file, 7,755 files; `stats.py`; `pls_ibm.txt` = list of the
2,901 classified modules; `purepls.txt`).

**Method warning (affects anyone re-measuring):** in this shell `grep` is a shell function
(wrapper), not `/usr/bin/grep`, and it silently skips files containing byte X'AC' (the PL/S
not-sign `¬`, invalid UTF-8). An early `grep -l %GOTO` gave 415 files; Python over bytes
gives 495. An early grep END-card survey saw 37 `PL/S-II` IDRs; the byte scan finds 127 in
`ibm/` alone. `cut` and `ls` also misbehave (locale; `ls` aliased to a long-format
lister). Every number below comes from Python over raw bytes or from
`LC_ALL=C /usr/bin/grep`.

---

## 1. Which tree is pristine IBM, and what is already known about PL/S output

### Takeaway
`mvs38-ibmsrc` is the pristine IBM tape source (no MACCVT); it is the base for all counts.
Dave Kreiss' MVSBLD tree and the `WORK/IKJ` `.pli` extractions are MACCVT-derived or
transcription-damaged in part and must not be used as the IBM reference. The mvs38src docs
already hold a substantial body of measured PL/S-output conventions.

### Cited Findings
- README: "MVS 3.8j source as IBM shipped it … Dave Kreiss' reconstructed tree is not here at all"; 5,927 members in `ibm/<PREFIX>/`, 436 tape variants, 90 stben variants, records exactly 80 chars, cp037 translation — `mvs38-ibmsrc/README.md:1-50`.
- Byte evidence that it is unconverted: `¬` survives as single byte X'AC' (e.g. `IF TIOFERT¬=0` at `mvs38-ibmsrc/ibm/IKJ/IKJCT436.asm:2759`), and END-card IDRs that MACCVT strips are present in 2,630 of 2,901 PL/S modules (e.g. `END IKJCT460,(C'PLS0024',0603,74342)` at `mvs38-ibmsrc/ibm/IKJ/IKJCT460.asm:1134`). MACCVT stripped exactly this parameter: "`END [label],(C'PL/S',nnnn,nnnnn)` stripped of the compiler version parameter" — `mvs38src/docs/kreiss-project.md:176`.
- 333 member names have conflicting versions across tapes; the tree defaults to stben.net's choice and keeps the rest in `variants/` — `mvs38-ibmsrc/README.md` (section "The 333 conflicts").
- Kreiss' MVSBLD (`~/repos/MVSSRC/Dave Kreiss - MVS from Source/MVSBLD`, 5,528 files): 2,665 carry PL/S markers; of the 4,902 whose name is in `ibm/`, only 1,618 are byte-identical to IBM; among differing PL/S modules, 2,341 differ even in the comment (PL/S source) lines and 107 differ only in code; 760 PL/S files carry `DSKnnnn` change markers; only 1,766 of 2,665 keep an IDR (scan in this session).
- `MACCVT` rules: register equates standardised to R0–R15, inline PL/S structure EQUs replaced by system DSECTs (`L R10,LCCACPUS-LCCA(,R8)`), bit settings converted to system-DSECT flags — `mvs38src/docs/kreiss-project.md:153-166`; MACCVT also exists as program with control cards `#MACCVTX/#MACCVTF/#MACCVTR` — `mvs38src/docs/kreiss-project.md:282`.
- `WORK/IKJ` (269 `.asm` + 270 `.pli`): `.pli` = every line starting with `*`, with the `*` removed, sequence numbers kept; produced by `~/repos/MVSSRC/WORK/IKJ/copy.py` (lines 1-30); the CLAUDE.md there says they are "kein echter PL/S-Source … nicht übersetzbar" — `~/repos/MVSSRC/CLAUDE.md:78-86`. Measured: 136 `.pli` equal the comment lines of the IBM member exactly; 133 differ (the underlying `.asm` is Kreiss' version: `¬` rendered as `^`, X'08' bytes removed, `DSK0290` markers in comment text, e.g. `IKJCT464`), 1 has no IBM counterpart (scan in this session). 135 of 269 `.asm` are byte-identical to `ibm/IKJ`.
- Mirrors (jay, tk4-, stben, mainframe.eu): each damaged differently — UTF-8 `¬` shifting columns (31,470 occurrences), X'15' in constants torn into line breaks (28 members), a third translation table in the web mirrors — `mvs38-ibmsrc/README.md` ("Why this exists"); full reconciliation in `~/repos/MVSSRC/WORK/doc/allsrc-harmonisierung.en.md`.
- `mvs38src/src` is patched-forward reconstructed source and was excluded (coordinator instruction).
- PTFs (`mvs38ptf`, 858 `.txt` card streams): no `++SRC`; 50 `++SRCUPD`, all against `HASP*` (JES2, hand-written assembler); no PTF text carries PL/S output markers (`@PSTART` or a `C'PL…'` IDR); 9 `++MAC` elements are bilingual (IECDSECS, IECDSECT, IEFALLCT, IEZDEB, IFGRPL, IKJDFPL, IKJEBECA, IKJECT, IKJTMPWA) (scan in this session). So PTFs add no PL/S-output modules, only a few updated declaration macros.

Prior knowledge on PL/S output conventions (mvs38src):
- Registers are allocated per statement, so no module-wide rename is correct; IKJCT462 uses three different assignments in two S99 regions; one source name `@05` can be two registers in the deck — `mvs38src/docs/route-b.md:505-520`.
- PL/S parks every declared-but-unreferenced name below `AGO .@UNREFD`; the macro language skips that block, so a name taken from there is undefined (as370 severity 8, instructions assembled to zero) — `mvs38src/docs/route-b.md:748-760`. The block is therefore a record of what the original program did not reference — `mvs38src/docs/route-b.md:762-770`; boundary is the line number, not a grep count — `mvs38src/CLAUDE.md:609`.
- Eight flags on one byte are declared as eight EQUs to the same address; masks are not in the module; alias EQUs in reverse line order run X'80'→X'01' — `mvs38src/docs/route-b.md:1136-1145`.
- `@NMnnnnn` is PL/S's name for a reserved field — `mvs38src/docs/route-b.md:1218-1226`.
- 3-byte pointers declared one byte into a fullword: `L @10,PDXP-1(,PDRPTR)` / `LA @10,0(,@10)` — `mvs38src/docs/route-b.md:995-1003`.
- Prologue base: `BALR 12,0` at X'1C' in every PL/S module measured — `mvs38src/docs/route-b.md:1660-1667`; `LA 12,4095(0,6)` style second base — `mvs38src/docs/route-b.md:727`.
- PL/S `¬` (X'AC') vs `^` normalisation in comparisons — `mvs38src/docs/fahrplan.md:116`; NOT and concatenation operators rendered differently by transcriptions in `*%` comment cards — `mvs38src/docs/macro-collisions.md:55-64`.
- PL/S output stamps the assembly date into the module eyecatcher — `mvs38src/docs/assembler-options.md:182-188`; date-free `47F0F0xx`+AL1+name prologue variant — `mvs38src/docs/what-is-left.md:1871`.
- Bilingual macros: `*%` prologue artefacts — `mvs38src/docs/missing-macros.md:847,997`.
- Sibling modules compiling the same PL/S construct serve as naming witnesses — `mvs38src/docs/route-b.md:1409-1434`.

### Inferences
- By analogy with layout B (IKJEFP60 prints its PROC header and DCLs before the first executable statement) and IKJCT460 (first visible statement 23), the unprinted statements 2 … first-1 are the main PROC statement and the module-level declaration block.
- For a compiler project, only `mvs38-ibmsrc` (plus its `variants/`) is a trustworthy (source-text, object-text) pair corpus; in Kreiss' tree 2,341 of the 2,448 PL/S modules that differ from IBM differ even in their comment (source) lines, so it cannot serve as the source side either; judging by the `.pli` comparison these differences are mostly transcription artefacts (`¬`→`^`, stripped X'08') plus Kreiss' comment edits (`DSKnnnn`). Whether normalising `¬` alone recovers the IBM text was not measured.
- The `.pli` files are a convenience extraction, not reconstructed source; they can be regenerated from `mvs38-ibmsrc` without the Kreiss damage in one line of Python.

### Gaps
- Whether stben/jay/tk4- carry any PL/S module absent from the tapes was not re-measured here (the README's manifest covers it; not re-derived).

---

## 2. How many modules are PL/S output, per component

### Takeaway
2,901 of the 5,009 `ibm/` members of kind `module` in `MANIFEST.tsv` (58 %) are PL/S compiler output (all 2,901 are `kind=module`; none is a macro or DSECT), across 41 component
prefixes; `variants/` adds 233 further distinct versions (161 member names), none of them a
new member and none byte-identical to the chosen `ibm/` version.

### Cited Findings
- Classification rule (frozen; `stats.py`): a file is PL/S output if **any** of: (a) END card IDR translator starts `PL` (2,630 in `ibm/`); (b) one of `@PSTART`/`@PROLOG`/`AGO .@UNREFD` present **and** ≥ 5 lines carrying a statement number (2,794); (c) ≥ 5 PL/S comment lines ending in `;` immediately followed by an instruction carrying a statement number (2,773); (d) old-compiler prologue signature `LCLA &T,&SPN` / `.@001 ANOP` (107). Union = 2,901. (d) alone adds 22 (e.g. `IFDMSG54`, message tables); (c) alone adds 1 (`AHLSETD`).
- Statement number positions found (three output layouts): **A** cols 68-71 (`0023`), the PL/S-II layout, 2,194 modules; **C** cols 65-71 as `ddSnnnn` (nesting depth, `S`, statement; 3 modules use `.`), 595 modules (e.g. `01S0024` at `mvs38-ibmsrc/ibm/IEF/IEFAB4F7.asm:37`); **B** cols 67-70 followed by two blanks, the old layout, 107 modules (e.g. `IGE0019I`); 5 IDR-only data modules have no numbered lines.
- Excluded borderline: `IDACALL` (pure PL/S `%`-macro source that mentions `@PSTART` in generated text — `mvs38-ibmsrc/ibm/IDA/IDACALL.asm:1-12`) and `IDAPATCH` (bilingual macro: a PL/S `*%IDAPATCH: MACRO;` half emitting `PATCH DC ((*-@PSTART)/20)X'00'` via `ANS(...)`, then an assembler `MACRO`/`MEND` half — `mvs38-ibmsrc/ibm/IDA/IDAPATCH.asm:1-25`); non-PL/S modules with digits in cols 68-71 (e.g. `IFFADA01`, 711 numbered lines but 4 distinct values).
- Control: IKJCT460 distinct statement numbers on comment lines = 71 by both `scan.py` and `LC_ALL=C grep '^\*' | cut -c68-71 | sort -u | wc -l`; AHLSETD (layout C) = 128 by both.
- PL/S modules / `kind=module` members per prefix (`ibm/`, joined to `MANIFEST.tsv`): IST 594/595, BLS 334/337, IEF 211/299, IEA 173/257, IKJ 159/258, IGG 115/599, HMA 111/111, IGT 110/129, IDC 101/210, IDA 97/98, IGC 94/234, IEE 75/176, AMD 68/79, IFD 66/83, IKT 61/63, IED 60/284, IGF 51/58, ICK 43/58, IRB 42/43, ICG 42/42, ICB 41/42, ILR 40/42, AHL 37/37, ISD 37/37, IFC 31/192, IGE 17/78, IEB 15/75, IRA 13/16, IEC 13/36, IOE 10/10, IFG 8/101, HMB 8/9, IDD 6/7, IAP 6/6, AKJ 3/3, XTB 3/3, IGX 2/3, AMA 1/4, IMD/ICA/ICH 1/1 (session scan).
- `variants/`: 233 PL/S files, all "other version of an `ibm/` member", 233 distinct hashes, 161 distinct member names; by source: tape `evt1202` 87, `source` 68, `sym5` 25, `sym1-4` 22, `etc1102` 14, `stben` 17 (`stats.py`).

### Inferences
- VTAM (IST), dump/IPCS (BLS, AMD), allocation/SWA (IEF), supervisor (IEA), TSO (IKJ), SMP (HMA), Access Method Services (IDC) and VSAM (IDA) are almost entirely PL/S; data management (IGG), TCAM (IED) and utilities (IEB, IFG) are mostly hand assembler.
- The variants are useful as a second compilation of nearly the same source (APAR deltas) — natural differential test pairs.

### Gaps
- The classifier cannot see PL/S modules whose generated code was later hand-edited to strip all markers; none were found by the pairing rule beyond `AHLSETD`, but absence is not proven.

---

## 3. Volume: statements, lines, size distribution

### Takeaway
About 8.66 M source lines in 2,901 modules; the compiler numbered ~1.39 M statements, of which
~0.82 M distinct statement numbers are visible in the text, and 2.62 M comment lines carry
PL/S source text. The typical module is 2,350 lines / ~350 statements.

### Cited Findings (all `ibm/`, `stats.py`)
- Lines: 8,662,249 total; quantiles q10/25/50/75/90/max = 685 / 1,281 / 2,351 / 4,067 / 6,132 / 20,465 (largest `IEAVNP05`). Histogram: <100: 65; 100-250: 54; 250-500: 83; 500-1k: 299; 1-2k: 742; 2-4k: 916; 4-8k: 606; 8-16k: 131; ≥16k: 5.
- Σ max statement number ("numbered by compiler", includes hidden declarations) = 1,390,275; quantiles 79 / 173 / 354 / 650 / 1,000 / 3,537.
- Σ distinct statement numbers appearing on any line = 817,390; Σ distinct numbers on comment lines (layouts A/C only) = 459,728 (layout B puts numbers only on code lines). Per-module comment-statement quantiles 7 / 32 / 86 / 196 / 374 / 2,519 (largest `IDCRM01`, 17,475 lines; then `IDCRS01`, `ICKRI01`, `IDCRI01`, `AMDPRSUM`).
- Comment lines (PL/S source text plus `*/*` comments) in PL/S modules: 2,617,834.
- Per layout: A 2,194 modules, 7.14 M lines, 685,015 distinct stmts; C 595 modules, 1.40 M lines, 114,856; B 107 modules, 0.12 M lines, 17,519.
- `variants/` (non-identical): 427,384 lines, 47,369 distinct statement numbers, 82,215 Σ max statement.
- `AGO .@UNREFD` block present in 2,322 of 2,901.

### Inferences
- Even discounting the hidden declaration prefix, ~0.8 M executable/structural statements each paired with its generated code is a very large conformance corpus for a code generator — far more than any hand-written test suite would cover.

### Gaps
- "Distinct statement numbers" counts statements that produced a numbered line; ELSE/END/comments that generate nothing and carry no number are undercounted.

---

## 4. Compiler versions (END-card IDR) and eye-catcher dates

### Takeaway
Three translator generations are visible: `PL/S` ver 1400 (old layout), `PL/S-II` ver
0402/0502/0503 (1973-75) and `PLShhmm` ver 0602-0802 (1974-79). In `PLShhmm` the 4 digits are
the compile time of day, not a version; the version is the second field. 271 PL/S modules
carry no IDR.

### Cited Findings (`ibm/`, 2,901 modules, `stats.py`)
- Translator field: `PLSnnnn` 2,471; `PL/S-II` 127; `PL/S` 32; none 271. Over all 2,471 `PLSnnnn`, max HH = 23 and max MM = 59 → consistent with hhmm time stamps (e.g. `PLS0024` in IKJCT460, `PLS0145` in IKJCT430 at `mvs38-ibmsrc/ibm/IKJ/IKJCT430.asm:3329`).
- Version × translator: PLS 0701 1,572; PLS 0603 708; PLS 0802 108; PL/S-II 0502 91; PLS 0602 51; PL/S 1400 32; PL/S-II 0503 23; PL/S-II 0402 13; PLS 0700 12; PLS 0801 9; PLS 0800 6; PLS 0702 5.
- IDR year (third field yyddd): 73: 72, 74: 129, 75: 442, 76: 374, 77: 541, 78: 1,067, 79: 5. By version: 0402 → 73-74; 0502/0503 → 73-75; 0602 → 74-75; 0603 → 74-78; 0701 → 75-78; 0702 → 79; 0800-0802 → 78; PL/S 1400 → 73-78.
- Eye-catcher `DC C'NAME  yy.ddd'`: 72: 7, 73: 133, 74: 119, 75: 383, 76: 291, 77: 401, 78: 865, 79: 26. Old form `DC C'<len>NAME dd MON yy'` (with an embedded length byte, e.g. X'12' before `IGE0019I 17 APR 74`, `mvs38-ibmsrc/ibm/IGE/IGE0019I.asm:7`): 38 further modules (73: 14 … 78: 6). 638 PL/S modules have neither.
- Eye-catcher date equals IDR date in 2,069 of 2,078 modules that have both.
- Layout × IDR: all 32 `PL/S`/1400 modules are layout B; layout B otherwise has no IDR (75); layout C carries mostly 0701 (452) and 0603 (75); layout A spans 0402-0802 plus 142 without IDR.

### Inferences
- The corpus is dominated by two compiler levels (PLS 0603 and 0701, ~2,280 modules), so a compatible compiler can target one code-generation style first and treat the PL/S-II 05xx and PL/S 1400 output as separate dialects.
- The eye-catcher date is the compile date (it matches the IDR), so `mvs38src/docs/assembler-options.md:184`'s point holds generally.

### Gaps
- What distinguishes 0603 from 0701 code generation was not measured.

---

## 5. Completeness: what of the original PL/S source can be reconstructed

### Takeaway
In the dominant PL/S-II layouts (A, C; 2,789 modules) the executable statements, internal
procedure headers, local DCLs inside internal procedures, GEN blocks and comments are present
verbatim, but the main PROC statement, all module-level DECLAREs and all `%INCLUDE`d
declarations are **not** reproduced. They survive only as their generated EQU/DS/DC output and
as a list of `%INCLUDE` names. The old layout B (107 modules) prints the main PROC and full
DCLs as comments, but its code uses absolute displacements and no symbolic field names.
`%` preprocessor statements are essentially never shown.

### Cited Findings
- Hidden prefix: the first statement number visible on a comment line has median 132 (q10 22, q90 406) in layout A (2,191 modules) and 96 in layout C; only 2.4 % (A) / 0.2 % (C) of modules show a statement ≤ 3 (session scan). These numbers are not printed on any line.
- `%INCLUDE` statements are replaced by a trailer: `THE FOLLOWING INCLUDE STATEMENTS WERE FOUND IN THIS PROGRAM.` then `*/*%INCLUDE SYSLIB  (IKJOCMTB)` … — `mvs38-ibmsrc/ibm/IKJ/IKJCT460.asm:502-507`; present in 1,974 of 2,194 layout-A modules and 589 of 595 layout-C.
- Included executable segments *do* appear as source (e.g. `*/* ENTRY - INCLUDED SEGMENT` and the segment's statements in `mvs38-ibmsrc/ibm/IEF/IEFAB4F7.asm:40-57`, includes listed at :341-344 incl. `SYSUT5 (SWFACEPR)`).
- DCL comment lines in layouts A/C occur only after `@PSTART`, i.e. inside the body (1,662 modules): internal procedure headers plus their local DCLs, e.g. `*NONNCP:` / `*   PROC OPTIONS(NOSAVE,NOSAVEAREA);` / `*   DCL` / `*     NNCP14SA PTR(31);` — `mvs38-ibmsrc/ibm/IGC/IGCFA10D.asm:215-219`. Comment lines matching `PROC…OPTIONS`/`PROC…;` (loose regex, mostly internal procedure headers) in 1,528 modules (`*POSRESPR:  PROCEDURE OPTIONS(SAVE(RTNREG));` in IKT modules).
- `%` preprocessor lines as comments: 1 of 2,901 modules (layout A) (`stats.py`, `pct`).
- GEN blocks are visible with their assembler text inline: `*       GEN(BR 14);` — `mvs38-ibmsrc/ibm/IKJ/IKJCT460.asm:72`; `*   GEN(STACK MF=(E,(1)));` — :370; in 1,648 A, 340 C, 75 B modules.
- What replaces the declarations: register EQUs `@00…@15` and `OCMTBPTR EQU @02` (`IKJCT460.asm:563-580`); structure fields as offset EQUs (`EWASA2 EQU ESTAEWA+248` — :731); automatic storage as `@DATD DSECT` (:516-561); constants pooled as `@CF00132 DC F'12'`, `@CB00549 DC X'0080A000'`, `@CV00576 DC V(IKJCT464)`, `@CC00665 DC C'IKJCT460'` (:520-533); unreferenced names below `AGO .@UNREFD` (:730-1130), reserved fields as `@NM000nn` (:738-745). Types, BASED/DEFINED, INITIAL and BDY attributes of the declarations are not written out; bit names lose their mask (see §1, route-b.md:1139).

Representative modules:
1. **IKJCT460** (layout A, `PLS0024`/0603/74342, 1,134 lines, 160 statements): statements 1 (prologue) and 23-160 visible; 2-22 (PROC + DCLs) absent; source starts `*   IF R0=TWELVE THEN` at `mvs38-ibmsrc/ibm/IKJ/IKJCT460.asm:22`; ends `*   END IKJCT460` (:500); 5 `%INCLUDE`s named (:503-507). Reconstructable: all executable logic with comments; declarations must be re-derived from EQU offsets and the macro library.
2. **IKJCT430** (layout A, `PLS0145`/0603/75024, 3,329 lines, 699 statements): two-base prologue (`@PSTART LA @10,4095(,@12)`, `USING @PSTART+4095,@10` — `mvs38-ibmsrc/ibm/IKJ/IKJCT430.asm:12-14`), GETMAIN of `@SIZDATD`; first visible PL/S statement `CPPLPTR=R1;` well into the numbering; RFY/RSTD register-restriction statements visible.
3. **IEFAB4F7** (layout C, no IDR, eyecatcher 73.340, 1,066 lines, 91 statements): statement ids carry nesting depth (`01S0024`), e.g. `*    IF SWNEW = YES | SWMOD = YES THEN` → `L @10,@PC00001` / `TM SWNEW(@10),B'00001010'` / `BZ @RF00024` — `mvs38-ibmsrc/ibm/IEF/IEFAB4F7.asm:36-39`. Declarations again only as EQUs (`SWVAFW EQU SWAEPA+4` — :424).
4. **IGE0019I** (layout B, no IDR, `C'IGE0019I 17 APR 74'`, 1,186 lines): hex register names `@0…@F`, `BC 15,24(0,@F)` branch-around, numbers only on code lines, absolute displacements: `*  R9=IOSERP+'28'X;` → `LA @F,X'28'` / `A @F,52(0,@3)` / `LR @9,@F` — `mvs38-ibmsrc/ibm/IGE/IGE0019I.asm:13-16`. END card without IDR.
5. **IKJEFP60** (layout B, 10,282 lines, 1,765 statements): main procedure header printed — `* IKJPARS2:` / `*    PROC OPTIONS(DONTSAVE,CODEREG(2,3),NOSAVEAREA, REENTRANT);` — `mvs38-ibmsrc/ibm/IKJ/IKJEFP60.asm:159-160`; full DCLs as comments, e.g. `*    DCL` / `*          R1 PTR(31)REG(1);` (:194-197) and structure levels `*           3 RNGEVAL1 BIT(1),` (:303); but code addresses fields by number (`L @9,612(0,@B)` — :1063). Here the source side is almost complete and the symbolic side is missing — the inverse of layout A.
- Layout statistics: modules with any DCL comment line — A 1,262/2,194, C 400/595, B 51/107.

### Inferences
- Layout A/C give exact statement→code pairs but need a separate declaration corpus (the bilingual macros, §6) to recompile; layout B gives near-complete source but its code side lacks symbol names. Combining both kinds is the route to full round-trip tests.
- Recovering module-level declarations for A/C requires joining the `%INCLUDE` trailer names to the bilingual macro PL/S halves, then reconciling local DCLs from EQU offsets/DS lengths; the `AGO .@UNREFD` block gives the complete set of declared names (referenced above, unreferenced below).

### Gaps
- Whether the PL/S-II listing option that suppresses DCLs was a compiler default or a build choice is not determinable from the corpus.
- `%` compile-time logic (`%IF`, `%DCL`, macro invocations) in the module sources is lost; only its effect is visible.

---

## 6. Bilingual macros (the declaration corpus)

### Takeaway
~480 distinct macros carry a PL/S half after `MEND` (55,885 lines), roughly half in `ibm/`
and half in `macros/maclib/`; plus ~190 pure PL/S include members. Together these are the
declaration corpus for the `%INCLUDE` names in the modules.

### Cited Findings
- Bilingual = `MEND` followed by `DCL`/`DECLARE` lines and either a `*%label:` or a `%GOTO` header: 484 files (ibm 257, macros/maclib 223, variants/tape 4), 480 distinct member names; 55,885 lines after the last `MEND` (session scan). Second signal, files containing `%GOTO`: 495 (ibm 263, maclib 227, variants 5).
- Shape: `*        %GOTO OPREPLS;  /* BILINGUAL HEADER` / `MACRO` … `MEND` / `*%OPREPLS : ;` / `*DECLARE` / `*DCL OPRA0 BIT(8) CONSTANT('01'X);` — `mvs38-ibmsrc/ibm/TOP/TOPRED.asm:1-100`.
- `ibm/` bilingual macros by prefix (byte scan with `MEND` in cols 10-13; 252 files, the 5 fewer than 257 are `MEND` cards in other columns): IHA 43, IRA 18, IEF 13, IEC 12, IGG 9, IKJ 9, IEZ 8, TXS 7, IEE 7, IEA 5, IDA 5, IGF 5, TCO 4, TXC 4, TPR 4, rest scattered.
- 129 members carry PL/S comment text with a blank instead of `*` in column 1 (IFOX00 IFO054 RC 8, object unaffected) — `mvs38-ibmsrc/README.md` ("129 members with an invalid statement"); the session scan shows many of these (e.g. `IDCCSGR`, `ISDACSB5`, `IEZVVICB`) have no `MACRO`/`MEND` at all and are pure PL/S declaration members, not assembler DSECT macros.

### Inferences
- The IHA/IEF/IKJ/IDA macros named in `%INCLUDE SYSLIB (...)` trailers (e.g. IKJOCMTB, IHASDWA, IKJCPPL, IKJECT, IKJIOPL) are largely present in this corpus, so layout-A modules can in principle get their include declarations back.

### Gaps
- No join between all `%INCLUDE` names in module trailers and available PL/S halves was computed (coverage % unknown).

---

## 7. PL/S source proper in the tree

### Takeaway
Yes, a small amount: ~190 members are pure PL/S (no assembler), including 7 real procedures
(the largest `IEAVSPSA`, 339 lines), 17 compile-time `%MACRO`s and ~163 declaration includes.

### Cited Findings (`purepls.txt` + categorisation, session scan)
- PL/S procedures (7, 918 lines): `IEAVSPSA` starts ` IEAVSPSA: PROC(FREEPTR) OPTIONS(NOSAVE(1,15),NOSAVEAREA);` with full DCLs (`DCL REGSAPTR PTR(31) REG(13);`) — `mvs38-ibmsrc/ibm/IEA/IEAVSPSA.asm:1,81`; `ICGDSMA1-4`, `IDCDF52`, `IDCDF55` are internal subroutines (`/* PROCEDURE NAME - SSCMSG`).
- Compile-time `%MACRO`s (17, 2,443 lines): `%IDACALL: MACRO KEYS(ADDBASE);` with `ANS('DO;') COL(MACCOL);` — `mvs38-ibmsrc/ibm/IDA/IDACALL.asm:1-12`; also `IGGSTART`, `IGGMPROC`, `IGGMEND`, `IDASMFR`, `VOPEN`, `VCLOSE`, `VREADWR`, `VJOURN`.
- DCL includes (163, 18,019 lines): e.g. `IDCCS*` function data tables with `%GO TO BASEFDT1;` / `DCL 1 FDTTBL BASED BDY(WORD),` — `mvs38-ibmsrc/ibm/IDC/IDCCSGR.asm:8-22`; `IHASCVA`, `IHASCE`, `IDAREGS`, `ISDA*5`.
- 9 further members mix an assembler `MACRO` with an unstarred PL/S half; 5 others unclassified.

### Inferences
- `IEAVSPSA` is the only full main procedure in pure source; whether its compiled form exists (LPA object) would give the one true source→object pair; no PL/S-output version of it is in the tree (its name occurs only in its own member and a few callers).
- The `%MACRO` members document PL/S's compile-time macro language (`ANS`, `MACLIST`, `MACKEYS`, `COL`) in real use.

### Gaps
- Not checked whether object code for IEAVSPSA is available in the DLIBs for a pair test.

---

## 8. Evidence of construct mapping (paired examples)

### Takeaway
Every executable statement is directly followed by its code, linked by the statement number;
the pairing is mechanical enough to extract automatically (2,773 modules have ≥ 5 such pairs).

### Cited Findings
- IF / compare: `*   IF R0=TWELVE THEN` → `C R0,@CF00132` / `BNE @RF00023` (stmt 0023) — `mvs38-ibmsrc/ibm/IKJ/IKJCT460.asm:22-24`.
- Bit flag off: `*       SDWASW=NO;` → `NI SDWASW(@06),B'01111111'` (0028) — `IKJCT460.asm:35-36`; bit on: `*  EXIT='1'B;` → `OI EXIT,B'10000000'` (0783) — `mvs38-ibmsrc/ibm/IKJ/IKJCT436.asm:2757-2758`.
- Assignment through based pointer with temp register: `*       R13=ADDR(EWASA1);` → `L @06,EWAPTR(,OCMTBPTR)` / `LA R13,EWASA1(,@06)` (0027) — `IKJCT460.asm:32-34`.
- Compound bit test merged into one TM: `IF POSERR=NO& VALIDERR=NO& JCTR-SCTR<CON253 THEN` → `TM POSERR,B'00100100'` / `BNZ @RF00312` / `L @09,JCTR` / `SLR @09,SCTR` / `C @09,@CF01249` / `BNL @RF00312` — `mvs38-ibmsrc/ibm/IKJ/IKJCT431.asm:874-882`; OR of two flags: `IF SWNEW = YES | SWMOD = YES` → `TM SWNEW(@10),B'00001010'` / `BZ` — `IEFAB4F7.asm:36-39`.
- DO WHILE (test at bottom): `DO WHILE SRETCDE=4& RSRRET=0;` → `B @DE00427` / `@DL00427 DS 0H` … `@DE00427 CLC SRETCDE(4),@CF00095` / `BNE @DC00427` / `LTR RSRRET,RSRRET` / `BZ @DL00427` / `@DC00427` — `mvs38-ibmsrc/ibm/IKJ/IKJCT430.asm:1005-1008,1035-1039`.
- Iterative DO: `DO ICTR=1 TO 2;` → `LA ICTR,1` / `@DL00623 DS 0H` — `IKJCT430.asm:1714-1716`.
- CALL internal procedure: `CALL MSGRTN;` → `BAL @14,MSGRTN` (0128) — `IKJCT430.asm:112-113`.
- RETURN CODE: `RETURN CODE(EXRETCDE);` → `L @13,4(,@13)` / `L @00,@SIZDATD` / `LR @01,@11` / `FREEMAIN R,LV=(0),A=(1)` / `LR @15,@06` / `L @14,12(,@13)` (0585) — `IKJCT430.asm:1586-1592`.
- Substring move: `STAEG(1:STAELEN)=STAEL(1:STAELEN);` → `L @04,STAELEN` / `BCTR @04,0` / `EX @04,@SM01390` (0111) with the executed `MVC` parked at `@SM…` after `@DATA` — `IKJCT430.asm:70-73`, cf. `IKJCT460.asm:514-515`.
- Arithmetic: `FREEADDR=(CPRECNXT+CON7)/CON8*CON8;` → `LA @00,7` / `L @03,CPRECNXT-1` / `LA @03,0(,@03)` / `ALR @00,@03` / `SRDA @00,32` / `D @00,@CF00059` (0459) — `IKJCT430.asm:1125-1131` (also shows the 3-byte-pointer `-1` idiom).
- Label conventions: `@RFnnnnn` false branch, `@RCnnnnn` continuation, `@DLnnnnn`/`@DEnnnnn`/`@DCnnnnn` loop label/end/exit, `@ELnnnnn`/`@EFnnnnn`/`@ERnnnnn` procedure end/return — `IKJCT460.asm:495,510-512`; `IKJCT430.asm:1035-1039`.

### Inferences
- Constant pooling (`@CF`/`@CB`/`@CC`/`@CV`), label numbering keyed to statement numbers and `@SM` EX targets are deterministic and can be checked byte-for-byte by a compatible compiler; register choice is per statement (route-b.md:505) and is the hardest part to match.

### Gaps
- SELECT/OTHERWISE, GOTO, and ON-unit-like constructs were not sampled; frequency of each construct across the corpus was not counted.
