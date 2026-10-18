# Tier 58: scaled FIXED BINARY variables

PL1C/370 refused `FIXED BIN(p,q)` variables with q > 0. Tier 1 had
measured scaled binary values in expressions, and PL1C/370 computes
those, but a variable could not be declared. Found writing `t184bfm` in
Tier 57.

Measured 2026-10-13 on MVSTK5-BLD with the probes.
The tests are identical:

| Probe | Job | Test | Job |
|---|---|---|---|
| Q1SBIN | JOB00924 | `t185sbn` | JOB00929 |
| Q2SCNV | JOB00927 | `t186scv`: Q2SCNV, with STATIC and FLOAT | JOB00929 |
| | | `t187sgt`: GET LIST, EDIT and DATA | JOB00933 |

## The rules

- **A FIXED BINARY(p,q) variable** holds its value times 2**q in a
  halfword (p <= 15) or a fullword. `FIXED BIN(15,3) INIT(5.5)` holds
  44 and prints `5.5`. Output follows the conversion of Tier 1, to FIXED
  DECIMAL(1+CEIL(p/3.32), CEIL(q/3.32)), truncated:
  - `FIXED BIN(15,3)` prints one decimal: 0.3 is `0.2`, -1.875 is
    `-1.8`;
  - `FIXED BIN(31,16)` prints five.
- **A decimal value assigned to one** is converted to the target's scale
  directly, truncated (Q2SCNV): 1.1 into FIXED BIN(31,16) is
  72089/65536, which prints `1.09999`. That holds for a constant, a
  FIXED DECIMAL variable of any scale and a decimal expression
  (`1.1 + 0`).
- **In an expression with a binary operand,** the decimal operand still
  takes the scale of the conversion rule: `I * 1.1` is `3.18750`, 1.1
  being FIXED BINARY(8,4) there.
- **FLOAT assigned to one** is converted at the target's scale too:
  1.1E0 into FIXED BIN(31,16) is `1.09999`.
- **STATIC INITIAL** values are converted at the variable's scale:
  `FIXED BIN(31,10) STATIC INIT(3.1416)` is `3.1406`.
- **Scaled binary in other uses** behaves as expected:
  - a DO loop `DO T = 0.5 TO 2 BY 0.5` gives 0.5, 1.0, 1.5, 2.0;
  - comparisons, PUT EDIT F, PUT DATA and UNSPEC work;
  - so does assignment to an integer, which truncates: -1.875 is -1.
- **GET** converts the field to the target's scale, truncated: 0.3 into
  FIXED BIN(15,6) is 19/64 (`'0000000000010011'B`).
- **A FLOAT constant read into a FIXED target** takes its exponent:
  `U=1E2` is 100, and `25E-2` is 0.25. This holds for FIXED DECIMAL
  too: `1E-5` into FIXED DEC(9,6) is `0.000010`. PL1C/370 raised
  CONVERSION there before.

## In PL1C/370

- **parse.c** keeps the scale of a FIXED BINARY declaration, from 0 to
  31.
- **The store of a FIXED DECIMAL value** into a FIXED BINARY target
  converts it at the target's scale (`gen_bin_at`). Before, it went
  through the scale of the conversion rule and was rescaled. The first
  full run (JOB00935 to JOB00938) found one slip: for an integer target
  (scale 0) the decimal fraction was not shifted off. `I = P` with P =
  123.45 gave 12345 (`t2dec`), and `B = P` with the numeric picture
  P = 7.25 gave 725 (`t91pctx`). Fixed; both identical again
  in JOB00940.
- **STATIC INITIAL** converts constants at the variable's scale.
- **The run time:**
  - `PL1GFXB` gets the scale with the precision (p + 256 q). It computes
    the fraction's bits by doubling its decimal digits.
  - `parse_number` takes an exponent.

## The suite

All 189 programs are identical: JOB00942 to JOB00945, and JOB00947.
In JOB00943, PL/I (F) itself ended abnormally compiling the 86th program
(IEA703I 106-F, module IEMML: storage, as in JOB00829). That flushed the
job's last 14 programs. They ran again in JOB00947.

## Not done

- A negative scale or one above 31.
- B format of a scaled binary value (Tier 57's B passes p - q bits; not
  tested with q > 0).
