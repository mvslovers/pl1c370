# Tier 96: sterling data

Issue #219, block A. Sterling constants and sterling pictures
(GC28-8201-4, Section B "Sterling Fixed-Point Data" and Section D
"Sterling Pictures", Figure D-9). The Optimizing Compiler dropped them;
PL1C/370 keeps them in both languages (`docs/language-levels.md`).

## The rules

- A **sterling constant** is pounds.shillings.pence L, all three fields
  present, at most 13 digits of pounds, shillings below 20, the pence's
  integer part below 12 with an optional fraction. Its value is FIXED
  DECIMAL pence, pounds * 240 + shillings * 12 + pence, the precision
  that of the result.
- A **sterling picture** is `G [edit 1] M pounds M [separator 1]
  shillings M [separator 2] pence [edit 2]`. Its value is pence. 8 is a
  shilling digit in one position (ten `&`, eleven to nineteen `A` to
  `I`), 7 a pence digit in BSI form (ten `&`, eleven `-`), 6 in IBM form
  (ten `-`, eleven `&`). H writes `S`, P writes `D`. Zero suppression
  covers the whole item; the separators `/ . B H` are never suppressed;
  Z is not allowed right of 6, 7, 8 or 9.

## Measured on PL/I (F) V5.5

Probes.

- **Q1STL, JOB01230:** the constants 2.4.6L, 101.13.8L, 0.0.2.5L and
  1.10.0L are 534, 24404, 2.5 and 360; five pictures for 534, 19, 131,
  -534 and 0 pence (the table in TODO.md of 2026-10-14, kept in the
  test). 131 is 0.10.11: `0&-` in 8/7, `0&&` in 8/6. `GMZZM.ZZM.ZZP` of
  0 is `  .  .  D`.
- **Q2STL, JOB01978:**
  - The precision: `P3 + 0` of `GM99M/99M/99` converts to a string of
    9 characters, so P3 is FIXED(5); `GMZ9M.99M.99V99` gives 11, so
    FIXED(7,2). The pounds' digits and 3, the pence's fraction.
  - `G$MZZZM.ZZMH99V9P`, `GM$$$9M.99M.9T` (a drifting `$` in the
    pounds, T over the last pence digit: 534 ends `0F`, -534 `0O`),
    `GMZ9M.99M.99V99`, `GMZ9MBY9MB99B`, for 534, 131.5, -534, 2399.
    Pence beyond the picture's scale are truncated (131.5 in `9T` is
    `1A`).
  - `G$MZZ9M.ZZ...` is refused (IEM0745I: Z right of 9), H inside the
    shillings field too (IEM0735I). A constant too large for the
    picture is a compile-time error (IEM1808I).
  - A-format input into a sterling picture converts the field as a
    character string: `12/03/04` raises CONVERSION (IHE604I).
- **Q3STL, JOB01980, JOB01982:**
  - GET EDIT with the P format: `12/03/04` by `P'GM99M/99M/99'` is
    2920, `  0&-` by `P'GMZZ9M8M7'` 131, ` 1.02.0325` by
    `P'GMZ9M.99M.99V99'` 267.25.
  - GET LIST of `2.4.6L` is CONVERSION (ONCODE 613); a CHAR `2.4.6L`
    assigned to FIXED DECIMAL also (612): sterling constants exist only
    in the source.
- **Q4STL, JOB01984:**
  - The P format for output, `CR` and `V.9` included.
  - `2.4.6L * 2` is 1068, `1.0.0L / 3` 80.000000000000, `0.0.2.5L +
    1.0.0L` 242.5.
  - 240000 pence into `GMZZ9M8M7` (at most 999.19.11) raises SIZE
    (ONCODE 340), and the picture then holds `00000`: no zero
    suppressed. 239999 is `999I-`.
  - `GMZZM.ZZM.ZZP` of -19 is `  . 1.07D`, and its value 19: without a
    sign character the sign is lost.

## In PL1C/370

- **lex.c** makes a sterling constant the decimal constant of its pence.
- **parse.c** takes a picture beginning with G; its precision and scale
  come from rt/pl1pic.c (`pl1_pic_digits`, `pl1_pic_scale`).
- **rt/pl1pic.c**: `sterling_form` turns a sterling picture into one of
  the ordinary kind (G and M left out, the separators and P literal
  characters, 8, 7 and 6 a 9 whose digit is the field's character);
  editing and reading go through the ordinary code with the pounds,
  shillings and pence worked out from the pence and back. Pounds lost
  to SIZE leave no zero suppressed.
- **SIZE** for every numeric picture target (gen.c): beyond 10**p, or
  for a sterling picture 240 * 10**(p-3) pence.
- **GET into a numeric picture** (gen.c) and **the P format in GET
  EDIT** (rt/pl1ioget.c, the field the picture's width, its value as the
  picture reads it) were missing for every picture; both are added.

## Test

`t249stl`: the constants, the nine pictures for seven values, the
precision, PUT LIST and DATA, the P format both ways, GET LIST of a
sterling constant (CONVERSION), SIZE for a sterling and an ordinary
picture. Identical in JOB01991; the full suite 250 of 250 in JOB01994
to JOB02012.
