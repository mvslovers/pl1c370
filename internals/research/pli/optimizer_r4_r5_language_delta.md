# OS PL/I Optimizing Compiler: language delta R3.1 -> R4 -> R5.0 -> R5.1 (1976-1985)

Researched 2026-10-12. Builds on `standards_language_levels.md`, `ibm_timeline_licensing.md` and `manuals_inventory.md` (not repeated here).

**Headline:** GC26-3977, the manual this research was supposed to rest on, could not be found online in any edition. So the deltas below are **reconstructed from the companion manuals** of the same releases: the OS and DOS Programmer's Guides, the Messages book, the R5 Installation Guide and the Execution Logic. They give a bound, not proof. The result: R4 added **one** language feature, the GRAPHIC (DBCS) data type, with its constants, format item and options. R3.1, R5.0 and R5.1 show **no language changes** in any summary of amendments that was read. None of the manuals claims conformance to ANSI X3.53 or X3.74.

Source abbreviations (all bitsavers PDFs, text from pdftotext, page = printed page number):

- **[PG85]** OS PL/I Optimizing Compiler: Programmer's Guide, SC33-0006-7, Seventh Edition, Sep 1985, "applies to Releases 4.0, 5.0, and 5.1" — https://bitsavers.org/pdf/ibm/370/pli/SC33-0006-7_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep85.pdf
- **[DOSPG84]** DOS PL/I Optimizing Compiler: Programmer's Guide, SC33-0008-6, Seventh Edition, May 1984, Release 6.0 — https://bitsavers.org/pdf/ibm/370/pli/SC33-0008-6_DOS_PLI_Programmers_Guide_Rel_6.0_May84.pdf
- **[MSG]** OS PL/I Optimizing Compiler: Messages, SC33-0027-5, Sixth Edition, Feb 1982, with TNL SN26-8314 (30 Mar 1985), "applies to Release 5.1" — https://bitsavers.org/pdf/ibm/370/pli/SC33-0027-5_OS_PLI_Optimizing_Compiler_Messages_Feb82.pdf
- **[INST85]** OS PL/I Optimizing Compiler: Installation Guide for MVS, SC26-4121-1, Sep 1985, Release 5 (5.0 and 5.1) — https://bitsavers.org/pdf/ibm/370/pli/SC26-4121-1_PLI_Release_5_Installation_Guide_for_MVS_Sep1985.pdf
- **[EXL85]** OS PL/I Optimizing Compiler: Execution Logic, SC33-0025-3, Sep 1985, Release 5.1 — https://bitsavers.org/pdf/ibm/370/pli/SC33-0025-3_PLI_Optimizing_Compiler_Execution_Logic_Sep85.pdf
- **[LRM76]** OS PL/I Checkout and Optimizing Compilers: Language Reference Manual, GC33-0009-4, Oct 1976, V1R3M0 — https://bitsavers.org/pdf/ibm/360/pli/GC33-0009-4_PLI_Checkout_And_Opt_Compiler_Lang_Ref_Oct76.pdf
- **[LRM95]** PL/I for MVS & VM Language Reference, SC26-3114-01, Jun 1995 (outside the period, used only as a control) — https://bitsavers.org/pdf/ibm/370/pli/SC26-3114-01_PLI_for_MVS_and_VM_Release_1.1_Language_Reference_199506.pdf
- **[CCH]** Centre for Computing History, catalogue entry CH10613 — https://www.computinghistory.org.uk/det/10613/OS-and-DOS-PL-I-Language-Reference-Manual/

---

## Q1. Which editions of GC26-3977 exist, their dates and releases, and where are they online?

### Takeaway
Only **GC26-3977-1, September 1984** is documented by a catalogue entry (physical copy, no scan). The first edition (-0) is dated to **September 1981** only by inference. **No edition of GC26-3977 is on bitsavers or archive.org.** Neither are its two direct precursors and companions, SC26-3971 (the R4 graphic supplement) and GC26-3991 (Specifications).

### Cited Findings
- "OS and DOS PL/I Language Reference Manual", reference number **GC26-3977-1**, publication date **September 1984**, IBM, exhibit CH10613. The catalogue entry shows no scan or download. — [CCH](https://www.computinghistory.org.uk/det/10613/OS-and-DOS-PL-I-Language-Reference-Manual/)
- The OS Programmer's Guide amendment for **SEPTEMBER 1981** says: "This edition is for use with the new OS and DOS PL/I Language Reference Manual, order number GC26-3977". So GC26-3977 was new in Sep 1981, which dates the first edition (-0) to about then. — [PG85](https://bitsavers.org/pdf/ibm/370/pli/SC33-0006-7_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep85.pdf) Summary of Amendments p. ix
- The DOS Programmer's Guide amendment for SEPTEMBER 1981 says the same in the same words ("for use with the new OS and DOS PL/I Language Reference Manual"). — [DOSPG84](https://bitsavers.org/pdf/ibm/370/pli/SC33-0008-6_DOS_PLI_Programmers_Guide_Rel_6.0_May84.pdf) Summary of Amendments p. vi
- GC26-3977 merged **two** earlier books. In the OS guide the moved material comes from "the old OS PL/I Checkout and Optimizing Compilers: Language Reference Manual" (GC33-0009) — [PG85] p. ix. In the DOS guide it comes from "the DOS PL/I Optimizing Compiler Language Reference Manual" — [DOSPG84] p. vi.
- Every R4–R5.1 manual read names GC26-3977 **without an edition suffix** as the language book: [PG85] p. v; [DOSPG84] p. iv; [INST85] preface; [EXL85] preface ("OS and DOS PL/I Language Reference Manual, GC26-3977").
- Quirk: the Messages book SC33-0027-5 (Sixth Edition, Feb 1982, updated by TNL to Mar 1985) still lists "Language Reference Manual, Order No. GC33-0009" under Associated Publications. Its own Summary of Amendments refers to the R4 graphic supplement SC26-3971. — [MSG](https://bitsavers.org/pdf/ibm/370/pli/SC33-0027-5_OS_PLI_Optimizing_Compiler_Messages_Feb82.pdf) pp. iii–iv, vii
- **Absent from bitsavers.** The full `IndexByDate.txt` (fetched 2026-10-12) was grepped for `3977`, `GC26-39` and every `ibm/*/pli/`, `ibm/*/pl1/` and `ibm/360/dos/pl1/` path. No GC26-3977, SC26-3971 or GC26-3991 turned up. The newest pre-1987 IBM PL/I LRM there is GC33-0009-4 (Oct 1976). The next one is SC26-3114-01 (1995). — https://bitsavers.org/pdf/IndexByDate.txt
- **Absent from archive.org.** Advanced searches for `GC26-3977`, `GC263977`, `GC26 3977`, "OS and DOS PL/I", "pli language reference" and "pl1 language reference" returned only bitsavers mirrors of GC33-0009-3, GC28-8201-4, SC26-3114-01, the TSS PL/I LRM, a **Japanese DOS PL/I Optimizing Compiler LRM dated 1974** (`dos-pli-optimizing-compiter-language-reference-japanese`) and Digital Research manuals. — archive.org advancedsearch API, 2026-10-12
- No later GC33-0009 edition (-5 or later) was found anywhere either. A search turned up only the Computer History Museum's copy of GC33-0009-3, Jul 1974, 527 pp. — [CHM 102724087](https://www.computerhistory.org/collections/catalog/102724087)

### Inferences
- The edition numbers probably follow the releases: -0 (Sep 1981) for OS R4 and DOS of that date, -1 (Sep 1984) to go with the OS R5.0 documentation wave (the OS PG's next amendment is dated Oct 1984). Nothing confirms that -1 contains language changes; the Sep 1984 date fits the R5.0 environment items just as well.
- GC33-0009-4 (Oct 1976, R3.0) appears to have stayed the OS language book through R3.1 (Jul 1979) and R4's first months (Dec 1980, when the graphic features came out as supplement SC26-3971). Then GC26-3977 replaced it in Sep 1981.

### Gaps
- GC26-3977-0 itself (exact date and release statement) is **unverified**. Whether a -2 or later edition exists before the OS PL/I V2 manual SC26-4308 (1987) is unknown.
- SC26-3971 ("OS and DOS PL/I Optimizing Compilers: Extended Graphic Character Set Support Supplement", Dec 1980) and GC26-3991 ("OS PL/I Optimizing Compiler: Specifications") were not found online. One web search for SC26-3971 returned nothing relevant. HathiTrust and WorldCat were not queried.
- Possible physical sources: the Centre for Computing History (CH10613, -1), and the CHM collection, which was not searched for -0/-1.

---

## Q2. Language changes per release (R3.1, R4, R5.0, R5.1)

### Takeaway
**R3.1 (Jul 1979): none**, only a larger compiler storage size. **R4 (Dec 1980 / Sep 1981): the GRAPHIC data type** (DBCS / "Extended Graphic Character Set"), with graphic string constants, the G format item, the GRAPHIC ENVIRONMENT option and the GRAPHIC compiler option, plus a large *editorial* change: the I/O language (ENVIRONMENT, data transmission statements, files) moved out of the LRM into the Programmer's Guides. **R5.0 (Oct 1984) and R5.1 (1985): no language items in any summary of amendments read.** They were environment releases: MVS/XA, HEAP/ISAINC/TASKHEAP, CICS 1.6.1, VM/SP 3.

### Cited Findings

**R3.1 (V1R3M1, Jul 1979)**
- PG, JULY 1979: "For Release 3, Modification 1, the storage size in which the compiler runs has been increased". The other items in that amendment are editorial deletions (DCB appendix, the F-compatibility appendix, now in General Information). — [PG85] p. ix
- Messages: "A technical change was made for release 3, modification 1, to increase the storage size in which the compiler runs. See messages IEL0015 and IKJ65012." Two messages were added (IEL0395, IEL0885). — [MSG] p. viii
- The Mar 1980 service amendment added messages IEL0333, IEL0552, IEL0564 and IEL0775 — [MSG] p. vii. These concern constructs that already existed. IEL0333 is the nesting limit for PROCEDURE/BEGIN/IF/DO/SELECT. IEL0564 is the COPY option conflicting with GET STRING. IEL0775 is an INITIAL iteration factor above 65,535. — [MSG] message bodies
- Constructs sometimes taken for later additions are already in the R3.0 text. **SELECT and LEAVE** are in the table of contents of GC33-0009-4. So are the preprocessor **%NOTE** statement, the **STATEMENT option** of the preprocessor PROCEDURE statement, and keyword arguments to preprocessor procedures; parts of that text carry revision bars (OCR shows leading `I`), so they may have been new in R3.0. — [LRM76](https://bitsavers.org/pdf/ibm/360/pli/GC33-0009-4_PLI_Checkout_And_Opt_Compiler_Lang_Ref_Oct76.pdf) contents; text near index entries "STATEMENT option 243/482" and the "%NOTE statement" section
- The preprocessor messages IEL2233–IEL2250 (%NOTE syntax, "PARAMETER T MAY NOT BE SET MORE THAN ONCE" in a `%P: PROC(A,B) STMT RETURNS(CHAR)` example) therefore belong to R3.0 preprocessor features, not to R4. — [MSG] pp. 4–48.1

**R4 (Dec 1980 messages / Sep 1981 manuals)**
- Messages, DECEMBER 1980: "For Release 4, several new messages have been added and several messages have been modified for the PL/I Extended Graphic Character Set support which is described in OS and DOS PL/I Optimizing Compilers: Extended Graphic Character Set Support Supplement, Order No. SC26-3971." — [MSG] p. vii
- PG, SEPTEMBER 1981, "NEW PROGRAMMING SUPPORT": "For Extended Graphic Character Set support, the GRAPHIC compiler option and the GRAPHIC ENVIRONMENT option are described." — [PG85] p. ix. The DOS guide says the same for the DOS compiler. — [DOSPG84] p. vi
- **Editorial narrowing of the LRM.** The OS PG Sep 1981 edition took over from the old GC33-0009: "'The ENVIRONMENT Attribute,' data transmission statements, and related topics", the chapter "Data Sets and Files", and "Interlanguage Communication with COBOL and FORTRAN". — [PG85] p. ix. The DOS guide took the same kind of material from the DOS LRM: ENVIRONMENT, data transmission statements, "Associating Data Sets with Files", and "Communication with COBOL, FORTRAN, and RPG". — [DOSPG84] p. vi
- Shape of the R4 graphic language, reconstructed from the messages and the PG:
  - **Graphic string constants** are delimited by graphic quotes and must be "followed by a graphic 'G' and a right delimiter (the default is an SI control character)". So the form is `'…'G` inside shift-out/shift-in. The preprocessor checks this too (IEL2232 sits in the preprocessor message range). — [MSG] IEL2232 and IEL0556I; p. 48 area
  - **Length limit:** "a character string cannot exceed 32767 characters in length and a graphic string cannot exceed 16383 graphics in length". There is a CHAR/BIT/GRAPHIC/AREA size check (IEL0538I). — [MSG] explanation of the message preceding IEL0165I; IEL0538I
  - **No conversions:** IEL0554I "CONVERSION OF GRAPHIC EXPRESSION INVALID … No conversions are made by the compiler for graphic data." — [MSG]
  - **Comparison** only with `=` and `¬=`: "For graphic string data, values cannot be compared for other than = and ¬=." — [MSG] explanation before IEL0691I
  - **Not allowed in GET/PUT STRING:** IEL0855I "… A GRAPHIC ITEM IS NOT ALLOWED IN 'GET' OR 'PUT' STRING". — [MSG]
  - Graphic continuation and margins: IEL0555I/IEL01131 "INVALID CONTINUATION OF GRAPHIC STRING … each graphic requires 2 bytes". IEL0553I "END OF SOURCE TEXT IN GRAPHIC STRING". IEL2260I "RESTRICTED VALUE FOUND IN GRAPHIC CHARACTER" (X'00'–X'06' replaced by blank). — [MSG] (IEL2260I on p. 48.1, a TNL insert page)
  - **Run time:** ONCODE 3797 "INVALID ASSIGNMENT OF GRAPHIC CHARACTER STRING" in stream LIST/DATA/EDIT I/O when the source, target or file lacks "the necessary graphic attribute". Null graphic constants are restricted in PUT LIST/EDIT data lists. — [MSG] Part II, IBM648I area
  - **GRAPHIC / NOGRAPHIC compiler option** (default NOGRAPHIC). It is required for graphics in comments, or for graphics in comments and graphic constants together. The left delimiter (SO) "begin[s] a graphic string". — [PG85] "GRAPHIC Option", p. 20 area
  - **ENVIRONMENT(GRAPHIC)** for stream files: required for graphic variables and constants in stream I/O. It adds delimiters on output for edit-directed I/O, requires them on input, and raises ERROR if they are missing. "For information on the graphic data type, and on the **G-format item** for edit-directed input/output, see the OS and DOS PL/I Language Reference Manual." The example uses `ENV(GRAPHIC)` and the format list `(A(8),G(7),G(7),X(4),G(22))`. The feature is marked "Optimizing Compiler Only" (that is, not the Checkout compiler). — [PG85] pp. 134–136
  - "Details on processing Japanese or Chinese graphics are available through the IBM World Trade Americas/Far East Corporation." — [PG85] p. 134
  - The Execution Logic has a section "GRAPHIC Option of ENVIRONMENT" (p. 402). — [EXL85] contents

**R5.0 (Oct 1984)**
- PG, OCTOBER 1984: MVS/XA appendix; new **ISAINC, HEAP, TASKHEAP** run-time options; IMS/VS 1.2/1.3 error handling and 31-bit support; no VM/CMS or CICS in 5.0; "Release 4 … is the last release to support VS1"; compiler storage 128K. No language item. — [PG85] p. viii
- Messages, MARCH 1985 (for R5.0): MVS/XA support, 128K compiler, "New messages IEL0670I and IEL0935I have been added", clarified IEL0099I and restriction 305. — [MSG] p. vii
- Installation Guide, "New Programming Features … Beginning with Release 5.0": storage above 16 MB under MVS/SP V2R1 with MVS/XA DFP; a new installation option **ARCH**; and the run-time options HEAP ("isolate storage allocated to CONTROLLED and dynamically allocated BASED variables"), ISAINC and TASKHEAP. No compiler language feature is listed. — [INST85](https://bitsavers.org/pdf/ibm/370/pli/SC26-4121-1_PLI_Release_5_Installation_Guide_for_MVS_Sep1985.pdf) Ch. 1, pp. 3–4

**R5.1 (Mar/Sep 1985)**
- Messages, MARCH 1985: "Release 5.1 adds support for VM/SP Release 3, CICS/VS Release 1.6.1, and VM/PC Release 1.1." — [MSG] p. vii
- PG, SEPTEMBER 1985: 31-bit under CICS/OS/VS 1.6.1, R5.1 for VM/SP and VM/PC, miscellaneous corrections. — [PG85] p. viii
- Installation Guide, September 1985: CICS command level in 24- and 31-bit modes, macro level in 24-bit only; CMS under VM/SP 3; book reorganisation. — [INST85] pp. vii–viii

**DOS side, for completeness**
- DOS PG May 1984 (DOS R6.0): 3370-2/3380 device support and a FORTRAN I/O restriction section. Earlier amendments (Dec 1979, Jan 1979, Aug 1977) are device and VSAM items only. — [DOSPG84] p. vii

### Inferences
- The only language extension between GC33-0009-4 (1976) and the end of the V1 line is the **graphic (DBCS) data type** and its I/O. It was a Far-East-driven feature with deliberately thin semantics: no conversions, only `=`/`¬=`, no STRING I/O, and Optimizer only.
- R5.0/5.1 were environment releases. The two new R5.0 messages could in principle hint at a language edge, but everything around them (XA, storage, restriction 305) is environment.
- For an MVS 3.8j-oriented compiler project, the effective "Optimizer language level" is therefore **GC33-0009-4 (R3.0) plus, optionally, GRAPHIC**. The 1976 LRM is sufficient for everything except DBCS. Note that I/O and ENVIRONMENT details after 1981 sit in the Programmer's Guides, not in the LRM.

### Gaps
- The text of messages **IEL0670I and IEL0935I** (new in R5.0) is not in the bitsavers scan, which skips from IEL0660I to IEL0671I and from IEL0934I to IEL0940I. Their subject is therefore unverified.
- The R4 graphic language as defined in the LRM is not verified from the LRM itself. That covers the exact declaration syntax (presumably `GRAPHIC(n)` and `GRAPHIC VARYING`), whether mixed character/graphic strings existed (no "mixed" or M-constant wording appears in [MSG] or [PG85]), and any graphic built-in functions (none show up in [MSG]).
- GC26-3977-1 (Sep 1984) may carry its own Summary of Amendments with language corrections. **Not seen.**

---

## Q3. Was R5 a language change at all?

### Takeaway
On the evidence available, **no**. Every R5.0/R5.1 summary of amendments that was read lists only operating-environment, storage and run-time-option items. None of them is a language manual, so this is a strong bound but not proof.

### Cited Findings
- R5.0 items: MVS/XA, ISAINC/HEAP/TASKHEAP, IMS/VS, 128K compiler, VS1 dropped. — [PG85] p. viii
- R5.0 "New Programming Features": above-16 MB storage, ARCH, HEAP/ISAINC/TASKHEAP. — [INST85] pp. 3–4
- R5.1 items: CICS 1.6.1 (31-bit), VM/SP 3, VM/PC 1.1. — [MSG] p. vii; [PG85] p. viii; [INST85] p. vii
- Later IBM documentation treats V1R5.1 as a *compatibility* baseline rather than a language level: PL/I for MVS & VM supports object code from "OS PL/I Version 2 and Version 1 Release 5.1", and its CMPAT option governs sharing aggregates with programs "compiled by OS PL/I Version 1 Release 5.1". — [LRM95](https://bitsavers.org/pdf/ibm/370/pli/SC26-3114-01_PLI_for_MVS_and_VM_Release_1.1_Language_Reference_199506.pdf) pp. xvii, and the Arrays chapter

### Inferences
- The HEAP option touches language semantics only indirectly: where CONTROLLED and BASED storage lives. The language itself is unchanged.
- R5.x does not run on plain MVS 3.8 (see `ibm_timeline_licensing.md`), and it adds no language. So for a 3.8j target, nothing in the language is lost by stopping at R4 (or R3.1, if GRAPHIC is ignored).

### Gaps
- No LRM edition was checked against the R5 releases (GC26-3977-1 is unseen).

---

## Q4. Any ANSI X3.53-1976 or X3.74 (Subset G) statement in these manuals?

### Takeaway
**None in any manual read.** The "Industry Standards" sections of the OS and DOS Programmer's Guides (and still the 1995 LRM) name only ASCII and the pocket-select and carriage-control standards. GC26-3977's own preface could not be checked.

### Cited Findings
- OS PG, "INDUSTRY STANDARDS": "designed according to the specifications of the following industry standards as understood and interpreted by IBM as of May, 1979". The list is ASCII X3.4-1977; Pocket Select Characters X3.77-1980; dpANS X3.78 Vertical Carriage Positioning Characters. No PL/I standard is named. — [PG85] p. vii
- DOS PG: the same three standards, "as of October, 1979". — [DOSPG84] p. v
- The 1995 PL/I for MVS & VM LRM repeats the same three-standard list. A grep for "X3.53", "X3.74" and "ANS PL/I" found nothing there. — [LRM95] preface (about line 960 of the text)
- The earlier notes already record that GC33-0009-4 (1976) contains no conformance statement. — `standards_language_levels.md` Q4

### Inferences
- IBM did not position the Optimizer (R3.1–R5.1) as conforming to X3.53-1976 in its user documentation, even though GC26-3977 appeared five years after the standard. The ANSI comparison IBM did publish in this era is in the Series/1 PL/I manuals (see `standards_language_levels.md`).

### Gaps
- The preface of GC26-3977 (-0/-1) and the General Information manual GC33-0001 (later editions, which the Jul 1979 PG says contain the F-compatibility comparison) were not read for ANSI statements.
- The Specifications manual GC26-3991 was not found.
