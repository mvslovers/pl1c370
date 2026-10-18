# IBM PL/I compilers for OS/360, OS/VS1, OS/VS2 (SVS/MVS), PL/I (F) V5 to OS PL/I V2 (about 1966-1987): timeline and licensing

Method note: most dates below come from the title page ("Nth Edition (Month Year) ... This edition applies to ...") and the "Summary of Amendments" of IBM manuals on bitsavers. I downloaded the PDFs and ran pdftotext on them; the OCR text is quoted where it matters. **An edition date is a publication date. It is neither an announcement date nor a general-availability (GA) date.** It shows that a release existed by that date at the latest. I found no IBM announcement letters for any of these products (no Computerworld hits came up either), so almost all "announced" dates are missing. Gaps are marked.

## Q1: PL/I (F): versions, dates, last level, program numbers, OS/VS

### Takeaway
PL/I (F) shipped in five "versions", tied to OS/360 releases: Release 1 in 1966, the 4th version at OS Release 17/18, and the 5th version at OS Release 19/20 (c. 1970-71). Compiler 360S-NL-511 and library 360S-LM-512 were bundled components of OS/360, not program products. No later "version 6" exists. The successor products are the Optimizing and Checkout compilers (1971). TK5's "VERSION 5.5", with IDR dates of 1972-74, is a maintenance level of the 5th version that came with the last MVT releases (21.x). I found no source for an end-of-service or withdrawal date.

### Cited Findings
- Program Guide, 8th edition: "IBM System/360 Operating System PL/I (F) Programmer's Guide, Program Number 360S-NL-511" (OCR reads "360S-NL-Sll"), GC28-6594-7, Eighth Edition (January 1971), "applies to Release 20 of the IBM System/360 Operating System". Copyright line: "© Copyright International Business Machines Corporation 1966,1967,1968,1969,1970,1971". — [GC28-6594-7 (bitsavers)](https://bitsavers.trailing-edge.com/pdf/ibm/360/pli/GC28-6594-7_PLIf_pgGd_Jan71.pdf)
- PL/I Subroutine Library PLM, GY28-6801-6, 7th edition (June 1972), "Program Number 360S-LM-512" (OCR reads "3606-LM-512"), "applies to Release 18 ... and to all subsequent releases". It calls the library "a system component of IBM System/360 Operating System". Copyright 1966-1969. — [GY28-6801-6 (bitsavers)](https://bitsavers.trailing-edge.com/pdf/ibm/360/pli/GY28-6801-6_PL1_Subroutine_Library_Program_Logic_Manual_197206.pdf)
- Appendix C of GC28-6594-7, "Versions of the PL/I (F) Compiler", maps each version to a Programmer's Guide edition: 1st version = C28-6594-0, 2nd = -1, 3rd = -2, 4th = -3, 4th version at release 17 = -4, 5th version = -5. Edition -6 "documents the fifth version of the compiler with the improvements incorporated for Release 19 of the operating system". Sub-headings cover "Changes at Fourth Version, Release 17/18" and "Changes at Fifth Version, Release 19 / Release 20". — [GC28-6594-7](https://bitsavers.trailing-edge.com/pdf/ibm/360/pli/GC28-6594-7_PLIf_pgGd_Jan71.pdf)
- What each version added (same appendix):
  - 2nd version: RECORD I/O (READ/WRITE/REWRITE/DELETE; KEYED, DIRECT, etc.), and the PL1LFLG procedure.
  - 3rd version: the compile-time preprocessor, abbreviated options, INITIAL for STATIC arrays, PAGESIZE/LINESIZE, and object-code performance improvements.
  - 4th version: LOCATE I/O and list processing (BASED, POINTER, OFFSET, AREA, ALLOCATE/FREE), multitasking (CALL with TASK/EVENT/PRIORITY, under MVT), COBOL/FORTRAN data interchange, the STRINGRANGE condition, PL/I SORT, and the new options OPLIST/EXTDIC/MACDCR/NEST.
  - 4th version at Release 17: object-program optimizations and checkpoint/restart under PCP.
  - 5th version (Release 19/20): the language reference names teleprocessing support and keyword abbreviations as new in the fifth version; the Programmer's Guide says it was done "mainly for" optimization.
  — [GC28-6594-7](https://bitsavers.trailing-edge.com/pdf/ibm/360/pli/GC28-6594-7_PLIf_pgGd_Jan71.pdf); [GC28-8201-4 Language Reference, 5th ed. Dec 1972](https://bitsavers.trailing-edge.com/pdf/ibm/360/pli/GC28-8201-4_PLI_F_Language_Reference_Dec72.pdf)
- Multitasking needs "a PL/I (F) compiler and a PL/I subroutine library of version 4 level or later". — [GC28-6594-7](https://bitsavers.trailing-edge.com/pdf/ibm/360/pli/GC28-6594-7_PLIf_pgGd_Jan71.pdf)
- Sample listings in the Jan 1971 guide show the header "VERSION 5.0". — [GC28-6594-7](https://bitsavers.trailing-edge.com/pdf/ibm/360/pli/GC28-6594-7_PLIf_pgGd_Jan71.pdf)
- A German guide for "PL/1 F - OS/360 V 5.02" exists (title only seen, not read). — [fruehes.berlin PDF](https://www.fruehes.berlin/wp-content/uploads/2011/02/PL-1-F-OS-360-V-5.02.pdf)
- Wikipedia (a lead only, not verified against a primary source): "Release 1 shipped in 1966"; Release 2 brought RECORD I/O, Release 4 list processing, Release 5 DO-loop optimization. Its "Release n" matches IBM's "nth version". — [Wikipedia: PL/I](https://en.wikipedia.org/wiki/PL/I)
- Jay Moseley lists "OS 360 PL/I (F) - version designation: 5.5" among the compilers taken from the OS/MVT 21.8F distribution for use on MVS 3.8j. — [Moseley, Language Compilers Available for MVS 3.8](https://jaymoseley.com/hercules/compilers/list_of.htm)
- MVT 21.8F (1974) was the last MVT release. — [Dave Morton, IBM Mainframe Operating Systems Timeline v33 (on jaymoseley.com)](https://www.jaymoseley.com/hercules/downloads/pdf/$OSTL33.pdf)
- PL/I (F) also served as the reference that the newer products defined compatibility against. The optimizer's General Information (Dec 1972) describes its language as a superset "as implemented by version 5 of the PL/I (F) compiler". — [GC33-0001-2](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/GC33-0001-2_PLIgenInf_Jun73.pdf)

### Inferences
- "V5.5" (IDR 1972-11 and 1974-01, as measured on TK5) is a later maintenance level of the 5th version than "VERSION 5.0" (Jan 1971 guide) and "V5.02". It fits the MVT 21.x maintenance stream up to 21.8F (1974), which is where Moseley says the copy comes from. There was no 6th version: from 1971 IBM's PL/I development moved to the Optimizing and Checkout compilers.
- PL/I (F) was never a separately ordered product in the OS/VS era. OS/VS1 and OS/VS2 were SCPs, and language processors were not delivered with them (Morton's timeline lists Assembler, COBOL, RPG, PL/1 and FORTRAN as included only for OS/360 and DOS). On MVS 3.8j, hobbyists install it from the MVT 21.8F libraries, not from an MVS distribution tape. Whether IBM formally supported PL/I (F) under OS/VS is **not verified** (see gaps).

### Gaps
- No primary source found for GA dates of versions 1-5, except "Release 1 1966" (Wikipedia) and the edition dates above.
- **No source found for a withdrawal-from-marketing or end-of-service date for PL/I (F).**
- Not verified whether IBM officially supported PL/I (F) under OS/VS1/VS2/MVS 3.8. The OS/VS2 MVS 3.8 distribution apparently did not include it.
- The meaning of the ".5" in V5.5 (PTF level? a 21.x-specific modification?) is not documented in anything I read.
- I could not read the PL/I (F) PLM GY28-6800-5 (Dec 1971) because the bitsavers PDF is damaged (xref errors). It may hold the last PLM-level version information.

## Q2: OS PL/I Optimizing Compiler (5734-PL1), releases, OS, storage

### Takeaway
The Optimizing Compiler was a separately licensed Program Product, 5734-PL1. It came with a Resident Library (5734-LM4) and a Transient Library (5734-LM5), and all three were also orderable as composite package 5734-PL3. It was announced with System/370 in 1970, and the OS release existed by Sep 1971 (R1.0). The documented releases:

| Release | Earliest documented | Notes |
|---|---|---|
| R1.0 | Sep 1971 | |
| R1.2.0 | by Dec 1972 | |
| R1.2.2 | by Dec 1974 | |
| R1.3.0 | by Oct 1976 | |
| R3.1 (V1R3M1) | Jul 1979 | larger compiler storage |
| R4 | Dec 1980 / Sep 1981 | DBCS/GRAPHIC; **last release for OS/VS1** |
| R5.0 | Oct 1984 | MVS/XA support, 128K compiler; requires at least MVS/SP 1.3 |
| R5.1 | Mar/Sep 1985 | CICS 1.6.1 31-bit, VM/SP 3 |

R1 ran under MFT/MVT in 50-52K, and later under VS1/VS2. **R5.x no longer runs on plain OS/VS2 MVS 3.8.** It needs MVS/SP V1R3 or MVS/XA.

### Cited Findings
- Wikipedia (a lead): "Announced with IBM S/370 in 1970, it shipped first for the DOS/360 operating system in August 1971", shortly afterwards for OS/360. — [Wikipedia: PL/I](https://en.wikipedia.org/wiki/PL/I)
- **R1.0, Sep 1971.** OS PL/I Optimizing Compiler Programmer's Guide SC33-0006-0 (Sep 1971), "applies to Release 20.1 of IBM System/360 Operating System" — [SC33-0006-0](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC33-0006-0_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep71.pdf).
  - The System Information manual SC33-0026-0 (Sep 1971) gives the minimum core storage for "PL/I Optimizing Compiler (MFT)" as 51,200 bytes and for "(MVT)" as 53,248 bytes, Program Number 5734-PL1. — [SC33-0026-0](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC33-0026-0_PLI_Optimizing_Compiler_System_Info_Sep71.pdf)
  - The Language Reference SC33-0009-2 (Sep 1972) "applies to Release 1.0 of the OS PL/I Optimizing Compiler". The compilers "require an MFT or MVT version" of OS, and multitasking runs only under MVT. — [SC33-0009-2](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC33-0009-2_PLIoptRef_Sep72.pdf)
- **R1.2.0, by Dec 1972.** General Information GC33-0001-2, Third Edition (December 1972), "applies to Version 1, Release 2, Modification 0 of the OS PL/I Optimizing Compiler, Program Product 5734-PL1". CMS material applies to VM/370 Release 1 PLC5.
  - Machine minimum: a "System/360 Model 40 with ... 128K bytes"; the compiler "will operate in a 50K partition under MFT, in a 52K region under MVT, or in a 72K region with ... TSO. Under VS1 and VS2, the partition or region sizes are rounded up to a multiple of 64K."
  - Requires the universal instruction set, and runs on S/360 or S/370.
  — [GC33-0001-2](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/GC33-0001-2_PLIgenInf_Jun73.pdf)
- **R1.2.2, by Dec 1974.** Programmer's Guide SC33-0006-3, Third Edition (December 1974), "applies to Version 1 Release 2 Modification 2". It lists program numbers 5734-PL1, 5734-LM4 and 5734-LM5, "available as composite package 5734-PL3".
  - "The compiler is designed to operate under Release 20.1 of the IBM Operating System and under all subsequent releases ... also operates under Release 1.0 ... of CMS".
  - "An MFT, MVT, VS1, or VS2 version of the Operating System is required. ... PL/I multitasking facilities can be used only on an MVT or VS2 system."
  - "Programs translated by the optimizing compiler cannot be link-edited with object modules produced by the PL/I (F) compiler."
  — [SC33-0006-3](https://www.bitsavers.org/pdf/ibm/370/pli/SC33-0006-3_PLIpgmGde_Dec74.pdf)
- **R1.3.0, by Oct 1976.** The joint Checkout/Optimizing Language Reference GC33-0009-4, Fifth Edition (October 1976), applies to "Version 1, Release 3, Modification 0" of both the Checkout Compiler 5734-PL2 and the Optimizing Compiler 5734-PL1. — [GC33-0009-4](https://bitsavers.trailing-edge.com/pdf/ibm/360/pli/GC33-0009-4_PLI_Checkout_And_Opt_Compiler_Lang_Ref_Oct76.pdf)
- **R3.1, Jul 1979.** TSO User's Guide SC33-0029-3 (July 1979) "applies to version 1, Release 3, Modification 1". Its summary of amendments: "For Release 3, Modification 1, the storage size in which the compiler runs has been increased." The Programmer's Guide summary for JULY 1979 says the same. — [SC33-0029-3](https://bitsavers.trailing-edge.com/pdf/ibm/360/pli/SC33-0029-3_OS_PL1_Optimizing_Compiler_TSO_Users_Guide_Jul1979.pdf); [SC33-0006-7](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC33-0006-7_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep85.pdf)
- **R4, Dec 1980 / Sep 1981.**
  - Messages SC33-0027-5, Summary of Amendments "DECEMBER 1980 ... For Release 4, several new messages ... for the PL/I Extended Graphic Character Set support". — [SC33-0027-5](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC33-0027-5_OS_PLI_Optimizing_Compiler_Messages_Feb82.pdf)
  - Programmer's Guide summary "SEPTEMBER 1981": GRAPHIC compiler option and ENVIRONMENT option, and the new OS and DOS Language Reference GC26-3977.
  - Same guide, October 1984 note: "Release 4 of the PL/I Optimizing Compiler and libraries is the last release to support VS1."
  — [SC33-0006-7](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC33-0006-7_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep85.pdf)
- **R5.0, Oct 1984.** The Programmer's Guide summary for "OCTOBER 1984" adds MVS/XA support, the ISAINC/HEAP/TASKHEAP run-time options, and IMS/VS 31-bit support. "Release 5.0 will not support VM/CMS and CICS/VS. Users ... will continue to be supported on OS PL/I Release 4.0." "For Release 5.0, the storage size of the compiler is increased to 128K." — [SC33-0006-7](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC33-0006-7_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep85.pdf); also the Messages summary dated March 1985: [SC33-0027-5](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC33-0027-5_OS_PLI_Optimizing_Compiler_Messages_Feb82.pdf)
- **R5.1, Mar/Sep 1985.** Messages: "Release 5.1 adds support for VM/SP Release 3, CICS/VS Release 1.6.1, and VM/PC Release 1.1" (summary dated MARCH 1985, TNL SN26-8314 of 30 Mar 1985). — [SC33-0027-5](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC33-0027-5_OS_PLI_Optimizing_Compiler_Messages_Feb82.pdf)
  - Programmer's Guide SC33-0006-7, Seventh Edition (September 1985), "applies to Releases 4.0, 5.0, and 5.1". — [SC33-0006-7](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC33-0006-7_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep85.pdf)
- **R5.0/5.1 system requirements** (Installation Guide for MVS SC26-4121-1, Second Edition, September 1985, "applies to Releases 5.0 and 5.1"):
  - Operating system: "OS/VS2 (MVS) Version 3 Release 8 with MVS/SP Version 1 Release 3", or "MVS/SP Version 2 Release 1, with MVS/XA DFP Version 1 Release 1", or VM/SP Release 3.
  - Machine: "An IBM System/370, 303x, 308x, or 43xx processor".
  - An installation-time option 'ARCH' selects 24-bit-only or 31/24-bit.
  - Delivery: an SMP-format tape, 1600 or 6250 bpi.
  - "You may not install the resident library without the transient library; you may not install the compiler without both transient and resident libraries."
  - Also needs IEAXPSIM in the link list on MVS/SP 1.3 when the hardware has no extended floating point, and Assembler XF to change option defaults.
  — [SC26-4121-1](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC26-4121-1_PLI_Release_5_Installation_Guide_for_MVS_Sep1985.pdf)
- Execution Logic SC33-0025-3, Fourth Edition (Sep 1985), applies to Release 5.1 of 5734-PL1, LM4, LM5 and PL3. — [SC33-0025-3](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC33-0025-3_PLI_Optimizing_Compiler_Execution_Logic_Sep85.pdf)
- Product IDs for "OS PL/I 1.x": 5734-PL1 (compiler), 5734-LM4 (resident library), 5734-LM5 (transient library), 5734-PL3 (compiler and libraries), 5734-PL2 (checkout compiler). — [lookupmainframesoftware.com](https://lookupmainframesoftware.com/soft_detail/dispsoft/1347)

### Inferences
- For a plain MVS 3.8j (no MVS/SP), the newest optimizer level that formally fits is **R4.0** (c. 1980/81). R5.0 and later list MVS/SP V1R3 as the minimum. Whether R4 or R3.1 actually runs on base 3.8 is not documented in what I read. Their manuals list "MFT, MVT, VS1, or VS2" (seen in the R1.2.2 edition), which suggests that plain VS2 was acceptable up to R4.
- The storage need grew over time: about 50-52K (R1), then larger at R3.1 (amount not stated), then 128K (R5.0).

### Gaps
- No announcement letters or GA dates for any release. The years above are documentation dates.
- No sources for a "R2.x" beyond V1R2M0/M2, or for when IBM began calling them "Release 3.0/4.0/5.0" instead of V1R3. The renaming seems to fall between 1979 and 1980.
- R4's operating system list (does it require MVS/SE or MVS/SP, or does plain VS2 R3.8 suffice?) is not verified, since no R4 install guide is on bitsavers.
- The exact R3.1 storage increase is not given in the texts I read.

## Q3: OS PL/I Checkout Compiler (5734-PL2)

### Takeaway
A separate interactive, TSO-oriented interpretive compiler for debugging, using the same language level as the optimizer. It was announced in 1970 (Aug 1970 per Wikipedia) and released in November 1971. It shared the Transient Library 5734-LM5, and reached V1R3M0 by May 1975.

### Cited Findings
- General Information GC33-0003-0, First Edition (July 1970), Program Number 5734-PL2: "a planning aid only ... prior to the availability of ... OS PL/I Checkout Compiler, Program Product 5734-PL2; OS PL/I Transient Library, Program Product 5734-LM5". — [GC33-0003-0](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/GC33-0003-0_PLI_Checkout_Compiler_General_Info_Jul70.pdf)
- "The PL/I Checkout Compiler, released by IBM in November 1971 and developed at the Hursley Laboratories near Winchester ..." — [The Computer Journal 15(2):99, "Conversational Compiler for Full PL/I"](https://academic.oup.com/comjnl/article-pdf/15/2/99/1003014/15-2-99.pdf)
- Wikipedia: "announced in August 1970". — [Wikipedia: PL/I](https://en.wikipedia.org/wiki/PL/I)
- Checkout Compiler Messages SC33-0034-2, Third Edition (May 1975), "applies to Version 1, Release 3, Modification 0 of the OS PL/I Checkout Compiler, Program Product 5734-PL2". It incorporates TNLs of July 1974 and March 1975. — [SC33-0034-2](https://bitsavers.trailing-edge.com/pdf/ibm/360/pli/SC33-0034-2_OS_PL1_Checkout_Compiler_Messages_Oct1976.pdf)
- The joint Language Reference of Oct 1976 covers Checkout V1R3M0 together with Optimizer V1R3M0. — [GC33-0009-4](https://bitsavers.trailing-edge.com/pdf/ibm/360/pli/GC33-0009-4_PLI_Checkout_And_Opt_Compiler_Lang_Ref_Oct76.pdf)
- The OS PL/I V2 PLM (1987) still carries "Reserved for the Checkout Compiler" fields in control blocks. — [LY27-9528-0](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/LY27-9528-0_OS_PLI_Version_2_Problem_Determination_Dec87.pdf)

### Gaps
- Releases after V1R3, and the withdrawal date, were not found. One search snippet (softpanorama) mentions a debugging-oriented variant "withdrawn in the 1990s", which I did not verify.

## Q4: Resident Library (5734-LM4) and Transient Library (5734-LM5)

### Takeaway
LM4 holds the routines link-edited into the load module. LM5 holds the routines loaded dynamically at run time, and the Checkout Compiler uses it too. Both are separate licensed program products. Optimizer-compiled object code needs LM4 to link-edit and LM5 to run. Compiled load modules run with only LM5 present, but LM5 is itself licensed.

### Cited Findings
- Separate PLMs exist for each: PL/I Resident Library PLM LY33-6011-1 (Oct 1976) and PL/I Transient Library PLM LY33-6012-1 (Oct 1976). — [bitsavers 370/pli listing](http://www.bitsavers.org/pdf/ibm/370/pli/)
- Release compatibility rule: "Different release levels of the optimizing compiler and libraries will produce compatible executions, provided that: the release and service levels of the transient library are equal to or greater than those of the resident library; the release and service levels of the resident library are equal to or greater than those of the compiler." — [SC26-4121-1](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC26-4121-1_PLI_Release_5_Installation_Guide_for_MVS_Sep1985.pdf)
- The libraries were orderable separately (LM5, LM4, PL1) or as composite 5734-PL3, and "You may not install the resident library without the transient library". — [SC26-4121-1](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC26-4121-1_PLI_Release_5_Installation_Guide_for_MVS_Sep1985.pdf)
- The Checkout Compiler was planned with only the Transient Library (5734-PL2 + 5734-LM5). — [GC33-0003-0](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/GC33-0003-0_PLI_Checkout_Compiler_General_Info_Jul70.pdf)
- The Execution Logic manual has a section on "Execution When Using the Shared Library" (the library in the link pack area). — [SC33-0025-3](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC33-0025-3_PLI_Optimizing_Compiler_Execution_Logic_Sep85.pdf)
- Optimizer objects cannot be link-edited with PL/I (F) objects. PL/I (F) uses the IHE* library (360S-LM-512); the optimizer uses its own IBM* library, as the module name IBMBEER in SC26-4121-1 shows. — [SC33-0006-3](https://www.bitsavers.org/pdf/ibm/370/pli/SC33-0006-3_PLIpgmGde_Dec74.pdf); [SC26-4121-1](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC26-4121-1_PLI_Release_5_Installation_Guide_for_MVS_Sep1985.pdf)

### Inferences
- A load module produced by the optimizer and link-edited with LM4 contains resident-library code (IBM copyrighted), and at run time it dynamically loads LM5 modules. So TK5's SYS1.PL1LIB (IHE*) cannot stand in for either library. Running optimizer output on a hobbyist system therefore requires the licensed libraries as well.

### Gaps
- I found no statement of which IBM* modules belong to LM4 and which to LM5.

## Q5: OS PL/I Version 2 (5668-909/910/911)

### Takeaway
OS PL/I Version 2 Release 1 existed by December 1987 (first edition of the Problem Determination manual), with V2R2 by Oct 1988. Program numbers:
- 5668-909: compiler, library and Interactive Test Facility
- 5668-910: compiler and library
- 5668-911: library only

All were "Licensed Programs" with "Restricted Materials of IBM" notices. Its copyright line ("1985, 1987") suggests development from 1985. I found no primary source on whether V2 still supported MVS/370 (non-XA) or required MVS/XA, so that remains unverified.

### Cited Findings
- "First Edition (December 1987). This edition applies to Version 2 Release 1 of the following licensed programs: OS PL/I Optimizing Compiler, Library, and Interactive Test Facility, Licensed Program 5668-909; OS PL/I Optimizing Compiler and Library, Licensed Program 5668-910; OS PL/I Library Only, Licensed Program 5668-911". Also "Restricted Materials of IBM / Licensed Materials - Property of IBM ... © Copyright IBM 1985, 1987". — [LY27-9528-0](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/LY27-9528-0_OS_PLI_Version_2_Problem_Determination_Dec87.pdf)
- OS PL/I Version 2 General Information, Release 2, GC26-4313-3, Fourth Edition (October 1988), for 5668-909, 5668-910 and 5668-911. — [Computing History catalogue CH10737](https://www.computinghistory.org.uk/det/10737/IBM-OS-PL-I-Version-2-General-Information-Release-2/)
- Product IDs 5668-909/910/911 for "OS PL/I 2.x". The page also says "Support ended 2003", without saying for which release. — [lookupmainframesoftware.com](https://lookupmainframesoftware.com/soft_detail/dispsoft/1347)
- Later product, one line only: IBM PL/I for MVS & VM, Release 1.1, manuals dated June 1995. — [bitsavers 370/pli listing](http://www.bitsavers.org/pdf/ibm/370/pli/)

### Inferences
- GC26-4313-3 is a fourth edition dated Oct 1988, so the earlier editions (-0 to -2) probably fall in 1986-88. That fits a V2R1 announcement around 1986-87 (unverified).
- Given that the V1 R5.x line already ran on MVS/SP V1R3 (MVS/370) as well as XA, it is plausible that V2R1 also supported MVS/370 with MVS/SP. This is **not verified**. In any case, plain MVS 3.8 without MVS/SP would not be a supported base.

### Gaps
- No announcement letter, GA date or system requirements found for V2R1. The V2 General Information (GC26-4313-0) and Installation Guide (SC26-4311) are not online in anything I could access.

## Q6: DOS PL/I Optimizing Compiler and PL/I (D) (for completeness)

### Cited Findings
- DOS PL/I Optimizing Compiler 5736-PL1, DOS Resident Library 5736-LM4 and DOS Transient Library 5736-LM5. These are named in the planning manual "PL/I Optimizing Compiler General Information", First Edition (March 1970), which describes itself as "a planning aid only ... prior to the availability of" those products. (Note: the bitsavers filename GC33-0004-0 implies the OS optimizer, but the content is the DOS one.) — [GC33-0004-0](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/GC33-0004-0_PLI_Optimizing_Compiler_General_Info_Mar70.pdf)
- Shipped first for DOS in August 1971 (per Wikipedia). — [Wikipedia: PL/I](https://en.wikipedia.org/wiki/PL/I)
- DOS PL/I Programmer's Guide for Release 6.0, SC33-0008-6 (May 1984). — [bitsavers 370/pli listing](http://www.bitsavers.org/pdf/ibm/370/pli/)
- PL/I (D): "using 16 kilobytes of memory, was developed by IBM Germany for the DOS/360 low end operating system" and "shipped within a year of PL/I F". DOS PL/I (D) Programmer's Guide GC24-9005-6 (Apr 1971). — [Wikipedia: PL/I](https://en.wikipedia.org/wiki/PL/I); [bitsavers 360/pli listing](http://www.bitsavers.org/pdf/ibm/360/pli/)

## Q7: Licensing status

### Takeaway
PL/I (F) and its library were components of OS/360 (360S- numbers) and were supplied without a separate charge. OS/360 is treated as distributable because it was "not protected by copyright at the time of distribution", according to an assurance IBM gave via its agents, as reported on cbttape.org. TK4-/TK5 follow that community consensus. The PL/I (F) manuals do carry IBM copyright notices, though. Whether copyright notices exist in the load modules or on the tapes has not been checked here.

The Optimizing Compiler, Checkout Compiler and Libraries (5734-), and OS PL/I V2 (5668-, "Licensed Program", "Restricted Materials of IBM"), are post-unbundling licensed program products. I found no source showing that IBM ever released any of them for free use. So between PL/I (F) V5.5 and the mid-1980s **there is no legitimately free newer IBM PL/I compiler for MVS 3.8j.**

### Cited Findings
- On OS/360: "The IBM Corporation, through its agents, has assured me that this material may be distributed freely, since it was not protected by copyright at the time of distribution." (Rick Fochtman) — [cbttape.org: Download OS/360](https://cbttape.org/os360.htm)
- Hercules FAQ: OS/360, DOS/360, DOS/VS, MVS 3.8, VM/370 and TSS/370 are "widely believed to be either public domain or 'copyrighted software provided without charge.'" — [Hercules FAQ](http://www.hercules-390.eu/hercfaq.html)
- The copyright status of MVS 3.8 is "less clear" than OS/360's. According to IBM's lawyers, tapes without IBM copyright notices on the label are not considered copyrighted, and the May 2000 MVS distribution consists of the tapes that do not contain copyrighted material. — search-result summary of [Hercules FAQ (hercules-390.org)](http://www.hercules-390.org/hercfaq.html). This is a summary only; I did not re-read the passage itself.
- Morton calls plain MVS 3.8J "in the Public Domain". He defines "Program Product / System Product / Licensed Product" as "A product which may be licensed from IBM for a fee ... They must be obtained from IBM, and a license is required from IBM to run them." — [Morton timeline v33](https://www.jaymoseley.com/hercules/downloads/pdf/$OSTL33.pdf)
- Unbundling, June 1969: after the announcement, new IBM programs were classed as either System Control Programming or Program Products, and IBM catalogs list "Type I, II, III, IV and Prior Use Programs". — search summary of [Computer History Museum, "Software Becomes a Product"](https://www.computerhistory.org/revolution/story/172) and [IBM GC20-1601-10 Catalog of Programs, Jan 1971](https://kuiper.mirrorservice.org/sites/www.bitsavers.org/pdf/ibm/1401/GC20-1601-10_Catalog_Of_Programs_Jan71.pdf). I did not read either directly.
- The PL/I (F) manuals do carry copyright notices: Programmer's Guide "© Copyright IBM 1966-1971"; Library PLM "Copyright IBM 1966-1969". — [GC28-6594-7](https://bitsavers.trailing-edge.com/pdf/ibm/360/pli/GC28-6594-7_PLIf_pgGd_Jan71.pdf); [GY28-6801-6](https://bitsavers.trailing-edge.com/pdf/ibm/360/pli/GY28-6801-6_PL1_Subroutine_Library_Program_Logic_Manual_197206.pdf)
- The optimizer manuals label the products "Program Product", and the 1985 manuals carry "© Copyright IBM 1985". OS PL/I V2: "Licensed Program", "Restricted Materials of IBM, Licensed Materials - Property of IBM". — [SC26-4121-1](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/SC26-4121-1_PLI_Release_5_Installation_Guide_for_MVS_Sep1985.pdf); [LY27-9528-0](https://bitsavers.trailing-edge.com/pdf/ibm/370/pli/LY27-9528-0_OS_PLI_Version_2_Problem_Determination_Dec87.pdf)
- MVS 3.8 itself ships only the Assembler; other compilers are taken from the MVT 21.8F distribution. — [Moseley compilers page](https://jaymoseley.com/hercules/compilers/list_of.htm)
- A community group exists: "PL/I(F) and MVS 3.8j" at groups.io/g/pl1f-and-mvs38j. Its content could not be fetched (HTTP 402). — [groups.io](https://groups.io/g/pl1f-and-mvs38j/)

### Inferences
- The basis for treating PL/I (F) as free rests on (a) it being part of OS/360 (a 360S- component, not a 5734-/5736- program product) and (b) the reported IBM assurance about OS/360's copyright status. It does not rest on the absence of copyright notices in the manuals, because those are present. A notice on the documentation does not settle the status of the program material. The community relies on IBM's statements about tape contents. I found no formal IBM public-domain declaration specific to PL/I (F).
- PL/I (F) was released before June 1969 and maintained afterwards, with the 4th and 5th versions coming 1968-1971. It stayed in the OS/360 SCP stream; it was not re-classified as a program product.
- Optimizer, Checkout, the 5734 libraries and V2 are all licensed. Running them on Hercules without an IBM licence is not legitimate. Copies circulating in the hobby are not a legal basis.

### Gaps
- No Hercules-list or groups.io discussion about the availability of the Optimizing Compiler was retrievable in this session (groups.io is paywalled to the fetcher, and the archives were not searchable). The claim that "the community says the optimizer is not available" therefore remains unsourced here.
- No primary IBM document classifying PL/I (F) as Type I/Prior Use was read. The program catalog of the period (GC20-1601-xx, or the S/360 equivalent) would be the place to check.
- I did not check whether the PL/I (F) load modules (IEM*/IHE*) contain embedded copyright strings.
