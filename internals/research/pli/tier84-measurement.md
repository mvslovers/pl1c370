# Tier 84: STRINGSIZE and the PARM after the IBM rules

The two deviations Tier 83 found between the default language (PL/I for
MVS & VM, which includes the Optimizer's) and PL1C/370 (#189). `-lang=F`
keeps PL/I (F)'s behaviour in both.

## The rules, from the manuals

- **STRINGSIZE:** "disabled by default and within the scope of a
  NOSTRINGSIZE condition prefix. It is enabled only within the range of
  a STRINGSIZE condition prefix" -- GC33-0009-4 (Part II, Section H)
  and SC26-3114-01 (chapter 15) alike. Standard system action: "A
  message is printed and processing continues". The Optimizer's
  messages (SC33-0027-5): IBM440I `'STRINGSIZE' CONDITION RAISED BY
  'SIGNAL' STATEMENT.` and IBM441I `'STRINGSIZE' CONDITION RAISED`.
  Condition code 150 in both (SC26-3114-01: "SIGNAL STRINGSIZE statement
  executed or STRINGSIZE condition occurred"; 151 is for mixed strings,
  DBCS).
- **The PARM:** "place the argument, preceded by a slash, after the
  execution-time options ... If you wish to pass an argument without
  specifying options, it should be preceded by a slash" (SC33-0006-7,
  p. 30). PL/I for MVS & VM: "optional run-time options, followed by
  a slash (/), followed by optional characters to be passed to the MAIN
  procedure" (SC26-3113-01, beside Figure 93). NOEXECOPS turns the
  splitting off.
- **PL/I (F)** has no execution-time options in the PARM. Its slash
  separates loader options from the program parameter (GC28-6594-7,
  chapter 6), which the program does not see.

## In PL1C/370

- **STRINGSIZE:**
  - `EN_STRZ` is no longer in `EN_DEFAULT` (pl1c.h). A block's flags in
    PL1NOCV start from the default (bit 4 set: disabled), and a
    statement prefix changes them for the statement, as NOCONVERSION and
    NOOVERFLOW did already.
  - `pl1i_strsize` (rt/pl1cond.c) raises the condition whenever it is
    enabled, not only when an on-unit is established; the system action
    writes IBM441I, or IBM440I when SIGNAL raised it, and returns.
  - A STRINGSIZE prefix sets `uses_strz`, so that bit strings cut in
    registers are checked as they are with ON STRINGSIZE.
- **The PARM:** for a main procedure with one CHARACTER VARYING
  parameter, without NOEXECOPS and without `-lang=F`, the prologue asks
  PL1MPRM for mode 2 (gen.c). `pl1_main_parm` (rt/pl1parm.c) then gives
  the part after the first `/`, or an empty string when there is none.
  The run-time options before it are not used.
- **The runner:** `<name>.sem` marks a `.opt.pli` whose source is PL/I
  (F)'s and only means something else in the default; `-lang=F` need
  not refuse it.

## Tests

- `t209ans` (STRINGSIZE and STORAGE): the prefix `(STRINGSIZE):` on the
  main procedure, so it shows what it showed before; the cut after
  `REVERT STRINGSIZE` now gives the system action's message. The PL/I
  (F) twin writes the message's lines, an empty record before and
  after, as PL1C/370's messages are written (JOB01309 showed the form;
  identical in JOB01312).
- `t229prm`: PARM `NOSPIE/HELLO/WORLD`, the parameter `HELLO/WORLD`
  (11 characters).
- `t230prm`: PARM `HELLO`, the parameter empty. Both identical in
  JOB01315.
- The full suite: 232 of 232 identical (JOB01318 to JOB01326).

## Not done

- A main procedure with another parameter than one VARYING string and no
  NOEXECOPS: SC26-3114-01 does not provide for it; PL1C/370 passes the
  PARM as PL/I (F) does.
- The run-time options themselves (ISASIZE, REPORT, STAE, SPIE, ...):
  PL1C/370 has none to set.
