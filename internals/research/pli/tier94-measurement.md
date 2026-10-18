# Tier 94: DISPLAY ... REPLY

Issue #218, block A. The DISPLAY statement (GC28-8201-4, Section J,
"The DISPLAY Statement"; GC28-6594-7, "Communication with Program
during Execution" and Appendix J).

## The rules

- `DISPLAY (element-expression);` writes the message to the operator,
  `DISPLAY (e) REPLY (c) [EVENT (ev)];` asks for a reply into the
  character variable c and waits for it, or with EVENT goes on until a
  WAIT for ev. REPLY and EVENT may be given in either order.
- The manuals: the message is converted to a varying string of at most
  72 characters, the reply at most 126; "the reply string's current
  length is set equal to its maximum length and padded with blanks".
- Route code 2 for DISPLAY, 1 for DISPLAY with REPLY, descriptor code 7.
- ONCODE 1005 / IHE025I: "message length for DISPLAY statements is
  zero", "appears only if the REPLY option is exercised".

## Measured on PL/I (F) V5.5

Probes. The replies were given at the console of
MVSTK5-BLD through mvsMF (`mbt target console m38b -- 'R nn,text'`);
`D R,L` lists each outstanding reply id with the message text.

- **Q1DSP, JOB01915:**
  - The messages appear in the job log as `+text`, a WTOR as `@nn text`.
  - FIXED BIN(15) 42 is shown as `       42`, FLOAT DEC(6) 1.5 as
    ` 1.50000E+00`, BIT '1010'B as `1010`: the standard conversion to
    character.
  - An 87-character message is shown whole, not cut to 72.
  - CHAR(10) gets `ABC` padded; CHAR(20) VARYING gets length 20,
    `HELLO WORLD` padded; CHAR(3) VARYING length 3.
  - A reply longer than the variable is refused by MVS itself (the WTOR
    carries the variable's length): `R 03,ABCDEFG` was not taken, the
    WTOR stayed outstanding.
  - With EVENT, the reply variable is blank right after the DISPLAY
    (it held `UNCHANGED`), COMPLETION is '0'B, and after the WAIT '1'B
    with the reply in place. EVENT before REPLY works too.
- **Q2DSZ, JOB01916:** an empty message without REPLY writes nothing
  and raises nothing. With REPLY, an empty message, and a CHAR(0)
  VARYING reply variable, each give IHE025I `DISPLAY - MESSAGE OR REPLY
  AREA LENGTH ZERO AT OFFSET ...`, then ERROR with ONCODE 1005.
- **Q3DSL, JOB01916:** a 200-character message reaches the job log cut
  to 107 characters, followed there by blanks: the job log line ends at
  column 132, 112 characters of message (JOB01920 shows 112). PL/I (F)
  passes 126 (below), so the cut is the job log's. A CHAR(130) reply
  variable filled with X is all blanks but for the reply: the whole
  variable is cleared, not only the first 126.
- **What PL/I (F) does, measured:** a message is cut to 126
  characters, and so is the reply length in the WTOR; the reply
  variable is set to blanks over its full length before the WTOR; a
  VARYING reply gets its maximum length as its current length; a
  message of length zero shows nothing without REPLY and is an error
  with it; without EVENT the program waits for the reply.

## In PL1C/370

- **parse.c:** the DISPLAY statement, REPLY and EVENT in either order.
  EVENT without REPLY is an error. With REPLY, EVENT is refused: PL1C/370
  has no event variables yet (the EVENT attribute is not taken either;
  multitasking, #228).
- **check.c:** the message an element string, bit string or arithmetic
  value; the reply an element CHAR or CHAR VARYING variable.
- **gen.c** converts the message as PUT LIST does (gen_string) and calls
  `PL1DSP` (rt/pl1dsp.c), which issues the WTO or WTOR (SVC 35,
  ROUTCDE 2 or 1, DESC 7) and WAITs, as IHEDSP does (126 characters
  either way, the area blanked, VARYING at its maximum length, IHE025I
  and ONCODE 1005).

## Test

`t248dsp`, with its replies in `t248dsp.reply`: `tests/run_mvs.py`
runs it in a job of its own and answers its WTORs at the console of
m38b, only those whose text begins with the test's name. It also
compares the console messages of the two steps from the job log (for
programs with DISPLAY only: LIBC/370's malloc writes "Out of memory"
and a traceback to the console when the heap is exhausted, t209ans).
Identical in JOB01920, twelve console messages each, and again in
JOB01946. The full suite 246 of 248 in JOB01923 to JOB01941; the two
left over: `t143sky`, PL/I (F)'s compile ending RC 16 on m38b, and
`t209ans`, the console messages above (compared then for every
program); both identical in JOB01944 and JOB01946.
