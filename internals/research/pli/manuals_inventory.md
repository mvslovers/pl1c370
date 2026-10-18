# IBM PL/I manuals (PL/I (F) and OS PL/I Optimizing/Checkout, to ~1987): inventory and online availability

Method: the bitsavers file list was taken from `https://bitsavers.org/pdf/IndexByDate.txt` (8.58 MB, newest entry 2026-10-09), grepped for every PL/I-related file under `ibm/`. Every bitsavers path in this file (53 paths extracted mechanically from the file, plus 16 cited in prose) was checked with an HTTP HEAD request on 2026-10-11/12 and answered 200. About 25 of them were also downloaded and their title pages read. bitsavers answers **403 to any request without a browser User-Agent**, files and directory listings alike. With a browser UA the directory listings work. The listings of `https://bitsavers.org/pdf/ibm/360/pli/` (36 files) and `…/370/pli/` (27 files) were diffed against the index entries and are identical, so nothing was added after the index date. Mirrors such as `bitsavers.trailing-edge.com` and `www.mirrorservice.org/sites/www.bitsavers.org/` serve the same paths. Editions, dates and "applies to" lines in the tables come from the title/edition pages extracted with `pdftotext`, unless a row says "(from filename)". The value column is my own judgment for someone writing a PL/I (F)-compatible cross-compiler and runtime.

Abbreviations: BS = `https://bitsavers.org/pdf/`. bitsavers files are mirrored on archive.org as items named `bitsavers_ibm…`. Three were confirmed via the archive.org search API (the rest were not checked): `bitsavers_ibm360pliGLogicManual197112_25560440` (GY28-6800-5), `bitsavers_ibm360pliGneLibraryProgramLogicManual197206_12649970` (GY28-6801-6) and `bitsavers_ibm360pliG71_38733777` (GC28-6594-7). A search of archive.org for PL/I titles outside the bitsavers mirror found no further IBM PL/I manuals. **vtda.org**: the only PL/I item is `https://vtda.org/docs/computing/IBM/Mainframe/SupportSoft/PLI/C28-6571-3_IBM_PLI_Language_Specs.pdf` (the 1966 language specification, also on bitsavers). The directory listing was checked on 2026-10-12; vtda's other IBM/Mainframe subdirectories were browsed and hold no PL/I compiler manuals. The **Centre for Computing History** (computinghistory.org.uk) has physical copies only (see Gaps).

## Inventory: what exists and what is online

### Takeaway
Bitsavers has nearly the whole PL/I (F) set a compiler writer needs. That includes the language reference, the programmer's guide (with implementation conventions and all messages), the **compiler PLM (GY28-6800-5, 1971)**, the **library PLM (GY28-6801-6, 1972)** and the computational subroutines. For the OS Optimizing Compiler, bitsavers has the language reference, programmer's guide, messages, execution logic, system info, TSO/CMS guides and general information. **The OS Optimizing Compiler's PLM (LY33-6007) and the OS Resident/Transient Library PLMs (LY33-6008/-6009) are not online.** The LY33-6010/6011/6012 PLMs on bitsavers are the **DOS** versions, even though two of the filenames do not say "DOS".

### Cited Findings

#### A. PL/I (F) (OS/360 component, program numbers 360S-NL-511 compiler, 360S-LM-512 library)

| Title | Order no. / edition | Date | Applies to | URL (verified 200) | Value |
|---|---|---|---|---|---|
| PL/I (F) Language Reference Manual | GC28-8201-4 (5th ed., reprint of -3 + TNLs GN33-6019, GN33-6021) | Dec 1972 | OS Release 19 and later; contains Section K "Data Mapping" | BS`ibm/360/pli/GC28-8201-4_PLI_F_Language_Reference_Dec72.pdf` | **High** (language definition, data mapping) |
| PL/I (F) Language Reference Manual, **OCR, text-searchable** | GC28-8201-2 (3rd ed.), 436 pp. | Oct 1969 | OS Release 18; Version 5 language. Older than -4: check a finding against -4 before relying on it | `https://www.prycroft6.com.au/misc/download/GC28-8201-2_PLIrefMan_Oct69OCR2009.pdf` (Greg Price, OCR 2009) | **High** for lookups (pdftotext works; -4 is page images) |
| PL/I (F) Language Reference Manual, cover and contents only | GC28-8201-3 (4th ed.) | Jun 1970 | | `https://www.lola.org.uk/docs/` ("PL-1 (F) Language Reference Manual (4th Edition)") | Low |
| System/360 PL/I Reference Manual | C28-8201-1 | Jan 1969 (from filename) | earlier PL/I (F) versions | BS`ibm/360/pli/C28-8201-1_PLIrefMan_Jan69.pdf` | Low–medium (older edition) |
| PL/I (F) Programmer's Guide | GC28-6594-7 (8th ed., major revision of -6 + TNL GN33-6016), 519 pp. | Jan 1971 | OS Release 20; Version 5. Contains App. C "Versions of the PL/I (F) compiler", App. E library subroutines, App. H compiler data sets, App. I "ON, RETURN and user codes", **App. J "Implementation conventions"**, **App. K "Diagnostic messages"** (IEMnnnnI and IHEnnnI messages; 62 mentions of "dope vector", 40 of "pseudo-register") | BS`ibm/360/pli/GC28-6594-7_PLIf_pgGd_Jan71.pdf` | **High** (JCL, options, listings, messages, conventions) |
| PL/I (F) Programmer's Guide | C28-6594-4 | Nov 1968 (from filename) | earlier version | BS`ibm/360/pli/C28-6594-4_PL1_F_Programmers_Guide_Nov68.pdf` | Low |
| **PL/I (F) Compiler Program Logic Manual** | **GY28-6800-5** (6th ed., minor revision of Y28-6800-4 + TNLs GN33-6018, GY33-6017), 528 pp. | Dec 1971 | OS Release 20 (program 360S-NL-511). Phase-by-phase design; ~850 IEM references, 225 mentions of "dope vector", 203 of "DSA" | BS`ibm/360/pli/GY28-6800-5_PL1_F_Program_Logic_Manual_197112.pdf` | **High** (compiler internals, phases IEMAA…) |
| PL/I (F) PLM, older editions | Y28-6800-1 (Sep 1966), Y28-6800-3 (Mar 1968); TNLs Y33-6002 (May 1968), Y33-6004 (Nov 1968) (from filenames) | 1966–68 | earlier versions | BS`ibm/360/pli/Y28-6800-1_PL1(F)_PLM_Sep66.pdf`, `…/Y28-6800-3_PL1(F)_PLM_Mar68.pdf`, `…/Y33-6002_Y28-6800upd_May68.pdf`, `…/Y33-6004_Y28-6800upd_Nov68.pdf` | Low (history only) |
| **PL/I Subroutine Library Program Logic Manual** | **GY28-6801-6** (7th ed., minor revision of -05 + TNLs GN33-6017, GY33-6018), 264 pp. | Jun 1972 | OS Release 18 and later (program 360S-LM-512). Covers "the relationships between the code produced by the PL/I (F) compiler, the PL/I Library modules and the control program" (94 mentions of "dope vector", 108 of "pseudo-register", 204 of "DSA"). Restricted distribution | BS`ibm/360/pli/GY28-6801-6_PL1_Subroutine_Library_Program_Logic_Manual_197206.pdf` | **High** (runtime ABI, IHE* modules, PRV, condition handling) |
| PL/I Subroutine Library PLM, older edition | Y28-6801-4 | Nov 1968 (from filename) | earlier | BS`ibm/360/pli/Y28_6801-4_PL1subrPLM_Nov68.pdf` (the filename has an underscore after Y28) | Low |
| PL/I Subroutine Library: Computational Subroutines | C28-6590-0 | 1966 | the library's computational subroutines: algorithms, accuracy, timing | BS`ibm/360/pli/C28-6590-0_PLIcompuSbr_1966.pdf` | Medium (math library algorithms) |
| PL/I (F) Version 5 Planning Guide | C33-0002-0 | Mar 1969 | planning aid before Version 5 shipped: language changes, optimisation, storage use, compatibility with earlier versions | BS`ibm/360/pli/C33-0002-0_PLI(F)plGd_Mar69.pdf` | Medium (V5 delta) |
| PL/I Language Specifications | C28-6571-1 (Jul 1965), C28-6571-3 (Jul 1966); Y33-6003-0 (Mar 1968) (from filenames) | 1965–68 | pre-(F) language specification | BS`ibm/360/pli/C28-6571-1_PL_I_Language_Specifications_Jul65.pdf`, `…/C28-6571-3_PL_I_Language_Specifications_Jul66.pdf`, `…/Y33-6003-0_PL1LangSpecMar68.pdf` | Low–medium |
| NPL Technical Report | 320-0908 | Dec 1964 | language origins | BS`ibm/360/pli/320-0908_NPL_Technical_Report_Dec64.pdf` | Low |
| PL/I Primer; Subset RM; guides for FORTRAN users and for commercial programmers | C28-6808-0; C28-8202-0; SC20-1637-3; SC20-1651-1 | 1965–68 | tutorials | BS`ibm/360/pli/C28-6808-0_PLIprimer_1965.pdf`, `…/C28-8202-0_PLIsubsetRM_1967.pdf`, `…/SC20-1637-3_A_Guide_to_PL_I_for_FORTRAN_Users_May68.pdf`, `…/SC20-1651-1_A_Guide_to_PL_I_for_Commercial_Programmers_Apr68.pdf` | Low (test programs) |
| PL/I Subset Language PLM Vol. 1 and Vol. 3 | Y33-9010-0, Y33-9012-0 | 1967 | the (D)/subset compiler, not (F) | BS`ibm/360/pli/Y33-9010-0_PLI_Subset_Language_PLM_Volume1_1967.pdf`, `…/Y33-9012-0_PLI_Subset_Language_PLM_Volume3_1967.pdf` | Low–medium (a second compiler design for comparison) |

Related on bitsavers (other operating systems). These are useful for comparison because TSS PL/I and DOS PL/I are relatives of PL/I (F):
- TSS/360 PL/I: Language Reference GC28-2045-1, Library Computational Subroutines GC28-2046-1 (Jun 1970), Programmer's Guide GC28-2049-1 (Sep 1971), Planning GC28-2050-0, **Compiler PLM GY28-2051-0 (Jun 1970)**, **Subroutine Library PLM GY28-2052-0 (Jun 1970)**. All under BS`ibm/360/tss/` (filenames from the index; GY28-2052-0 checked with HEAD). Value: medium.
- DOS PL/I (D): GC24-9005-5 (Sep 1970, BS`ibm/360/dos/pl1/`), GC24-9005-6 (Apr 1971, BS`ibm/360/pli/`). Model 20 PL/I C33-6007-1 (BS`ibm/360/model20/`). FORTRAN-to-PL/I translator GC33-2002-2 and PLM Y33-7000-0, ALGOL-to-PL/I PLM Y33-7006-0. CALL/360 PL/I System Manuals GY20-0567..0570 (BS`ibm/360/os/call_360/`). Value: low.
- IBM education material: I0103 PL/I Independent Study Textbooks 1/2 (1980). GF20-0015/-0018/-0019/-0020 list-processing booklets (BS`ibm/360/pli/`). Low.

#### B. OS PL/I Optimizing Compiler (5734-PL1) and libraries (Resident 5734-LM4, Transient 5734-LM5)

| Title | Order no. / edition | Date | Applies to | URL | Value |
|---|---|---|---|---|---|
| General Information | GC33-0004-0 | Mar 1970 (from filename) | pre-release | BS`ibm/370/pli/GC33-0004-0_PLI_Optimizing_Compiler_General_Info_Mar70.pdf` | Low |
| General Information | GC33-0001-2 (3rd ed.) | Dec 1972 (filename says Jun73) | has "Compatibility with the (F) compiler" and App. B "Compatibility with (F)" | BS`ibm/370/pli/GC33-0001-2_PLIgenInf_Jun73.pdf` | Medium (the F-to-Optimizer differences) |
| Language Reference | SC33-0009-2 (3rd ed., reprint of -1 + SN33-6036) | Sep 1972 | Release 1.0 | BS`ibm/370/pli/SC33-0009-2_PLIoptRef_Sep72.pdf` | Medium–high |
| Checkout and Optimizing Compilers: Language Reference | GC33-0009-3 | Jul 1974 (from filename) | not downloaded; release level not read (presumably V1R2) | BS`ibm/370/pli/GC33-0009-3_OS_PLI_Language_Reference_Jul74.pdf` | Medium–high |
| Checkout and Optimizing Compilers: Language Reference | GC33-0009-4 (5th ed., major revision) | Oct 1976 | Checkout and Optimizing V1R3M0 | BS`ibm/360/pli/GC33-0009-4_PLI_Checkout_And_Opt_Compiler_Lang_Ref_Oct76.pdf` (filed under **360**/pli) | Medium–high |
| Programmer's Guide | SC33-0006-0 (1st ed.) | Sep 1971 | Release 1, OS Release 20.1 | BS`ibm/370/pli/SC33-0006-0_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep71.pdf` | Medium |
| Programmer's Guide | SC33-0006-3 (3rd ed.) | Dec 1974 | V1R2M2 | BS`ibm/370/pli/SC33-0006-3_PLIpgmGde_Dec74.pdf` | Medium |
| Programmer's Guide | SC33-0006-7 (7th ed., major revision of -6) | Sep 1985 | Releases 4.0, 5.0, 5.1 | BS`ibm/370/pli/SC33-0006-7_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep85.pdf` | Medium |
| **Execution Logic** | SC33-0025-2 | Apr 1973 | V1R2M0 | BS`ibm/370/pli/SC33-0025-2_PLIexLgic_Apr73.pdf` | **High** (runtime model: DSA, TCA, condition handling; nearest to the "how compiled code calls the library" question) |
| **Execution Logic** | SC33-0025-3 (4th ed., major revision + SN26-8171, SN33-6173) | Sep 1985 | Release 5.1 | BS`ibm/370/pli/SC33-0025-3_PLI_Optimizing_Compiler_Execution_Logic_Sep85.pdf` | **High** |
| System Information (installation/sysgen) | SC33-0026-0 (1st ed.) | Sep 1971 | Release 1, OS 20.1 | BS`ibm/370/pli/SC33-0026-0_PLI_Optimizing_Compiler_System_Info_Sep71.pdf` | Low–medium |
| Messages | SC33-0027-2 (3rd ed.) | Jun 1973 | V1R2 | BS`ibm/370/pli/SC33-0027-2_PLIcmsMsg_Jun73.pdf` (the "cms" in the filename is misleading: this is the Messages book) | Medium |
| Messages | SC33-0027-5 (6th ed.) | Feb 1982 | Release 5.1 | BS`ibm/370/pli/SC33-0027-5_OS_PLI_Optimizing_Compiler_Messages_Feb82.pdf` | Medium |
| TSO User's Guide | SC33-0029-3 | Jul 1979 | V1R3M1 | BS`ibm/360/pli/SC33-0029-3_OS_PL1_Optimizing_Compiler_TSO_Users_Guide_Jul1979.pdf` (filed under 360) | Low–medium |
| CMS User's Guide | SC33-0037-1 (Jun 1973); SC33-0037-3 (Oct 1976) | | CMS | BS`ibm/370/pli/SC33-0037-1_PLIcmsUG_Jun73.pdf`; BS`ibm/370/CMS/pli/SC33-0037-3_OS_PLI_Optimizing_Compiler_CMS_Users_Guide_Oct76.pdf` | Low |
| Installation Guide for MVS / for CMS | SC26-4121-1 / SC26-4122-0 | Sep 1985 | Release 5 / 5.1 | BS`ibm/370/pli/SC26-4121-1_PLI_Release_5_Installation_Guide_for_MVS_Sep1985.pdf`, `…/SC26-4122-0_PLI_Release_5.1_Installation_Guide_for_CMS_Sep1985.pdf` | Low |
| OS PL/I Version 2 Problem Determination | LY27-9528-0 | Dec 1987 | OS PL/I V2 (incidental) | BS`ibm/370/pli/LY27-9528-0_OS_PLI_Version_2_Problem_Determination_Dec87.pdf` | Medium (dump/control-block layouts of the descendant runtime) |
| An Introduction to Structured Programming in PL/I | GC20-1777-1 | Jun 1977 | tutorial | BS`ibm/370/pli/GC20-1777-1_An_Introduction_to_Structured_Programming_in_PL-I_2nd_ed_197706.pdf` | Low |

#### C. OS PL/I Checkout Compiler (5734-PL2)

| Title | Order no. | Date | URL | Value |
|---|---|---|---|---|
| General Information | GC33-0003-0 | Jul 1970 (from filename) | BS`ibm/370/pli/GC33-0003-0_PLI_Checkout_Compiler_General_Info_Jul70.pdf` | Low |
| Messages | SC33-0034-2 (3rd ed., May 1975 reprint of -1 + SN33-6106; filename says Oct1976) | 1975 | BS`ibm/360/pli/SC33-0034-2_OS_PL1_Checkout_Compiler_Messages_Oct1976.pdf` | Low |
| Language Reference | shared GC33-0009 (see B) | | | |

#### D. DOS PL/I Optimizing Compiler (5736-PL1). These are on bitsavers, but beware the filenames.

| Title | Order no. | Date | URL | Note |
|---|---|---|---|---|
| DOS PL/I Optimizing Compiler PLM | LY33-6010-1 | Oct 1976 (from filename) | BS`ibm/370/pli/LY33-6010-1_DOS_PLI_PLM_Oct76.pdf` | DOS compiler internals. Same language level and architecture family as the OS Optimizer, so the best available substitute for the missing LY33-6007. Medium–high |
| **DOS** PL/I Resident Library: Program Logic | LY33-6011-1 (2nd ed. Mar 1973, revised Oct 1976 by TNL LN33-6179), program 5736-LM4 | 1973/76 | BS`ibm/370/pli/LY33-6011-1_PLI_Resident_Library_PLM_Oct76.pdf` | **The filename omits "DOS"**, but the title page reads "DOS PL/I Resident Library: Program Logic … applies to Version 1, Release 5 … of the DOS Optimizing Compiler". Medium (runtime design close to the OS library) |
| DOS PL/I Transient Library: Program Logic | LY33-6012-1 | Oct 1976 (from filename) | BS`ibm/370/pli/LY33-6012-1_PLI_Transient_Library_PLM_Oct76.pdf` | DOS, according to the LY33-6011-1 preface ("DOS PL/I Transient Library: Program Logic, Order No. LY33-6012"). Medium |
| DOS PL/I Execution Logic | SC33-0019-1 | Sep 1973 | BS`ibm/370/pli/SC33-0019-1_DOS_PLI_Execution_Logic_Sep73.pdf` | Medium |
| DOS PL/I Programmer's Guide (Rel 6), Transient Library Messages, CMS UG | SC33-0008-6, SC33-0035-2, SC33-0051-0 | 1976–84 | BS`ibm/370/pli/` | Low |

### Inferences
- For a PL/I (F) V5-compatible compiler and runtime on MVS 3.8j, the core set is fully online: GC28-8201-4 (language and data mapping), GC28-6594-7 (JCL, options, listings, messages, App. J conventions), GY28-6800-5 (compiler PLM), GY28-6801-6 (library PLM) and C28-6590-0 (math). C33-0002-0 adds the V5 changes.
- The editions online (PLM 1971, library PLM 1972) apply to OS Release 18/20 and later. The TK5 compiler calls itself V5.5. Neither PLM edition names a "V5.x" level on its title page, and no later GY28-6800 edition (-6 or after) turned up anywhere. Any small differences have to be checked against the TK5 modules themselves.
- GY28-6801-6 explicitly covers the compiler/library/control-program interface, so it is the primary source for DSA, PRV and dope-vector conventions. The Optimizer's Execution Logic (SC33-0025) describes the later IBMB*-library model (TCA, etc.), which is useful context but **not** PL/I (F)'s IHE convention.

### Gaps
- The exact meaning of TK5 "V5.5" in terms of manual editions is not established.

## Gaps: manuals known to exist but not found online

### Takeaway
The PLMs that are missing are those of the **OS Optimizing Compiler** (LY33-6007) and the **OS Resident and Transient Libraries** (LY33-6008, LY33-6009). Most of the **Checkout Compiler** set is missing too. For PL/I (F), no important gap was found apart from intermediate editions and the reference summary card.

### Cited Findings
- The OS Optimizing Compiler's bibliographies list "OS PL/I Optimizing Compiler: Program Logic, Order No. LY33-6007", "OS PL/I Resident Library: Program Logic, Order No. LY33-6008" and "OS PL/I Transient Library: Program Logic, Order No. LY33-6009". SC33-0006-7 also lists "OS PL/I Optimizing Compiler: Installation for MVS, SC26-4121" and "… for CMS, SC26-4122" — [SC33-0006-7](https://bitsavers.org/pdf/ibm/370/pli/SC33-0006-7_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep85.pdf); [SC33-0025-3](https://bitsavers.org/pdf/ibm/370/pli/SC33-0025-3_PLI_Optimizing_Compiler_Execution_Logic_Sep85.pdf). None of the three LY33-600x numbers appears in the bitsavers index (2026-10-09), and an archive.org search for "LY33-6" found only VSE/Sort manuals.
- LY33-6009-1 "OS PL/I Transient Library: Program Logic", Sep 1971, is held as a physical copy at the Centre for Computing History (UK). Their page says it is scanned and "may be available to view", and to "contact us for details" — [computinghistory.org.uk det/10214](https://www.computinghistory.org.uk/det/10214/OS-PL-I-Transient-Library-Program-Logic/). The same site lists the Checkout Compiler Execution Logic ([det/40719](https://www.computinghistory.org.uk/det/40719/IBM-Program-Product-OS-PL-I-Checkout-Compiler-Execution-Logic)), the Checkout TSO User's Guide SC33-0033 ([det/10699](https://www.computinghistory.org.uk/det/10699/OS-PL-I-Checkout-Compiler-TSO-User-s-Guide/)), Optimizer Programmer's Guide Release 4.0 ([det/60182](https://www.computinghistory.org.uk/det/60182/Program-Product-OS-PL-I-Optimizing-Compiler-Programmer-s-Guide-Release-4-0/)) and Optimizer General Information ([det/8981](https://www.computinghistory.org.uk/det/8981/IBM-OS-PL-I-Optimizing-Compiler-General-Information/)), all as catalogue entries, not downloads.
- The Checkout Compiler set, as listed in LY33-6011-1's "Associated Publications": Specifications GC33-0030, General Information GC33-0003, **Programmer's Guide SC33-0007**, **Execution Logic SC33-0032**, Installation SC33-0031, TSO User's Guide SC33-0033 and CMS User's Guide SC33-0047. It also lists the reference cards "OS PL/I Checkout and Optimizing Compilers: Keywords, SX33-6002" and "Terminal Commands and Compiler Options, SX33-6005" — [LY33-6011-1](https://bitsavers.org/pdf/ibm/370/pli/LY33-6011-1_PLI_Resident_Library_PLM_Oct76.pdf). Of these, only GC33-0003-0 and the Messages book (SC33-0034-2) are on bitsavers. The Waterloo computer museum has a physical "OS PL/I Checkout and Optimizing Compilers: Keywords Reference Summary" — [uwaterloo object 2344](https://computermuseum.uwaterloo.ca/index.php/Detail/objects/2344). A web search for SC33-0007 found no online copy.
- PL/I (F), not found online: Y28-6800-0/-2/-4 and GY28-6801-0..-5 (except -4); GC28-6594 editions other than -4 and -7; C28-8201 edition -0. (Edition -2 and a scan of -3's cover and contents were found later, see the rows added on 2026-10-14.) **No PL/I (F) reference summary/card (an X/GX order number) was found**; its order number is unverified. **No separate "PL/I (F) Messages" manual exists for the era covered**: the messages are in App. K of the Programmer's Guide — [GC28-6594-7](https://bitsavers.org/pdf/ibm/360/pli/GC28-6594-7_PLIf_pgGd_Jan71.pdf).
- **The order numbers asked about, resolved:** GC28-8201 = PL/I (F) Language Reference (verified). GC28-6594 = PL/I (F) Programmer's Guide (verified). GY28-6800 = PL/I (F) Compiler PLM (verified). GY28-6801 = PL/I Subroutine Library PLM (verified). C28-6590 = Library Computational Subroutines (verified). GC33-0009 = (Checkout and) Optimizing Language Reference (verified; earlier editions carry the prefix SC). SC33-0006 = Optimizer Programmer's Guide (verified). SC33-0025 = Optimizer Execution Logic (verified). SC33-0027 = Optimizer Messages (verified). SC33-0026 = Optimizer "System Information" (verified; the later Installation books are SC26-4121/4122). **LY33-6011 = DOS Resident Library PLM, not OS** (verified from its title page).

### Inferences
- The best available substitutes for the missing OS Optimizer PLMs are the DOS LY33-6010/6011/6012 books together with the OS SC33-0025 Execution Logic. For PL/I (F) itself no substitute is needed.

### Gaps
- Whether any private collection has scanned LY33-6007/6008/6009 or SC33-0007/0032 could not be determined. The Centre for Computing History scan of LY33-6009-1 is the one concrete lead.

## Is there an IBM "PL/I (F) to Optimizing Compiler" conversion manual?

### Takeaway
No standalone conversion manual was found. IBM put the F-to-Optimizer material into the Optimizer's General Information manual. GC33-0001-2 has a section "Compatibility with the (F) Compiler" and Appendix B "Compatibility with (F)". It describes the degree of compatibility with version 5 of PL/I (F) and the "significant incompatibilities".

### Cited Findings
- GC33-0001-2 describes the language "as implemented by version 5 of the PL/I (F)" compiler and "the degree to which the compiler is compatible with version 5 of the PL/I (F) Compiler". Its contents include "Compatibility with the (F) Compiler" and "APPENDIX B: COMPATIBILITY WITH (F)" — [GC33-0001-2](https://bitsavers.org/pdf/ibm/370/pli/GC33-0001-2_PLIgenInf_Jun73.pdf).
- C33-0002-0 (PL/I (F) Version 5 Planning Guide) has an appendix on "compatibility with previous versions of the compiler" (that is, F V4 to F V5) — [C33-0002-0](https://bitsavers.org/pdf/ibm/360/pli/C33-0002-0_PLI(F)plGd_Mar69.pdf).

### Gaps
- No order number for a dedicated conversion manual turned up in the bibliographies checked (SC33-0006-7, SC33-0025-3, LY33-6011-1, GC33-0001-2). A separate IBM conversion guide may still exist in a field-developed (G320/GG2x) series; this is unverified.

## Are sources of the PL/I (F) compiler or library on any public site?

### Takeaway
No assembler source of the PL/I (F) compiler (IEM*) or library (IHE*) modules was found. The public material contains the **object/load modules** plus a sysgen macro with PL/I library structure: Jay Moseley's MVT PL/I package, the OS/360 tapes on ibiblio, CBT Overflow File 092 (the compiler, V5.5) and CBT File 1058 (the PLI-F execution library). The IBM optional source tapes were not found online for PL/I.

### Cited Findings
- Jay Moseley's "PL/I Compiler from MVT" ships `pli.tgz`: a HET tape with PL1.LOAD (compiler load modules), PL1.LIB (library object modules) and PL1.PROCS (cataloged procedures), created by IEBCOPY unload from 3350, with install/IVP jobs. The page mentions no source — [jaymoseley.com pli.htm](https://www.jaymoseley.com/hercules/compilers/pli.htm).
- The OS/360 tape images on ibiblio (`https://www.ibiblio.org/jmaynard/`: `k360s-mb1xx-aws.zip`, `k360s-ma20x-aws.zip`, `k360n-ma1xx-aws.zip`, dated 2003) were downloaded and scanned here (EBCDIC decoded with Python). Results:
  - The `k360s-*` volumes contain PL/I (F) member names (IEMAA…IEMXF in PDS directories, about 270 IEM names, more than 700 IHE names) and JCL such as `//PL1D EXEC PGM=IEMAA,PARM='DECK,NOLOAD',REGION=52K`.
  - They also contain assembler **macro** source: a shared-library generation macro listing `IHEQxxx DXD` pseudo-register definitions and the `IHELTVA`/`IHELTV1` transfer vector with `AIF` logic on "IS THIS MODULE RESIDENT IN SHARED LIBRARY".
  - A heuristic search for `CSECT`-style source card images of IEM/IHE modules found none.
  - Treat this as "no compiler/library source found by heuristic scan", not as proof of absence — [ibiblio jmaynard](https://www.ibiblio.org/jmaynard/).
- IBM did offer optional source tapes so installations could modify and assemble modules that were normally shipped as object code (search-result summary of Wikipedia "OS/360 and successors"). Hercules users say the CBT CD-ROM / Rick Fochtman OS/360 distribution includes "source code to the operating system and utilities" — [conmicro.com hercos360 getstarted](http://www.conmicro.com/hercos360/getstarted.html). Whether the PL/I (F) program product source is among it is **unverified**. Program products such as PL/I (F) were usually separate from the OS source tapes.
- **CBT tape**. The file index ([cbtdowns.htm](https://www.cbttape.org/cbtdowns.htm), [covdowns.htm](https://www.cbttape.org/covdowns.htm)) lists "File # 092 PL1/F Compiler - moved to CBT Overflow Tape File 092". The Overflow index lists "File # 092 PL1/F Compiler and Library" ([ftp/cov/COV092.zip](https://www.cbttape.org/ftp/cov/COV092.zip), 947 KB, containing `COV466.FILE092.XMI`). CBT "File # 1058 PLI-F Execution Library - needed for other files" is [ftp/cbt/CBT1058.zip](https://www.cbttape.org/ftp/cbt/CBT1058.zip). Both were downloaded and their XMI contents scanned (EBCDIC). They hold **load/object modules only**: COV092 contains the compiler, whose strings include "PAGE 1 VERSION 5.5 / OS/360 PL/I COMPILER (F)" and the IEM39xxI option messages, about 260 IEM names in all. CBT1058 contains the library, with IHE message texts and IHEDUMP text such as "CHAIN BACK THROUGH SAVE AREAS", "PRV - PSEUDO REGISTERS" and "MODULE IHEDMA". Neither has a single `IEMxx/IHExxx CSECT` source card. Other CBT files relevant to PL/I: Overflow File 260 "SHARE PL/1 Tape Documentation", File 091 "PROCs for PL1/F …", Overflow 281/CBT 681 "PL/I Execution Analyzer (PLEA)", Overflow 282 "SORTPLI", CBT 783 "PL/I Scientific Subroutine Package for 360 (OS)" — [cbtdowns.htm](https://www.cbttape.org/cbtdowns.htm), [covdowns.htm](https://www.cbttape.org/covdowns.htm).
- A web search for PL/I (F) source / IEMAA source returned nothing relevant (no GitHub hits either).

### Inferences
- The compiler writer will have to work from the PLMs and from the behaviour of PL/I (F) on TK5, measured with probe programs. The shared-library macro on the k360s tapes is a useful machine-readable list of the library's pseudo-register (PRV) names.

### Gaps
- CBT Overflow File 260 ("SHARE PL/1 Tape Documentation") was not opened; it may contain SHARE-era technical material on the PL/I runtime.
- The exact identity of the ibiblio `k360s-ma20x`/`mb1xx` tapes (DLIB versus starter-system volumes) was not established.

## Secondary material useful for a compiler writer

### Takeaway
On bitsavers there are Joan K. Hughes' "PL/I Programming" (Wiley 1973), the Multics PL/I compiler PLM (AN54, 1974) and the Multics PL/I Reference (AM83A, 1978), and Digital Research PL/I manuals (1982). Iron Spring PL/I documents its own (x86) descriptor and calling conventions, not IBM's. **Found: B. L. Marks's IBM Systems Journal paper on the design of the PL/I Checkout Compiler** (vol. 12 no. 3, 1973), on bitsavers. It describes the execution environment that lets Checkout-compiled code run together with Optimizer-compiled code. No Systems Journal paper on the Optimizing Compiler was found.

### Cited Findings
- Hughes, Joan K., *PL/I Programming*, John Wiley & Sons, 1973 (verified from the scan's title page) — BS`ibm/360/pli/Hughes_PLI_Programming_1973.pdf`, [link](https://bitsavers.org/pdf/ibm/360/pli/Hughes_PLI_Programming_1973.pdf).
- Multics PL/I: compiler PLM AN54 (Aug 1974) — [bitsavers](https://bitsavers.org/pdf/honeywell/large_systems/multics/AN54_Pl1_Compiler_Program_Logic_Manual_197408.pdf). Multics PL/I Reference AM83A (Sep 1978) is at `honeywell/large_systems/multics/AM83A_PL1refMan_Sep78.pdf` (from the index). [multicians.org/pl1.html](https://multicians.org/pl1.html) has background. Value: medium (full-language compiler design, descriptors).
- Digital Research PL/I (Subset G): Language Reference Oct 1982 and Programmer's Guide Dec 1982, at bitsavers `digitalResearch/pl1/` (from the index). Value: low–medium (a small compiler/runtime).
- Iron Spring PL/I Programming Guide: describes "locator/descriptor" addressing (`Loc @DSA±'oooo'`), passing arguments by reference with optional descriptors for strings, arrays and structures, and a `desc.inc` layout "subject to change". This is Iron Spring's own x86 ABI, not IBM (F) — [iron-spring.com prog_guide](http://www.iron-spring.com/prog_guide.html) (TLS certificate expired; fetched over plain HTTP).
- SHARE 61 (1983) paper "The PL/I VSAM Interface" (Kinzer) — BS`ibm/share/SHARE_61_Proceedings_Volume_1_Summer_1983/A764_The_PLI_VSAM_Interface;_Kinzer.pdf` (from the index). Low.
- Search results point to CACM March 1973, "Design and implementation of a diagnostic compiler for PL/I" (PL/C) — [cacm](https://cacmb4.acm.org/magazines/1973/3/11923-design-and-implementation-of-a-diagnostic-compiler-for-pl-i), and IBM Research "A structural view of PL/I" — [research.ibm.com](https://research.ibm.com/publications/a-structural-view-of-pli). Low–medium.
- IBM Systems Journal vol. 12 no. 3 (1973), article file `ibmsj1203G.pdf` (13 pp.). The abstract: "The PL/I Checkout Compiler was designed to emphasize programmer productivity … The design is constrained by the requirement that a subroutine developed using this checkout compiler should be capable of executing in conjunction with code generated by a more conventional compiler. The execution environment that supports this operation is described." The running page header reads "MARKS". The text also says "The first implementation of PL/I, the PL/I F Compiler for Operating System/360 … was introduced in 1966. Further facilities and performance have been added in four subsequent versions of the F compiler. In 1971, the F compiler was superseded by a pair of compilers" — [bitsavers](https://bitsavers.org/pdf/ibm/IBM_Systems_Journal/123/ibmsj1203G.pdf). Value: medium.
- MITRE ESD-TR-68-154 "Proceedings of PL/I Seminars" (Apr 1968) is at bitsavers `mit/mitre/ESD_Technical_Reports/` (from the index).

### Gaps
- Only vol. 12 no. 3 of the IBM Systems Journal scans was checked article by article. Volumes 9–13 (1970–74) are on bitsavers under opaque file names (`ibmsjVVNNx.pdf`), so a paper on the Optimizing Compiler in another issue cannot be ruled out. dblp's per-volume tables of contents at vldb.org answered 404.
- Peter Flass' documentation of **IBM** PL/I data formats (locators/descriptors) was not found. The Iron Spring guide covers Iron Spring's own formats only.
