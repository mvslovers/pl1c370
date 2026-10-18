# IBM PL/S family (BSL, PL/S, PL/S II, PL/S III, PL/AS, PL/X): history, versions, availability

Scope: 1966-1985 focus. Primary documents were downloaded from bitsavers to
`a local download directory (PDFs not committed; URLs in the text)`
and converted with pdftotext; quotes below come from those text files. The IDR
statistics come from grepping the maintainer's local MVS 3.8j source trees
(`/Users/mike/repos/mvs/mvs38-ibmsrc`, 1.9 GB, and `/Users/mike/repos/mvs/mvs38src`).

Primary documents on bitsavers (all under https://bitsavers.org/pdf/ibm/360/):
- `bsl/BSL_libraryOct67.pdf` (BSL Library, Oct 1967)
- `bsl/Z28-6642-0_BSL_langSpec.pdf` (BSL Language Specification, revision of a description dated 24 Jan 1968; "IBM Confidential")
- `bsl/Z28-6678_BSL_primer.pdf` (Basic Systems Language Primer; "IBM Confidential")
- `bsl/Z28-6682-2_BSL_usersGuide.pdf` (BSL User's Guide, Fifth Edition, May 1969; "IBM Confidential")
- `bsl/BSL_LangSpecUpdMay69.pdf`, `bsl/BSL_bulletinSep69.pdf` (BSL Bulletin No. 7, 19 Sep 1969)
- `pls/GC28-0786-0_Guide_to_PL_S_Generated_Listings_Jul72.pdf` (GC28-6786-0, July 1972, public)
- `pls/GC28-6794-0_PLSIIguideMay74.pdf` (GC28-6794-0, Guide to PL/S II, May 1974, public)

## 1. Lineage, dates, designers; which version produced MVS 3.8 code; meaning of the IDR ids

### Takeaway
BSL (Basic Systems Language) existed by 1967-69 as an internal Poughkeepsie (Systems Development Division, Dept. D76 "Programming Technology") compiler running under OS/360 and DOS; it was renamed PL/S, then replaced by PL/S II (in use by 1972, "Version 4.0" compile dated 72.227; "Version 6.1M" dated 74.049). MVS 3.8 (OS/VS2 Release 3.x, 1974-1980 PTF levels) was compiled with PL/S II; its IDRs show two formats: `C'PL/S-II'` with versions 0402/0502/0503 (dates 1973-75: 1973-74 in `mvs38-ibmsrc`, plus three `0503,75006` instances in `mvs38src`) and a later `C'PLShhmm'` form with versions 0602/0603/0700/0701/0702/0801/0802 (dates 1974-1980), where the four digits after "PLS" are almost certainly the compile time of day (HHMM), not a component id. W. R. Brittenham (IBM) is the name attached to the published PL/S papers.

### Cited Findings
**BSL (late 1960s)**
- BSL Language Specification Z28-6642-0 states it "is a revision of the BSL Basic Systems Language Description, dated January 24, 1968"; copies from "Department D76, Systems Development Division, Poughkeepsie, New York"; marked "IBM Confidential ... None of this information shall be divulged to persons other than IBM employees authorized ..." — [BSL Language Specification Z28-6642-0](https://bitsavers.org/pdf/ibm/360/bsl/Z28-6642-0_BSL_langSpec.pdf)
- The BSL compiler had numbered internal releases: the spec says Compile-Time Facilities and the GENERATED attribute arrived in "BSL/10", OPTIONS(VLIST) in "BSL/11" — [Z28-6642-0](https://bitsavers.org/pdf/ibm/360/bsl/Z28-6642-0_BSL_langSpec.pdf)
- BSL Bulletin No. 7 (19 Sep 1969): "BSL/eleven' is a modification of the eleventh internal release of the BSL compiler"; "BSL/12 is scheduled to be distributed November 26, 1969"; distributed "to IBMers who are using BSL"; issued by "Basic Systems Language/Programming Technology/SDD, Poughkeepsie" — [BSL Bulletin Sep 1969](https://bitsavers.org/pdf/ibm/360/bsl/BSL_bulletinSep69.pdf)
- BSL User's Guide, Fifth Edition, May 1969: "The BSL compiler runs as a problem program under the IBM System/360 Disk Operating System (DOS) or the IBM System/360 Operating System (OS). Under DOS, the compiler requires ... 128K bytes ... Under OS, ... 100,000 bytes of main storage" plus dictionary and buffers; it has a "macro phase" (compile-time processor) followed by a "compile phase"; cataloged procedures BSLX, BSLASM, BSLALG — [BSL User's Guide Z28-6682-2](https://bitsavers.org/pdf/ibm/360/bsl/Z28-6682-2_BSL_usersGuide.pdf)
- A BSL Library (runtime procedures "available to the BSL user when running his program") existed in Oct 1967 — [BSL Language Spec preface](https://bitsavers.org/pdf/ibm/360/bsl/Z28-6642-0_BSL_langSpec.pdf); [BSL Library Oct67](https://bitsavers.org/pdf/ibm/360/bsl/BSL_libraryOct67.pdf)
- Wikipedia: PL/S "was developed by IBM in the late 1960s, under the name Basic Systems Language (BSL), as a replacement for assembly language on internal software projects"; early users were IEHMOVE and TSO for MVT; reference note "BSL was renamed PL/S and replaced by PL/S II" — [Wikipedia IBM PL/S](https://en.wikipedia.org/wiki/IBM_PL/S)

**PL/S (I) and PL/S II**
- GC28-6786-0 (July 1972) covers "the first version of PL/S"; GC28-6794-0 (May 1974) defines "PL/S II (Programming Language/Systems-Second Version)" and refers back to GC28-6786 for the first version. Both from IBM Publications Development, Dept. D58, Bldg 706-2, Poughkeepsie — [GC28-6786-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-0786-0_Guide_to_PL_S_Generated_Listings_Jul72.pdf); [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- The sample listings in GC28-6794-0 show the compiler headings "PL/S II VERSION 4.0 ... DATE 72.227" and "PL/S II VERSION 6.1M ... DATE 74.049"; the options listing includes `MACHINE (S360)`, `OPTIMIZE (3,SPACE)`, `MACRO`, `CONCHAR('@')`, and `NOIDR` (i.e. emitting the IDR on the END card was a compiler option) — [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- Succession per Wikipedia: "PL/S was succeeded by PL/S II, PL/S III and PL/AS (Programming Language/Advanced Systems), and then PL/X (Programming Language/Cross Systems). PL/DS ... used to develop the DPPX operating system, and PL/DS II ... for the DPPX/370 port" (article flagged "No footnotes") — [Wikipedia IBM PL/S](https://en.wikipedia.org/wiki/IBM_PL/S)
- An IBM-MAIN thread (seen only as search snippets; page fetch blocked by HTTP 429/503) states "PL/S-III was an old language that was replaced by PL/AS prior to 1985. PL/X-370 and then PL/X-390 replaced PLAS", and "PL/S went through at least 3 versions; PL/AS went through at least 4 versions" — [IBM-MAIN "PL/AS PL/S PL/X"](https://groups.google.com/g/bit.listserv.ibm-main/c/NtsoKQxODrI) (UNVERIFIED verbatim)
- SDS mainframe dictionary: PL/AS "Replacement for PL/S. The PL/AS compiler generates Assembler code, not final object code. Not available to Joe Public. Replaced by PL/X." — [SDS Mainframe Dictionary "P"](https://support.sdsusa.com/support/dictionary/p/)

**Designers / people**
- W. R. Brittenham, "PL/S, Programming Language/Systems", Proc. GUIDE 34, 14 May 1972, pp. 540-556; W. R. Brittenham and B. F. Melkun, "The Systems Programming Language Problem", IFIP Working Conference on Machine Oriented Higher Level Languages, Trondheim, 29-31 Aug 1973, pp. 29-47 (North-Holland 1974) — "explores the technical and psychological problems encountered in implementing PL/S" — [Wikipedia references](https://en.wikipedia.org/wiki/IBM_PL/S)
- Computer History Museum catalog 102712820 "PL/S Speech and Manual", 1972-1974, author W. R. Brittenham, includes the Guide to PL/S II GC28-6794; gift of John Ehrman — [CHM catalog](https://www.computerhistory.org/collections/catalog/102712820)
- Outside analysis: Gio Wiederhold and John Ehrman, "Inferred SYNTAX and SEMANTICS of PL/S", SIGPLAN Notices 6(10), Oct 1971 (Symposium on Languages for System Implementation) — [Wikipedia references](https://en.wikipedia.org/wiki/IBM_PL/S). (Could not fetch the paper itself.)

**IDR data in MVS 3.8j source (local measurement, this session)**
The table below is from `mvs38-ibmsrc` only; `mvs38src` additionally has `C'PL/S-II',0503,75006` x3.
Grep of `C'PL/S...'`/`C'PLSnnnn'` END-card IDRs in `/Users/mike/repos/mvs/mvs38-ibmsrc`, 534 hits, counted by (translator, version, year):

| IDR translator id | version | years (count) |
|---|---|---|
| `PL/S-II` | 0402 | 1973 (4), 1974 (1) |
| `PL/S-II` | 0502 | 1973 (13), 1974 (15) |
| `PL/S-II` | 0503 | 1973 (3), 1974 (1) |
| `PL/S` | 1400 | 1974-1978 (5; e.g. IFD/IFDMSG03, IFDMSG53) |
| `PLShhmm` | 0602 | 1975 (4) |
| `PLShhmm` | 0603 | 1974 (13), 1975 (108), 1976-78 (40) |
| `PLShhmm` | 0700 | 1974 (2) |
| `PLShhmm` | 0701 | 1975 (11), 1976 (38), 1977 (71), 1978 (146) |
| `PLShhmm` | 0702 | 1978 (27), 1979 (3), 1980 (19) |
| `PLShhmm` | 0801/0802 | 1978 (1/9) |

- In all 492 `PLSnnnn` ids, the first two digits are 00-23 and the last two 00-59 (0 violations; max minutes = 59). Different modules with the same version and date carry different nnnn (e.g. `IKJEFLG,(C'PLS2001',0701,75304)`, `IDA019S4,(C'PLS1344',0701,78173)`). Combined with the compiler's listing heading format "TIME 13.34 DATE 74.049" — [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf) — this strongly indicates `PLShhmm` = "PLS" + compile time HHMM.
- The IDR operand of the S/360 assembler END statement is (translator id, version/level as 4 digits, date YYDDD) — standard assembler convention; not separately re-sourced in this session.

### Inferences
- `0402`, `0502`, `0503`, `0603`, `0701`, `0702` are, approximately, compiler version.level (VVLL): PL/S II V4L2, V5L2, V5L3, V6L3, V7L1, V7L2. The ordering fits the guide's "VERSION 4.0" (Aug 1972) and "VERSION 6.1M" (Feb 1974) headings, but the mapping is not exact: no `0601` IDR was found, `0503` IDRs still carry 1974-75 dates after the guide's 6.1M sample (dated 74.049), and `0603` first appears later in 1974. So read VVLL as an approximate version/level, not an exact equation with the listing heading. MVS 3.8 code was therefore produced by successive PL/S II levels from roughly V4 (1972-73) through V7/V8 (1975-1980); nothing in the MVS 3.8 IDRs points to PL/S III or PL/AS.
- Around version 6 (1974) the IDR changed from the fixed string `PL/S-II` to `PLS`+time-of-day, i.e. "PLS2321" is "compiled at 23:21", not a component id. Base rate: a random 4-digit number passes the HH<=23/MM<=59 filter 1440/10000 = 14.4% of the time, so 492 passes by chance is ~0.144^492, effectively zero. (Inference; no IBM document states the format.)
- `C'PL/S',1400` on the IFD (OLTEP/diagnostic) modules is a different translator id/numbering, possibly a different PL/S compiler line (e.g. a DOS/VS or another lab's build); unexplained.
- Designer attribution beyond Brittenham (and Melkun as co-author) could not be established; BSL came out of SDD Poughkeepsie "Programming Technology" Dept. D76.

### Gaps
- No primary source found for exact introduction dates of PL/S III, PL/AS, PL/X, or PL/DS; the "PL/S III replaced by PL/AS before 1985" claim is from an IBM-MAIN snippet that could not be fetched.
- No IBM statement of the IDR format for PL/S; the HHMM reading is inferred.
- The Brittenham GUIDE 34 and IFIP 1973 papers and the Wiederhold/Ehrman SIGPLAN paper were not read (paywalled/not found online); they would likely name the design team and compiler architecture.
- Fred Brooks / Mythical Man-Month: no source found linking PL/S to OS/360 development in this session; OS/360 itself was written in assembler (not re-sourced here).

## 2. Was the compiler ever available outside IBM? Public documentation; reasons for secrecy

### Takeaway
In the 1970s the compiler was explicitly internal: IBM's own public manuals of 1972 and 1974 say "The PL/S [II] compiler is not available outside IBM", while the PL/S-generated assembler source (with PL/S as comments) and microfiche listings were orderable by customers. BSL documentation itself was "IBM Confidential". The publicly sold books (GC28-6786, GC28-6794) only teach reading listings. Much later (1990s), IBM made PL/X available to some ISVs under licence (Developer Partner/PWD), reportedly with restrictions and no longer sold; this is attested only by secondary sources.

### Cited Findings
- GC28-6786-0 (1972): "The PL/S (Programming Language/Systems) compiler is a proprietary program used by IBM to develop other programs that are made generally available. The PL/S compiler is not available outside IBM. Programs written by IBM in PL/S are documented by means of listings in microfiche form." — [GC28-6786-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-0786-0_Guide_to_PL_S_Generated_Listings_Jul72.pdf)
- GC28-6794-0 (1974) repeats this for PL/S II and adds: "This guide does not contain information on writing and compiling PL/S II source programs"; "If you are considering modifications to your operating system, you can order the assembler source modules in machine readable form from the IBM Programming Information Department"; listings "may be ordered from the IBM Programming Information Department" — [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- Section 4 of GC28-6794-0 gives rules for patching generated code because "When modules coded in PL/S II are recompiled by IBM for a new release, the assembler code ... may be generated differently" (e.g. do not reference @-labels, use `$` for own labels, add data before @ENDDATA/@ENDDATD, wrap mods in AGO/ANOP) — [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- BSL manuals: may only be divulged to IBM employees "or individuals or organizations authorized by the Systems Development Division in accordance with existing policy regarding release of company information" — [Z28-6642-0](https://bitsavers.org/pdf/ibm/360/bsl/Z28-6642-0_BSL_langSpec.pdf); [Z28-6678 primer](https://bitsavers.org/pdf/ibm/360/bsl/Z28-6678_BSL_primer.pdf)
- Wikipedia: "Although users frequently asked IBM to release PL/S for their use, IBM refused, saying that the product was proprietary. Their concern was that open PL/S would give competitors, Amdahl, Itel ..., Storage Technology Corporation, Trilogy Systems, Magnuson Computer Systems, Fujitsu, Hitachi, and other PCM vendors a competitive advantage. However ... they shipped the PL/S source code to large parts of the OS to customers"; "Closed PL/S meant that only IBM could easily modify and enhance the operating system." — [Wikipedia IBM PL/S](https://en.wikipedia.org/wiki/IBM_PL/S) (unfootnoted)
- Wikipedia cites a Washington Post article (20 Sep 1987, "IBM seen as loser in truce with Fujitsu over software") for IBM refusing to provide the compiler; the article could not be fetched (HTTP 403) — [Washington Post 1987](https://www.washingtonpost.com/archive/business/1987/09/20/ibm-seen-as-loser-in-truce-with-fujitsu-over-software/a94ea5ba-17b0-41f1-ae98-93a2107cbb8f/) (content UNVERIFIED)
- Wikipedia: "IBM recanted and has offered the current versions of PL/S to select customers (ISVs through the Developer Partner program.)" — [Wikipedia IBM PL/S](https://en.wikipedia.org/wiki/IBM_PL/S) (unfootnoted, no date)
- IBM-MAIN (search snippets only): "IBM has never released PL/X as a customer product but they have shipped the compiler at various times under limited circumstances - it was available to 3rd party developers for a while"; "A version was made available to ISVs for a brief period in the 1990s. The PWD program let vendors have a copy of PL/X on request. ISVs need a license to use PL/X and it's restricted for use only on LPARs with limited access." — [IBM-MAIN "PL/AS PL/S PL/X"](https://groups.google.com/g/bit.listserv.ibm-main/c/NtsoKQxODrI); [narkive "PL/S ??"](https://bit.listserv.ibm-main.narkive.com/wbCSyxG9/pl-s) (UNVERIFIED verbatim; pages returned 429/503)
- Hacker News (skissane, 29 Jun 2020): "PL/X is only an option for those few companies who have bought PL/X licenses, which apparently IBM doesn't even sell anymore." — [HN item 23674599](https://news.ycombinator.com/item?id=23674599)

- Local check (this session): `find /Users/mike/repos/mvs/mvs38-ibmsrc -iname 'PLS*' -o -iname 'BSL*'` returns nothing across its 276 component directories; the distribution source contains compiler *output* (assembler with PL/S comments) only, no compiler.

### Inferences
- The 1970s secrecy rationale documented by IBM itself is only "proprietary program"; the PCM-competition rationale comes from Wikipedia/secondary accounts. The antitrust context (US v. IBM 1969-1982, unbundling 1969) is plausible background but I found no source tying it directly to PL/S policy.
- Customers had legal access to the *output* (assembler with PL/S comments, microfiche) but not the translator; the MVS 3.8j source in the hobbyist world is exactly that output.
- "Available to ISVs under NDA" applies to PL/X in the 1990s (PWD = PartnerWorld for Developers era), not to 1970s PL/S II. Treat it as unverified folklore pending a primary IBM source.

### Gaps
- No Computerworld / SHARE primary article on PL/S licensing requests was retrieved (Google Books search not reachable in this session).
- No IBM announcement letter or contract for PL/X ISV licensing found.

## 3. Surviving compilers outside IBM; clones; legal discussion

### Takeaway
No IBM PL/S/PL/S II compiler binary or source is known to survive publicly (none on bitsavers, which holds only BSL/PL/S manuals; none found on CBT/Hercules search results). Two independent clones are documented: Rand's RL/S (1976, PL/S II-compatible, built with XPL, withdrawn from public distribution after IBM objected) and a Fujitsu PL/S compiler (late 1970s, reportedly derived from IBM's PL/I Optimizer source; claim is "citation needed"). The RL/S Language Reference Manual (1977) is on the Internet Archive; the RL/S compiler itself is not known to be available.

### Cited Findings
- bitsavers `pdf/ibm/360/pls/` contains only GC28-6786-0 and GC28-6794-0; `pdf/ibm/360/bsl/` only BSL manuals (IndexByDate.txt grep, this session) — [bitsavers IndexByDate](https://bitsavers.org/pdf/IndexByDate.txt)
- Rand RL/S: developed "by the Computation Center of the Rand Corporation in Santa Monica" using "the XPL compiler generator system" by R. Lawrence Clark, James S. Reiley and team leader David J. Smith, "independently of and without any assistance from IBM. Only publicly available, non-copyrighted PL/S documentation and PL/S source and generated assembler code examples from IBM distributed source files for the MVS operating system were used." — [Wikipedia IBM PL/S](https://en.wikipedia.org/wiki/IBM_PL/S)
- "RL/S is fully compatible with IBM's PL/S II language. This was assured by parsing many thousands of lines of IBM written PL/S code taken from the MVS distribution files. The assembler language code produced ... is not identical ... but it is functionally equivalent." — [Wikipedia IBM PL/S](https://en.wikipedia.org/wiki/IBM_PL/S)
- Motivation: Rand's MVT modifications (an ARPANET interrupt "hypervisor"), early MVS beta site; patches at generated-assembler level "were easily obsoleted when IBM modified the PL/S source code and distributed re-generated assembler code" — [Wikipedia IBM PL/S](https://en.wikipedia.org/wiki/IBM_PL/S)
- "The RL/S compiler was initially written in PL/I and later converted to RL/S"; "publicly announced in August, 1976, at a Share meeting ... IBM quickly responded claiming the language was 'proprietary'. While Rand admitted no wrongdoing ..., Rand management agreed not to distribute the compiler publicly" but used it internally — [Wikipedia IBM PL/S](https://en.wikipedia.org/wiki/IBM_PL/S)
- PRIMARY (manual's own text, OCR `_djvu.txt` from archive.org): title page "THE RAND COMPUTATION CENTER: RL/S LANGUAGE REFERENCE MANUAL", authors "[R.] Lawrence Clark, James S. Reiley, David J. Smith" (OCR reads "B. Lawrence Clark"), R-1555/11, "August 1975, Revised July 1977", "Copyright (c) 1975, 1976, 1977 by The Rand Corporation". Preface: describes RL/S "as processed by version 2.8 of the RL/S compiler"; "The goal of the RL/S development effort has been to produce a system development facility that is compatible with the IBM language PL/S. Incompatibilities may be present, due to our incomplete understanding of the PL/S language specifications"; "The assembler language code produced by the RL/S compiler is not identical to the code produced by the proprietary IBM PL/S compiler, but it is our intention that the two be functionally equivalent"; "The RL/S compiler is a proprietary product of The Rand Corporation. No formal relationship with IBM ... is implied"; "Portions of this manual were developed from uncopyrighted IBM PL/S documentation and other PL/S documentation in the open literature"; thanks "David Martin of UCLA and Franklin DeRemer of UCSC for providing the tools used in building the compiler" and "Jerry Marlatt for his assistance in formulating the RL/S grammar" — [RL/S manual files (djvu.txt)](https://archive.org/download/rand-rl-s-language-reference-manual-1977/)
- RL/S options mirror PL/S II: `MACHINE(S360|S370)` ("The IBM OS/360 (F) Assembler may be used for S360, while the OS/VS-VM/370 Assembler is required for S370"), default `MARGINS(2,72,1)`, `@` as default compiler-control character; some PL/S options are "parsed (to provide compatibility with IBM's PL/S), but are otherwise ignored"; compile-time substitution is described as "Literal data ... (a simplified macro facility)" — [RL/S manual files (djvu.txt)](https://archive.org/download/rand-rl-s-language-reference-manual-1977/)
- The manual's first edition (Aug 1975) predates the August 1976 SHARE announcement reported by Wikipedia — [RL/S manual](https://archive.org/details/rand-rl-s-language-reference-manual-1977); [Wikipedia](https://en.wikipedia.org/wiki/IBM_PL/S)
- RL/S Language Reference Manual, Rand R-1555/11, 1977, uploaded to Internet Archive 29 Mar 2023 by user "dvdsmith"; describes RL/S as "based on IBM's system development language PL/S" with inline assembler — [Internet Archive](https://archive.org/details/rand-rl-s-language-reference-manual-1977)
- Fujitsu: "A fully compliant PL/S compiler was developed by Fujitsu Ltd in the late-1970s, adapting IBM's PL/I Optimizer compiler source code as its starting point ... phase names ... with the initial 'I' ... replaced by an initial 'J'" — marked {{citation needed}} — [Wikipedia IBM PL/S](https://en.wikipedia.org/wiki/IBM_PL/S)
- Hobbyist reconstruction work exists for MVS 3.8j source (hercules-os360 "MVS 3.8j source recovery") and other languages (BCPL), but search found no PL/S compiler project — [hercules-os360 thread](https://groups.io/g/hercules-os360/topic/mvs_3_8j_source_recovery/72010833) (fetch returned 402; title only); [BCPL-for-MVS](https://github.com/cbmira01/BCPL-for-MVS)
- Moshix has an XPL compiler targeting S/390 (relevant as the RL/S implementation route, not a PL/S compiler) — [moshix/XPLcompiler](https://github.com/moshix/XPLcompiler)

### Inferences
- The RL/S precedent is the closest legal analogue for a hobbyist clone: built only from public manuals and IBM-distributed source; IBM's reaction was a proprietary claim, and Rand stopped *distribution*, not use, without any court ruling. Whether a language (as opposed to an implementation) is protectable is a separate question; no case law involving PL/S was found.
- The DeRemer/Martin credit fits the XPL / LALR parser-generator lineage Wikipedia names (inference).
- RL/S apparently had only a literal-substitution facility rather than IBM's full %-macro language (inferred from the manual's "simplified macro facility" wording; not exhaustively checked), so it may not have processed bilingual `%GOTO` macros the IBM way.
- The uploader "dvdsmith" is plausibly David J. Smith, the RL/S team leader (unverified); contacting him may be the only route to RL/S source.

### Gaps
- Could not search groups.io H390-MVS / hercules-390 archives or CBT tape catalogs directly (rate limits/paywall); "no surviving binary" is absence of evidence from web search, not a confirmed negative.
- Washington Post 1987 Fujitsu article not read; the IBM-Fujitsu arbitration (1983-1988) concerned operating-system software copying generally — not confirmed in this session whether PL/S was named.

## 4. Language characteristics relevant to a compiler writer

### Takeaway
PL/S II is a PL/I subset/dialect that compiles to S/360 assembler source (not object), with no runtime library requirement, explicit register binding, inline assembler (GENERATE), a %-macro compile-time phase, BASED structures/pointers, bit/char/fixed data, and a fully documented naming convention for generated labels (@PROLOG, @PSTART, @DATA, @DATD, @RFsn/@RTsn/@RCsn, @NMn, @ENDDATA ...). The two 1972/1974 IBM guides plus the BSL Language Spec/User's Guide (1968-69) and the RL/S manual (1977) together give near-complete language and code-generation documentation.

### Cited Findings
- Translation: "The PL/S compiler translates the PL/S source language statements into assembler language instructions suitable for input to a System/360 assembler"; generated code contains "PL/S II statements interspersed as assembler comments"; statement numbers appear in the remarks field — [GC28-6786-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-0786-0_Guide_to_PL_S_Generated_Listings_Jul72.pdf); [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- Keyword index of PL/S II: ABS, ADDR, AUTODATA, AUTOMATIC, BASED, BINARY, BIT, BOUNDARY, BYTE, CALL, CHARACTER, CODEREG, CONSTANT, DATAREG, DCL, DEFINED, DIM, DO/BY/TO/WHILE/UNTIL, DWORD, ENTRY, EXIT, EXTERNAL, FIXED, FLOWS, GENERATE, GENERATED, GOTO, HWORD, IF/THEN/ELSE, INITIAL, INTERNAL, LENGTH, LOCAL/NONLOCAL, MAX/MIN, NOSEQFLOW, OPTIONS, POINTER, POSITION, PROCEDURE, REENTRANT, REFS/SETS/DEFS, REGISTER, RESPECIFY, RESTRICTED/UNRESTRICTED, RETURN [TO], SAVE/SAVEAREA, SIGNED/UNSIGNED, STATIC, VLIST, WORD — [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- PROC options: REENTRANT, CODEREG/NOCODEREG, DATAREG/NODATAREG, SAVE/NOSAVE, SAVEAREA/NOSAVEAREA, ID, OS/DOS, ENTREG, RETREG, PARMREG, SAVEREG, PROLOG/NOPROLOG, EPILOG/NOEPILOG, AUTODATA, KEY. External procedure = one CSECT; standard OS linkage (R15 entry, R14 return, R13 save area, R1 parm list); REENTRANT obtains a dynamic area mapped by DSECT `@DATD` — [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- Register variables: REGISTER attribute binds a variable to a GPR; RESPECIFY releases/restricts registers; RESTRICTED keeps the compiler from allocating a register — [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- GENERATE: "The 'simple' GENERATE includes the assembler text in parentheses. The 'block' GENERATE ... inserts following lines until the ENDGEN control statement appears"; may precede the PROC (for macro definitions); `GEN DATA` places data in the compiler's data area; DEFS/REFS/SETS/EXIT/FLOWS/NOSEQFLOW keywords tell the optimizer what the inline code touches — [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- Data area layout: `@DATA` (constants, arg lists, STATIC LOCAL, GENERATED data, BASED/DEFINED equates, register equates, label equates) ... `@ENDDATA`; `@DATD` (save areas, temporaries, AUTOMATIC) ... `@ENDDATD`; BASED/parameter/NONLOCAL items are defined by EQU to displacements — [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- Generated-label table (Figure 20): @00-@15 registers, @ALn arg lists, @CA/@CB/@CC/@CF/@CH/@CV constants, @DATA/@DATD, @DB/@DC/@DE/@DL do-loop labels, @EF/@EL/@ER epilog, @EPsn entry prologue, @GLn, @MAINENT, `@NMn` "Substitute name supplied if no name or duplicate name is found", @PB/@PC, `@PROLOG`, `@PSTART`, `@RCsn`/`@RFsn`/`@RTsn` IF merge/false/true paths, @SApn save areas, @SC/@SM/@SN/@SO/@SX execute targets, @TFn/@TSn temporaries, @ZTEMPS/@ZLEN/@ZT0000x, @SIZDATD; sn = statement number, pn = procedure number 1-255 — [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- Sample prologue: `@PROLOG STM @14,@12,12(@13)` / `BALR @12,0` / `@PSTART DS 0H` / `USING @PSTART,@12` / save-area chaining; epilog `@EL00001 L @13,4(,@13)` / `@ER00001 LM @14,@12,12(@13)` / `BR @14` — [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- Compile-time facilities (%): BSL spec has a "COMPILE TIME FACILITIES" chapter with `%GOTO L; /*Macro GOTO*/`, `%DCL ... FIXED/CHAR`, `%INCLUDE`; PL/S II listings show `@MACRO INCLUDE SYSLIB(M1)`, `@INCLUDE`, `@ENDINCLUDE`, statistics like "DEEPEST LEVEL OF MACRO NESTING" — [Z28-6642-0](https://bitsavers.org/pdf/ibm/360/bsl/Z28-6642-0_BSL_langSpec.pdf); [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- Bilingual macros, defined 1969: "A bilingual mapping macro is a member of a partitioned data set that contains both the assembly language description and the BSL description of a system control block. The member is arranged so that the BSL compiler ignores the assembly language portion, and the assembler treats the BSL statements as comments. BSL programs that use bilingual macros must use column 2 as left source margin." Brought in with `%INCLUDE SYSLIB(...)`, parameterised by compile-time variables (e.g. `%DCL CVTSYSC CHAR; %CVTSYSC = 'MVT';`); a DSECT-to-BSL translator program existed; D96 Systems Interface Design planned to ship bilingual OS/360 control blocks "in PVTMACS after OS/360 release 19" — [BSL Bulletin No. 7](https://bitsavers.org/pdf/ibm/360/bsl/BSL_bulletinSep69.pdf)
- Local check: MVS 3.8j SYS1.MACLIB copy holds 131 members with `*%GOTO` bilingual skips (e.g. `ISTEXLEX`: `*%GOTO EXLEXTPL;` ... `MACRO` ... `MEND` ... `*%EXLEND: ;`; also labels like `ACBBSL2`) — local grep of `/Users/mike/repos/mvs/sys1.maclib` (this session)
- `AGO .@UNREFD` (skipping unreferenced declarations in generated code) appears in MVS 3.8j modules (per local `mvs38src/TODO.md`) but is not in the 1974 guide's label table; the guides only describe an "Unreferenced Variables Table" in the listing — [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)
- BSL used a BSL runtime library (1967); PL/S II's guide describes no runtime library — code is self-contained except CALLed modules via V-cons — [BSL Library Oct67](https://bitsavers.org/pdf/ibm/360/bsl/BSL_libraryOct67.pdf); [GC28-6794-0](https://bitsavers.org/pdf/ibm/360/pls/GC28-6794-0_PLSIIguideMay74.pdf)

### Inferences
- For a clone (cf. RL/S): front end = PL/I-subset parser + %-preprocessor (PL/I-style compile-time language) operating on column-2..72 margins; back end emits as370/IFOX-compatible source using the documented @-label scheme. Byte-identical output to IBM's is unrealistic (RL/S aimed for "functionally equivalent"), but the 2,000+ modules in MVS 3.8j (PL/S comments + generated code) provide a huge test corpus — exactly the method Rand used.
- `@UNREFD` and the `PLShhmm` IDR show the 1975-80 compiler (V6-V8) differs from what the 1974 guide documents; the corpus itself is the spec for those later behaviours.

### Gaps
- No complete PL/S II grammar is published by IBM; the BSL Language Specification (1968) is the closest formal spec; the RL/S manual (1977) is the best public PL/S II-level reference (only preface and option pages read in this session, via the archive.org OCR text).
- Optimizer/register-allocation algorithm of IBM's compiler is undocumented.

## 5. Successor state today (brief)

### Takeaway
PL/X is the current descendant, used inside IBM for z/OS, CICS and other system software; it is not a customer product.

### Cited Findings
- PL/X: "A PL/I-like programming language used internally at IBM for parts of mainframe operating systems and other systems software, including CICS. The successor to PL/AS." — [SDS Mainframe Dictionary](https://support.sdsusa.com/support/dictionary/p/)
- Wikipedia notes the Guide to PL/S II "is very out of date with respect to the PL/X language in use today" — [Wikipedia IBM PL/S](https://en.wikipedia.org/wiki/IBM_PL/S)
- PL/X licences to outside companies "apparently IBM doesn't even sell anymore" (HN, 2020) — [HN 23674599](https://news.ycombinator.com/item?id=23674599)

### Inferences
- No legal route to an IBM PL/S/PL/X compiler exists for hobbyists today.

### Gaps
- No primary IBM source on current PL/X licensing.
