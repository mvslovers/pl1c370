# Tier 45: unaligned bit strings, packed; the B format item

Measured 2026-10-13 on MVSTK5-BLD. The probes are:

- Q1BMAP: nine structures and an array, written to
  REPORT (JOB00745)
- Q1BFMT: B in PUT EDIT and GET EDIT (JOB00754)

The tests are identical:

- `t147bar`: JOB00741 found the difference, JOB00753 is identical
- `t148bmp`, which is Q1BMAP: JOB00749
- `t149bfm`: in the full suite, JOB00756 to JOB00759

## How PL/I (F) maps unaligned bit strings

A bit string without ALIGNED is aligned on a bit. PL/I (F) maps a
structure with the same pairing algorithm as in Tier 5, but in bits: a
unit is shifted, within a doubleword, by multiples of its largest
alignment. Every byte of Q1BMAP's REPORT follows from that.

| Declaration | Bytes | Where the bits lie |
|---|---|---|
| `2 A BIT(3), 2 B BIT(2), 2 C CHAR(1)` | `1D C1` | A, B at bits 3 and 6 of byte 0: the unit is shifted so that C starts a byte |
| `2 A CHAR(1), 2 B BIT(3), 2 C FIXED BIN(31)` | `C2 A0 00000007` | B at bit 0 of byte 1, padded; C at byte 2 (the structure's phase puts it on a fullword) |
| `2 A BIT(1), 2 B FIXED BIN(15), 2 C BIT(2)` | `01 0003 C0` | A at bit 7, B at byte 1, C at byte 3 |
| `2 A(3) BIT(3), 2 B BIT(2) ALIGNED, 2 C CHAR(1)` | `01 11 C0 C3` | the array packed into 9 bits ending at a byte; ALIGNED starts a byte |
| `2 T(3), 3 P BIT(2), 3 Q BIT(1), 2 C CHAR(1)` | `01 6D C4` | an array of 3-bit structures, packed |
| `2 V, 3 W BIT(4), 3 X BIT(5), 2 Y CHAR(1), 2 Z BIT(7)` | `01 F1 C5 AA` | Z left-aligned in the last byte |
| `2 A BIT(9), 2 B BIT(9), 2 C BIT(9), 2 D CHAR(1)` | `07 FC 03 01 C6` | 27 bits after 5 bits of padding |
| `X5(5) BIT(3)`, not in a structure | `B6 DE` | packed from the first bit |
| `2 A FIXED DEC(3), 2 B BIT(4), 2 C FLOAT DEC(6)` | `012C 90 41100000` | B in the byte after A |

PL1C/370 used to give every bit string its own bytes (`t147bar`:
11 bytes where PL/I (F) has 6).

**A packed element passed as an argument goes by reference.** In t147bar,
`CALL SHOW(BA(2))` with `X = '000'B` in SHOW clears BA(2) in the caller.

## In PL1C/370

lower.c maps every item in bits (`layout_bits`) and derives the byte
fields as before, so a structure without unaligned bit strings maps
exactly as it did. A BIT leaf is *packed* when either holds:

- its chain has a bit offset or a stride that is not whole bytes;
- its last byte holds another member's bits.

A packed leaf is reached through its bit offset, a constant or computed
at run time. In gen.c, `gen_place` adds bits, then splits off the byte;
`rt/pl1rt.c` has `PL1BGET`, which copies the bits to an aligned
temporary, and `PL1BPUT`, which stores them and keeps the bits around
them. GET into a packed target goes through a temporary.

A packed argument is passed by reference through a copy: the caller
copies the bits, passes the copy, and writes it back after the call.
Refused, for now:

- ADDR of a packed item;
- SUBSTR of one as a target;
- INITIAL of one in STATIC;
- DEFINED with packed items.

The storage of a level-1 item with packed bits is `tbits` bits, rounded
up to bytes. ALIGNED and UNALIGNED are attributes now.

## The B format item

From Q1BFMT:

- **`B`** writes the bit string's characters 1 and 0, as many as it has.
  A BIT(10) VARYING holding three bits writes three characters.
- **`B(w)`** left-justifies them in w columns and pads with blanks on the
  right, or cuts on the right: B(3) of `10011` is `100`.
- **A character string with B** is converted to bits: '1101' writes
  `1101`.
- **An arithmetic value with B** is converted to a bit string first:
  FIXED BIN(15) 5 with B(4) writes `0000`, the first four of its fifteen
  bits.
- **GET EDIT `B(w)`** reads w characters as a bit string and assigns it:
  `1101` into BIT(6) is `110100`, `10101111` into BIT(6) is `101011`.
  `011` into BIT(8) VARYING is three bits long.

PL1C/370 parses B and B(w): they work as A and A(w) on the characters of
the bits. `PL1GVBT` reads into BIT VARYING.

## Not done

- B with an arithmetic value (PL1C/370 stops with a message).
- B(w) and other widths given by an expression rather than a constant.
- BIT(*) arguments that are packed are copied, not written back.
