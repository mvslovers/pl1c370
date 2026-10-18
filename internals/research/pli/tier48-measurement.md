# Tier 48: external procedures compiled apart

Tier 47 compiled several external procedures of one source together.
This tier compiles them apart and links them, as a program of several
sources is built. PL/I (F) is the oracle through its batched
compilation (Q2EXT, JOB00781). The test `t156sep` is Q2EXT with one
change (below). On PL1C/370's side the runner splits it at its
`*PROCESS` cards and compiles each piece alone (`t156sep.sep`). It is
identical (JOB00792).

## In PL1C/370

- **A call to a procedure that is not in the source** goes through
  `=V(name)`. The ENTRY declaration stands in for the procedure:
  - its descriptors are the parameters, so arguments are converted or
    passed as with a procedure of the source;
  - a CHAR(*) descriptor takes the argument's descriptor;
  - an empty descriptor (`ENTRY(,FLOAT BIN,FLOAT BIN)`, as in IBM's
    SYS1.SAMPLIB(PL1SAMP)) passes the argument as it is;
  - so does an ENTRY declaration without descriptors.

  An external name may have at most eight characters.
- **An external procedure's name is an entry point** (`ENTRY name`) at
  its prologue. A source without OPTIONS(MAIN) is compiled with no
  program entry.
- **An EXTERNAL variable is a section of its name:**
  - an arithmetic scalar without INITIAL is a common area (`COM`);
  - anything else is a control section (`CSECT`): its INITIAL values, or
    zeros.

  The linkage editor then does what Tier 47 measured: a control section
  wins over a common area, and of two control sections the first in link
  order is kept. Compiled together, one source still emits each name once.
- **A file constant's descriptor is a control section** named `#` and
  seven characters of the file's name, so all procedures that name the
  file, compiled together or apart, use one descriptor and one print
  line.
- **The CHAR(*) parameters of an external procedure** get the bound of
  an expression (256) at least, since callers compiled apart are not
  seen.

## Found on the way: ld370

ld370 handles a common area and a control section of the same name in
link order:

- COM, then CSECT: correct, the control section with its data (checked).
- **CSECT, then a later COM of that name:** the section is turned back
  into a common area, and its INITIAL value is lost. IEWL keeps the
  control section.

Reported to the cc370 session with a three-object reproduction
(`merge_common` in ld370). `t156sep` therefore differs from Q2EXT in one
place: SQ does not declare LEVEL again, but NAME.

## Not done

- External names of more than eight characters (refused).
- A REGIONAL or INDEXED file shared by procedures compiled apart is
  untested.
