# Reference manuals (local only)

The manuals PL1C/370 is built against, kept here so that every session
finds them in the same place. **The PDFs are not in git** (`.gitignore`):
IBM manuals and other scans are linked, never committed (CLAUDE.md), and
some texts are licensed to one user (the ANSI standard). Each file can be
fetched again from the URL beside it. On 2026-10-15 `curl` got a 403
from `www.bitsavers.org` without a user agent, and the file from
`bitsavers.org` with `-A "Mozilla/5.0"` (which of the two mattered was
not tried).

What each manual is for: `../research/pli/manuals_inventory.md`. Text for
searching: `pdftotext -layout <file> <file>.txt` (the scans have an OCR
layer; check a finding against the page).

| File | Source |
|---|---|
| `C28-6590-0_PLIcompuSbr_1966.pdf` | https://bitsavers.org/pdf/ibm/360/pli/C28-6590-0_PLIcompuSbr_1966.pdf |
| `C33-0002-0_PLI_F_Version_5_Planning_Guide.pdf` | https://bitsavers.org/pdf/ibm/360/pli/C33-0002-0_PLI(F)plGd_Mar69.pdf |
| `GC28-6594-7_PLIf_pgGd_Jan71.pdf` | https://bitsavers.org/pdf/ibm/360/pli/GC28-6594-7_PLIf_pgGd_Jan71.pdf |
| `GC28-8201-2_PLIrefMan_Oct69OCR2009.pdf` | https://www.prycroft6.com.au/misc/download/GC28-8201-2_PLIrefMan_Oct69OCR2009.pdf |
| `GC28-8201-4_PLI_F_Language_Reference_Dec72.pdf` | https://bitsavers.org/pdf/ibm/360/pli/GC28-8201-4_PLI_F_Language_Reference_Dec72.pdf |
| `GC33-0001-2_PLIgenInf_Jun73.pdf` | https://bitsavers.org/pdf/ibm/370/pli/GC33-0001-2_PLIgenInf_Jun73.pdf |
| `GC33-0003-0_PLI_Checkout_Compiler_General_Info_Jul70.pdf` | https://bitsavers.org/pdf/ibm/370/pli/GC33-0003-0_PLI_Checkout_Compiler_General_Info_Jul70.pdf |
| `GC33-0004-0_PLI_Optimizing_Compiler_General_Info_Mar70.pdf` | https://bitsavers.org/pdf/ibm/370/pli/GC33-0004-0_PLI_Optimizing_Compiler_General_Info_Mar70.pdf |
| `GC33-0009-4_PLI_Checkout_And_Opt_Compiler_Lang_Ref_Oct76.pdf` | https://bitsavers.org/pdf/ibm/360/pli/GC33-0009-4_PLI_Checkout_And_Opt_Compiler_Lang_Ref_Oct76.pdf |
| `GY28-6800-5_PL1_F_Program_Logic_Manual_197112.pdf` | https://bitsavers.org/pdf/ibm/360/pli/GY28-6800-5_PL1_F_Program_Logic_Manual_197112.pdf |
| `GY28-6801-6_PL1_Subroutine_Library_Program_Logic_Manual_197206.pdf` | https://bitsavers.org/pdf/ibm/360/pli/GY28-6801-6_PL1_Subroutine_Library_Program_Logic_Manual_197206.pdf |
| `Hughes_PLI_Programming_1973.pdf` | https://bitsavers.org/pdf/ibm/360/pli/Hughes_PLI_Programming_1973.pdf |
| `LY27-9528-0_OS_PLI_Version_2_Problem_Determination_Dec87.pdf` | https://bitsavers.org/pdf/ibm/370/pli/LY27-9528-0_OS_PLI_Version_2_Problem_Determination_Dec87.pdf |
| `LY33-6011-1_PLI_Resident_Library_PLM_Oct76.pdf` | https://bitsavers.org/pdf/ibm/370/pli/LY33-6011-1_PLI_Resident_Library_PLM_Oct76.pdf |
| `SC26-3113-01_PLI_for_MVS_and_VM_Release_1.1_Programming_Guide_199506.pdf` | https://bitsavers.org/pdf/ibm/370/pli/SC26-3113-01_PLI_for_MVS_and_VM_Release_1.1_Programming_Guide_199506.pdf |
| `SC26-3114-01_PLI_for_MVS_and_VM_Release_1.1_Language_Reference_199506.pdf` | https://bitsavers.org/pdf/ibm/370/pli/SC26-3114-01_PLI_for_MVS_and_VM_Release_1.1_Language_Reference_199506.pdf |
| `SC26-3118-01_PLI_for_MVS_and_VM_Release_1.1_Migration_Guide.pdf` | https://publibfp.dhe.ibm.com/epubs/pdf/ibm3m101.pdf (Compiler and Run-Time Migration Guide; it compares with OS PL/I V2R3) |
| `SC26-4121-1_PLI_Release_5_Installation_Guide_for_MVS_Sep1985.pdf` | https://bitsavers.org/pdf/ibm/370/pli/SC26-4121-1_PLI_Release_5_Installation_Guide_for_MVS_Sep1985.pdf |
| `SC33-0006-0_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep71.pdf` | https://bitsavers.org/pdf/ibm/370/pli/SC33-0006-0_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep71.pdf |
| `SC33-0006-3_PLIpgmGde_Dec74.pdf` | https://bitsavers.org/pdf/ibm/370/pli/SC33-0006-3_PLIpgmGde_Dec74.pdf |
| `SC33-0006-7_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep85.pdf` | https://bitsavers.org/pdf/ibm/370/pli/SC33-0006-7_OS_PLI_Optimizing_Compiler_Programmers_Guide_Sep85.pdf |
| `SC33-0009-2_PLIoptRef_Sep72.pdf` | https://bitsavers.org/pdf/ibm/370/pli/SC33-0009-2_PLIoptRef_Sep72.pdf |
| `SC33-0025-2_PLIexLgic_Apr73.pdf` | https://bitsavers.org/pdf/ibm/370/pli/SC33-0025-2_PLIexLgic_Apr73.pdf |
| `SC33-0025-3_PLI_Optimizing_Compiler_Execution_Logic_Sep85.pdf` | https://bitsavers.org/pdf/ibm/370/pli/SC33-0025-3_PLI_Optimizing_Compiler_Execution_Logic_Sep85.pdf |
| `SC33-0026-0_PLI_Optimizing_Compiler_System_Info_Sep71.pdf` | https://bitsavers.org/pdf/ibm/370/pli/SC33-0026-0_PLI_Optimizing_Compiler_System_Info_Sep71.pdf |
| `SC33-0027-2_PLIcmsMsg_Jun73.pdf` | https://bitsavers.org/pdf/ibm/370/pli/SC33-0027-2_PLIcmsMsg_Jun73.pdf |
| `SC33-0027-5_OS_PLI_Optimizing_Compiler_Messages_Feb82.pdf` | https://bitsavers.org/pdf/ibm/370/pli/SC33-0027-5_OS_PLI_Optimizing_Compiler_Messages_Feb82.pdf |
| `SC33-0029-3_OS_PL1_Optimizing_Compiler_TSO_Users_Guide_Jul1979.pdf` | https://bitsavers.org/pdf/ibm/360/pli/SC33-0029-3_OS_PL1_Optimizing_Compiler_TSO_Users_Guide_Jul1979.pdf |
| `SC33-0034-2_OS_PL1_Checkout_Compiler_Messages_Oct1976.pdf` | https://bitsavers.org/pdf/ibm/360/pli/SC33-0034-2_OS_PL1_Checkout_Compiler_Messages_Oct1976.pdf |

Not here: **ANSI X3.53-1976 (R1998)**, the maintainer's copy at
`~/ANSI-X3_53-1976_R1998.pdf`. It is licensed to one user ("Copying and
networking prohibited"): it is not copied, not linked, and only cited
by section.

The language target and which manual decides what: CLAUDE.md, "Language
target". PL/I for MVS & VM (SC26-3114-01, 1995) is the reference for what came
after the Optimizing Compiler. No OS PL/I Version 2 Language Reference
(SC26-4308) or Programming Guide (SC26-4307) is online (searched
2026-10-15: bitsavers, archive.org, IBM's publibfp, computinghistory.org.uk;
the V2R2 General Information GC26-4313-3 is sold there as a scan). IBM
hosts the Enterprise PL/I Language References from V3R1 on
(publibfp.dhe.ibm.com/epubs/pdf/ibm3lr00.pdf and on); they are not here.
