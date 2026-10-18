# Prior Art: Available PL/I Compilers (for a new PL/I cross compiler targeting S/370, MVS 3.8j)

Research date: 2026-10-11. Note: www.iron-spring.com has an expired TLS certificate; its pages were read via plain HTTP (curl -k), so the citations below link the http:// URLs.

## Summary matrix (details and sources in the sections below)

| Implementation | License | Impl. language | Language level | Target(s) | Status / last release | Source? | (a) retarget base | (b) test source | (c) design ref |
|---|---|---|---|---|---|---|---|---|---|
| Iron Spring PL/I (Peter Flass) | Compiler: freeware binary, closed source; runtime library: LGPL | not stated by vendor (gap) | Aims at Subset G plus IBM extensions; source compatible with IBM "PL/I for MVS and VM" | Linux x86 (32-bit code); OS/2 frozen at 1.4.0 | Active: compiler 1.4.1 (Apr 2026), preprocessor 1.0.0 (15 Jul 2026) | Library + samples only | No (compiler closed) | Yes, samples | Yes, runtime library (PL/I + asm) |
| pl1gcc | GPL v2 | C (GCC front end, flex/bison) | Parser targets full PL/I; parses most of Multics source | none (no code gen) | Dead: 0.0.15, 26 Sep 2007 | Yes | Parser only | Weak | Grammar reference |
| Multics PL/I (Version 2) | Multics License (permissive) | PL/I (self-hosting) | IBM Y33-6003 spec, not full ANSI | Honeywell 6180 / DPS8M (EIS) | Historical; runs on DPS8M simulator | Yes (MIT archive) | No (in PL/I, 36-bit target) | Corpus yes | Strong (operators, runtime design) |
| PLI-2000 (Steadsoft) | GPL-3.0 | C | "much of" ANSI X3.74-1987 (Subset G) | 32-bit Windows NT, COFF | Builds on VS2019 | Yes | **Best C base found** (but GPL-3) | Yes (`/pl1`) | Yes |
| pli-llvm (ross-shk) | MIT | C++20 + C runtime | ~80% of TR 25.084 + some Enterprise extensions | LLVM (Linux/macOS/Windows) | Very new and active (announced Oct 2026, 431 commits, 1 star) | Yes | No (C++/LLVM, no S/370 backend) | **Yes, ~450 tests, MIT** | Yes |
| Digital Research PL/I-80/86 | Proprietary; binaries on abandonware sites | not stated (gap) | Subset G | CP/M-80, CP/M-86, DOS | Historical (1980-82) | No source found | No | No | Manuals on bitsavers |
| PL/C (Cornell) | Object code: informal permission (2012) for simulators | S/360 assembler + macros | Almost full PL/I, plus error repair | OS/360/370, HASP (PL/CT: VM/CMS, TSO) | Last release 7.6 (1977) | **Source lost** | No | No | Paper (AFIPS) |
| SP/k (Toronto), PLAGO (Brooklyn) | unknown | unknown | Small teaching subsets | 360/370, PDP-11 (SP/k) | Historical | Not found | No | No | Subset design (CACM 1977) |
| Kednos PL/I (ex-DEC VAX PL/I, Freiburghouse lineage) | Commercial; hobbyist licenses | not stated | Full DEC PL/I | OpenVMS VAX/Alpha, Tru64 | Company stopped trading Oct 2016 | No | No | No | Docs online |
| IBM PL/I (F) V5.5 | ships with MVT 21.8f (public-domain status in community) | asm | PL/I(F) | OS/360, MVS 3.8j | Already on target | No | No | Target-side reference behaviour | Yes |
| IBM OS PL/I Optimizer (5668-910) | IBM licensed program | — | Optimizer / V2 | MVS | No legal free source found | No | No | — | Manuals on bitsavers |

## Iron Spring PL/I (Peter Flass)

### Takeaway
The best-maintained non-IBM PL/I and the only active one: its compiler binary is freeware but closed source, and its runtime library and sample programs are LGPL and come as source. That makes it useful as a reference for language behaviour and runtime design, and maybe for test programs. It cannot be a codebase to retarget. It produces x86 ASCII code. The only S/370 work by Flass found is his XPL upload (not PL/I).

### Cited Findings
- License text: "Iron Spring Software grants you the right to use and copy the Iron Spring PL/I compiler and library freely. You may distribute programs compiled using the compiler and/or linked with the library under any terms you wish without restriction. The source to the library is licensed under the terms of the Gnu Lesser General Public License (LGPL). All material is copyright Iron Spring Software, 2023." — [Release Notes Linux 1.4.1](http://www.iron-spring.com/readme_linux.html)
- FAQ: "The compiler is currently closed-source. The source for the library has been released under the Gnu 'Lesser General Public License' (LGPL) ... to assist developers of other PL/I compilers provide compatibility, and to supply examples of PL/I coding." — [Iron Spring FAQ](http://www.iron-spring.com/faq.html)
- Price: "free for non-commercial and hobbyiest use"; a commercial version is announced, priced "approximately two orders of magnitude less than IBM's compiler for Windows". — [FAQ](http://www.iron-spring.com/faq.html)
- Language level: "Iron Spring PL/I beta does not yet completely implement either standard [X3.53-1976 full, X3.74-1987 Subset G], but goal is to implement the Subset G standard with extensions from IBM and other popular PL/I compilers." — [FAQ](http://www.iron-spring.com/faq.html)
- Mainframe compatibility: the design goal is source compatibility with IBM "PL/I for MVS and VM". It has compiled programs written for PL/I for MVS & VM, the Optimizer and PL/I(F) "with only minimal changes". There are no DL/I or CICS preprocessors, and "Many features of IBM's newer 'Enterprise' PL/I compilers are not supported." It uses ASCII and native x86 data formats, with native packed decimal. — [FAQ](http://www.iron-spring.com/faq.html)
- Distribution contents: the compiler executable `plic`, the static library `libprf.a`, "and the sources for the library and sample programs". The library tree has `include`, `source` (PL/I source), `asm` (assembler, which needs JWASM to rebuild) and `alt` (an alternative storage manager that uses malloc/free). The heap manager is based on code by Wolfram Gloger (ptmalloc). Static linking only (no PIC). Tested on Ubuntu 24.04; it needs 32-bit glibc only if C is called. — [Release Notes 1.4.1](http://www.iron-spring.com/readme_linux.html)
- Calling convention: the OS/2 "system" calling sequence. It can call C, and callbacks from C work only when main is PL/I. — [FAQ](http://www.iron-spring.com/faq.html)
- Status: compiler 1.4.0 on 26 May 2025 (array expressions), 1.4.1 on 04 Apr 2026 (array expressions fully supported, IBM-compatible HEX builtin). OS/2 support discontinued and frozen at 1.4.0. Preprocessor 0.3.0 (Sep 2025), 0.3.1 (Apr 2026), 0.3.2 (Jun 2026), 1.0.0 (15 Jul 2026). — [Iron Spring News](http://www.iron-spring.com/news.html). Discrepancy: the release-notes header dates Linux 1.4.1 "15 Apr 2026", the news page "04 Apr 2026". — [Release Notes](http://www.iron-spring.com/readme_linux.html)
- Company started in 2007. Its customers "range from individual developers to a large state government agency". — [About](http://www.iron-spring.com/about.html)
- The only S/370 or Hercules work by Peter Flass found: he uploaded an XPL compiler ("a derivative of PL/I designed for compiler writing") to the Hercules-390 group files in May 2001. It is also on the CBT tape as file #517. — [Jay Moseley, Compilers for MVS 3.8](https://www.jaymoseley.com/hercules/compilers/list_of.htm)

### Inferences
- The LGPL runtime is written in PL/I plus x86 assembler, so it cannot be compiled directly for S/370 by cc370. It is still a worked design for a PL/I runtime: conditions, stream I/O, picture editing, storage management, builtins. Reusing its code would put LGPL obligations on a statically linked MVS runtime, which needs a license decision. A clean-room C runtime that uses it only as a reference avoids that.
- The sample programs and the library's own PL/I source together form a medium-sized, real-world PL/I corpus that the new compiler could eventually compile (a good self-test target, like the Multics corpus).
- Its stated scope (Subset G plus IBM "PL/I for MVS and VM" extensions) is a realistic, proven scope for a hobbyist compiler, and a good template for scoping a new one.

### Gaps
- The implementation language of the Iron Spring compiler is not stated on the pages read. It is often said to be written in PL/I and self-hosting; not verified.
- No public test suite from Iron Spring was found beyond the samples directory. Whether `samples/` carries an explicit license was not checked (the release notes put "library and sample programs" under one distribution, with LGPL stated for the library).
- No evidence of any Iron Spring S/370 backend or plans for one.

## pl1gcc (GCC front end)

### Takeaway
A GPL-2 GCC front end, active 2002–2007. It got as far as a flex/bison parser and preprocessor that handle essentially full PL/I and most of the Multics source, but it never generated code. It is of limited value: possibly as a grammar reference, though GPL-2 restricts copying.

### Cited Findings
- "PL/I for GCC is released under the terms of the GNU Public License; version 2." The project started in 2002. — [pl1gcc home](https://pl1gcc.sourceforge.net/)
- 0.0.8: "the entire PL/I language can now be parsed using only flex and bison"; 0.0.10: "most if not all of the Multics source code can be parsed". — [pl1gcc home](https://pl1gcc.sourceforge.net/); [gcc mailing list, 0.0.11 announcement (May 2006)](https://gcc.gnu.org/pipermail/gcc/2006-May/157161.html)
- The last release was 0.0.15 (26 Sep 2007): preprocessor `%DO` partly implemented, "still no code generation taking place". — [pl1gcc 0.0.15 page](https://pl1gcc.sourceforge.net/pl1gcc-0.0.13.index.html); [SourceForge news](https://www.sourceforge.net/p/pl1gcc/news)

### Inferences
- cc370 is a GCC 3.4.6 fork, so in principle a GCC front end could plug into it. But pl1gcc never reached GENERIC/RTL lowering, so all the hard work (semantics, the PL/I data model, conditions, runtime) is missing. Its bison grammar is a useful cross-check on how to parse PL/I's keyword-less syntax with a LALR tool.

### Gaps
- Exact GCC version pl1gcc was built against; not checked.

## Multics PL/I compiler

### Takeaway
The Version 2 compiler source (written in PL/I) is publicly available under the permissive Multics License. It is historically the most important "real" non-IBM PL/I compiler, but it is self-hosting PL/I aimed at the 36-bit Honeywell 6180/DPS8M, so it is not a retarget base for a C99 tool. It is very valuable as a design reference (Freiburghouse's papers, the operator/runtime split) and as a large PL/I corpus.

### Cited Findings
- Version 1 came out in 1969 from GE CISL (Freiburghouse, Mills, Chang, Wolman, Belmont). It implemented IBM's specification Y33-6003-0, "not full ANSI standard", and was written in PL/I. — [multicians.org PL/I page](https://multicians.org/pl1.html)
- Version 1 compiled directly to Multics object segments and used `pl1_operators_` runtime support segments. Version 2 (from 1972) was co-designed with the Honeywell 6180 EIS and "produced much more efficient code than version 1". The Version 2 source is available at MIT; the Version 1 source is lost. — [multicians.org PL/I page](https://multicians.org/pl1.html)
- Freiburghouse's paper "The Multics PL/1 Compiler" describes the Version 1 compiler in detail and calls it "the only PL/1 compiler written in PL/1" and the first to produce high-speed object code. — [The Multics PL/1 Compiler (Freiburghouse)](https://www.multicians.org/pl1-raf.html)
- After 1975 Freiburghouse founded Translation Systems Inc. It built retargetable compilers on an internal n-tuple representation, licensed them to Data General, Prime, CDC and others, and the line eventually went to Kednos. — [multicians.org PL/I page](https://multicians.org/pl1.html)
- License: the Multics License lets users "use, copy, modify, and distribute these programs and their documentation for any purpose and without fee". The code was donated to MIT by Groupe Bull (2007). — [Open Hub: Multics License](https://openhub.net/licenses/Multics); [SPDX archived: Multics](https://spdx.org/licenses/archive/archived_ll_v1.17/Multics); [OSnews: MIT releases source of Multics](https://www.osnews.com/story/18906/mit-releases-source-of-multics/)
- Source archive: browsable at web.mit.edu/multics-history, which also has the `pl1.info` segment. — [pl1.info at MIT](https://web.mit.edu/multics-history/source/Multics/doc/info_segments/pl1.info)
- Multics runs today on the DPS8M simulator. — [awesome-pli list](https://github.com/ross-shk/awesome-pli)

### Inferences
- Permissive licensing makes Multics PL/I code (including system sources) a legally clean test corpus. pl1gcc showed that a parser can accept "most if not all" of it, which makes it a good parser stress test. Semantic and runtime tests would need Multics-specific builtins and system calls removed.
- The Freiburghouse papers, plus Translation Systems' n-tuple IR approach (the ancestor of the DG/Prime/DEC/Kednos compilers), are the most relevant published designs for a portable PL/I compiler.

### Gaps
- Size of the Version 2 compiler source (number of lines and segments in `bound_pl1_`) not measured.

## PLI-2000 (Steadsoft), a complete PL/I compiler in C

### Takeaway
The only complete, open-source PL/I compiler written in C that was found. It targets Subset G (ANSI X3.74-1987), the same level a hobbyist compiler would aim for. Its backend emits 32-bit COFF for Windows NT, and its runtime is in C. It is the closest structural match to a cc370/cobc370-style project, but it is GPL-3.

### Cited Findings
- License GPL-3.0. Implemented in C. Implements "much of the ANSI standard 74-1987". Generates 32-bit COFF objects that are linked with a C runtime support library (I/O, OS interface) and run on Windows NT. Started as an academic project in summer 1990 (lexer → recursive descent parser → codegen). Builds with Visual Studio 2019. Test sources are in `/pl1`. — [GitHub Steadsoft/PLI-2000](https://github.com/Steadsoft/PLI-2000)

### Inferences
- A recursive-descent, C-written Subset G compiler with a C runtime is architecturally close to what cobc370 is for COBOL. Its parser and semantic phases could in principle be reused with a new S/370 assembler emitter. But GPL-3 would apply to the whole compiler (the runtime license also matters, because it gets statically linked into MVS load modules). The maintainer must decide whether that is acceptable. Otherwise use it as a reference only.
- Its x86 backend presumably assumes ASCII and little-endian layouts; retargeting would need a full audit of data representation (FIXED DECIMAL → packed, CHAR → EBCDIC, big-endian BINARY, hex float).

### Gaps
- Last commit date, code size and runtime library size not read from the repository. Completeness of Subset G coverage not verified.

## pli-llvm (ross-shk), new and MIT-licensed

### Takeaway
A brand-new (announced October 2026), very active, MIT-licensed PL/I compiler in C++20 on LLVM, built to the formal Vienna definition (TR 25.084). It has ~450 tests. That MIT test suite is probably the most reusable artefact for this project. The compiler itself (C++ on LLVM) does not fit a C99/S/370 code base.

### Cited Findings
- It announces itself as "a new LLVM-based cross-platform open-source PL/I implementation", built to TR 25.084 (Concrete Syntax of PL/I, IBM Vienna, 1968) for syntax and Y33-6003 for semantics. At announcement, milestones M0–M2 (procedures, control flow, arrays, structures) were complete. — [DEV Community announcement](https://dev.to/rossshk/im-building-a-new-llvm-based-cross-platform-open-source-pli-implementation-227o)
- MIT license, C++20 with a C runtime library. About 80% of TR 25.084 plus selected IBM Enterprise PL/I extensions: conditions, stream and record I/O, preprocessor with `%INCLUDE`. About 450 tests in categories (core, builtins, usecases, driver, IR, preprocessor, multimodule, corner cases). 431 commits, 1 star. Diagnostics cite TR 25.084 rule numbers for unimplemented features. — [GitHub ross-shk/pli-llvm](https://github.com/ross-shk/pli-llvm)
- Same author keeps an "awesome-pli" list. It names only pli-llvm, IBM Enterprise PL/I and Iron Spring as compilers, plus the TeamPL/I source archive, Rosetta Code, TK5 (with PL/I (F)) and DPS8M. — [awesome-pli](https://github.com/ross-shk/awesome-pli)

### Inferences
- The "80% / 450 tests" figures come from the README as summarised by a fetch tool and were not counted independently. Being weeks old with 1 star, the project's longevity is unknown. Its MIT C runtime and its tests could be borrowed with attribution only.
- Tests written for a little-endian ASCII host must be checked for representation assumptions before they are used as MVS tests.

### Gaps
- The exact first commit date and how many tests are self-checking (return code vs. output comparison) were not checked.

## Digital Research PL/I-80 / PL/I-86 (Subset G)

### Takeaway
DR's compilers were the historical Subset G implementations on micros. Binaries and full manuals are readily available. No source release of the compiler was found (Kildall's CP/M source releases do not include PL/I). Useful only as a language-level reference (the manuals define a practical Subset G).

### Cited Findings
- Bitsavers has the PL/I-80 Language Manual (Dec 1980), PL/I-80 Applications Guide (Dec 1980), PL/I Language Reference Manual (Oct 1982) and Programmer's Guide (Dec 1982). — [bitsavers mirror: PL1-80 Language Manual](https://ftp.mirrorservice.org/sites/www.bitsavers.org/pdf/digitalResearch/pl1/PL1-80_Language_Manual_Dec80.pdf); [PL1 Language Reference Manual Oct82](https://ftp.mirrorservice.org/sites/www.bitsavers.org/pdf/digitalResearch/pl1/PL1_Language_Reference_Manual_Oct82.pdf); [Programmer's Guide Dec82](https://ftp.mirrorservice.org/sites/www.bitsavers.org/pdf/digitalResearch/pl1/PL1_Language_Programmers_Guide_Dec82.pdf)
- Binaries are distributed as abandonware. — [WinWorld: Digital Research PL/I Compiler 1.x](https://winworldpc.com/product/digital-research-pl-i-compiler/1x); [cpm.retropc.se DRI binaries](https://cpm.retropc.se/cpm/binary.html); [EDM2 wiki](https://www.edm2.com/index.php/Digital_Research_PL/I)
- Iron Spring notes that since DRI's demise PL/I "has not been readily available on personal computers". — [About Iron Spring](http://www.iron-spring.com/about.html)
- CHM and DRI source releases cover CP/M source, not the PL/I compiler. — [CHM: Early Digital Research CP/M Source Code](https://computerhistory.org/blog/early-digital-research-cpm-source-code/)

### Gaps
- No evidence of a PL/I-80/86 compiler source release. The "unofficial CP/M web site" (Caldera/Lineo) license covers DRI material in general; its exact scope regarding PL/I binaries was not verified.

## PL/C, SP/k, PLAGO (university S/360-370 compilers)

### Takeaway
PL/C is the famous free S/360/370 PL/I compiler, but its source is reported lost. Its object code may be run on simulators under an informal 2012 permission tied to the MTS archival effort. SP/k and PLAGO were small teaching subsets with no found source. None is a usable code base. PL/C's AFIPS paper on error-repairing compilation is a design reference.

### Cited Findings
- PL/C was "implemented in IBM 360 Assembly Language, with a heavy use of assembly language macros". It ran on S/360 and S/370 under OS/360 and HASP and was developed 1968–69. Regular use began September 1970. The final version was Release 7.6 (1977). — [Wikipedia: PL/C](https://en.wikipedia.org/wiki/PL/C)
- "As of 2022, the source for PL/C appears to have been lost"; in 2012 "permission was informally granted to run PL/C object code on IBM 360/370 simulators" as part of the MTS archival effort. — [Wikipedia: PL/C](https://en.wikipedia.org/wiki/PL/C)
- PL/CT (interactive, 1974/1976) was still a compiler, running under VM/CMS and TSO. — [Wikipedia: PL/C](https://en.wikipedia.org/wiki/PL/C)
- PL/C implemented "almost all of the large PL/I language" and never failed to compile a program (automatic error repair). Over 250 universities adopted it. — [Wikipedia: Cornell University Programming Language](https://en.wikipedia.org/wiki/Cornell_University_Programming_Language)
- Design paper: "PL/C: The design of a high-performance compiler for PL/I" (AFIPS). — [ACM DL](https://dl.acm.org/doi/pdf/10.1145/1478786.1478858)
- The MTS distribution (source, object, docs) is under CC BY 3.0, open to research since January 2012. — [Deep Blue: MTS Distribution](https://deepblue.lib.umich.edu/handle/2027.42/89588)
- SP/k (University of Toronto CSRG): a compatible teaching subset of PL/I, distributed for 360/370 and PDP-11. — [CACM 1977 "SP/k" abstract](https://cacmb4.acm.org/magazines/1977/5/11358-sp-k/abstract); PLAGO (Polytechnic Institute of Brooklyn) was a translator-interpreter for a PL/I subset. — [FOLDOC: PLAGO](https://foldoc.org/PLAGO)
- Jay Moseley's list of compilers available for MVS 3.8 includes only PL/I (F) 5.5 (from MVT 21.8f) and XPL (Flass, CBT #517) among PL/I-related items. — [Jay Moseley: Language Compilers Available for MVS 3.8](https://www.jaymoseley.com/hercules/compilers/list_of.htm)

### Inferences
- If a PL/C object deck exists in the MTS distribution, it would run on MTS rather than plain MVS 3.8j. Even so, it would be a behaviour reference only, never a code base.

### Gaps
- Not verified whether a PL/C object module is actually in the MTS D6.0 tapes or on bitsavers. The Wikipedia statement on the 2012 permission was not traced to its primary source.
- SP/k and PLAGO source availability: nothing found.

## Kednos PL/I (ex-DEC VAX PL/I) and other commercial lines

### Takeaway
Kednos PL/I descends from the Freiburghouse/Translation Systems line and is commercial and unmaintained since October 2016, with hobbyist licenses. It is not a source or code base. Liant/Micro Focus Open PL/I is commercial (not researched further).

### Cited Findings
- Kednos ceased trading in October 2016, and its PL/I compilers and runtimes will no longer be updated. Last releases: V3.8 (VAX) and V4.4/4.5 (Alpha). Kits stay downloadable for license holders, and hobbyist non-commercial licenses are still available. — [kednos.com](https://kednos.com/); [Kednos SPD](https://www.kednos.com/pli/docs/spd/spd.html); [Kednos Alpha release notes](https://kednos.com/pli/docs/AXP_RELEASE_NOTES/dpli_release_notes.html)

### Gaps
- Liant / Micro Focus Open PL/I licensing and status not researched (out of budget). Nothing suggests source availability.

## IBM compilers on Hercules (PL/I F, Optimizer)

### Takeaway
PL/I (F) V5.5 comes with the MVT 21.8f distribution and is installable on MVS 3.8j (also in TK5). The OS PL/I Optimizer (5668-910, part of OS PL/I V2) is an IBM licensed program. Its manuals are on bitsavers, but no community statement of a legal free distribution was found.

### Cited Findings
- PL/I (F) 5.5 from MVT 21.8f, with an installation archive and a verification jobstream. — [Jay Moseley: PL/I Compiler from MVT](https://www.jaymoseley.com/hercules/compilers/pli.htm)
- TK5 provides "access to the IBM PL/I (F) compiler". — [awesome-pli](https://github.com/ross-shk/awesome-pli)
- OS PL/I V2: the Optimizing Compiler and Library is licensed program 5668-910 (with 5668-909 including the Interactive Test Facility, and 5668-911 library only). The manuals are on bitsavers. — [bitsavers: OS PL/I V2 Problem Determination LY27-9528-0](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/LY27-9528-0_OS_PLI_Version_2_Problem_Determination_Dec87.pdf); [Computing History: Optimizer Installation Guide for MVS](https://www.computinghistory.org.uk/det/9668/IBM-OS-PL-I-Optimising-Compiler-Installation-Guide-for-MVS/)

### Inferences
- PL/I (F) on the target is the practical oracle for differential testing of a new compiler's output: compile the same test program with both and compare results on MVS. It also sets a natural minimum language level.
- Bitsavers' Optimizer and DOS PL/I manuals (for example SC33-0008-6) document the S/370 runtime conventions (PLIMAIN, DSA layout, ON-unit handling). They are a design reference for an MVS-compatible runtime ABI.

### Gaps
- No primary-source statement on the legal status of running the Optimizer on Hercules; hercules-390 / H390-MVS group archives were not reached in this pass.

## Test suites and corpora

### Takeaway
No formal public PL/I conformance suite (ANSI/NIST style) was found. Usable free sources: pli-llvm's ~450 MIT tests, PLI-2000's `/pl1` tests (GPL-3), Iron Spring samples and library source (LGPL), the Multics source (Multics License), the TeamPL/I source archive, and Rosetta Code.

### Cited Findings
- pli-llvm: ~450 categorised tests, MIT. — [GitHub ross-shk/pli-llvm](https://github.com/ross-shk/pli-llvm)
- PLI-2000: test sources in `/pl1`. — [GitHub Steadsoft/PLI-2000](https://github.com/Steadsoft/PLI-2000)
- PL/I Virtual Source Archive (games, data structures, text tools), numerical methods collection, Rosetta Code category PL/I. — [awesome-pli](https://github.com/ross-shk/awesome-pli) (pointing to http://www.teampli.net/plisrc.html, http://www.teampli.net/RobinV/numeric.htm, https://rosettacode.org/wiki/Category:PL/I)
- Iron Spring distribution: library sources (PL/I) and sample programs. — [Release Notes](http://www.iron-spring.com/readme_linux.html)
- Multics source, parseable "most if not all" by the pl1gcc parser. — [pl1gcc](https://pl1gcc.sourceforge.net/)

### Gaps
- Licenses of the TeamPL/I archive items and of Rosetta Code (GFDL 1.2) not individually verified. No NIST/ANSI PL/I validation suite located.

## Overall conclusions (inferences)
- **No permissively licensed, C-written, S/370-targeting PL/I compiler exists.** The closest code base is PLI-2000 (C, Subset G, GPL-3, x86 COFF). Everything else is closed (Iron Spring compiler, Kednos, DRI), written in PL/I (Multics), front-end only (pl1gcc), C++/LLVM (pli-llvm), or lost (PL/C).
- Recommended use: **design** from Freiburghouse/Multics, the Iron Spring runtime and PLI-2000; **tests** from pli-llvm (MIT) and the Multics corpus, plus differential testing against PL/I (F) 5.5 on the target; **scope** Subset G plus selected IBM "PL/I for MVS and VM" extensions, the same scope Iron Spring chose.
