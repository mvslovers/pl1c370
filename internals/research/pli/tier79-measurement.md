# Tier 79: the 48-character set

PL/I (F) reads source written in the 48-character set when the compiler
option CHAR48 is given (GC28-6594-7, "Compiler Options"; GC28-8201-4,
Part II, Section B). Its 48-character-set processor translates the
source into the 60-character set before any other phase. With CHAR48 the
two sets may be mixed (rule 4 of the Section B notes).

Measured 2026-10-14 on MVSTK5-BLD with the probe compiled with `PARM='LOAD,NODECK,CHAR48,MACRO'` (JOB01212). The test
`t222c48` is the probe, with the same PARM in `t222c48.parm`, identical
in JOB01215.

## The rules

| 48-character set | 60-character set |
|---|---|
| `GT GE LT LE NE NL NG` | `> >= < <= ¬= ¬< ¬>` |
| `CAT OR AND NOT` | `\|\| \| & ¬` |
| `PT` | `->` |
| `..` | `:`, as in a label `L..` and in bounds `C(2..4)` |
| `,.` | `;`, but not before a digit: `LIST(1.5,.5)` is two items |
| `//` | `%`: `//DCL K FIXED,. //K = 4,.` is a preprocessor statement |

- **Strings and comments** are not translated: `INIT('A;B:')` keeps its
  characters.
- **60-character symbols** may stand beside them:
  `PUT SKIP LIST(A * B ** 2 / 5 - 1);` ends with `;`.

## In PL1C/370

- **`-char48`** (`src/main.c`) is the option, for PL/I (F)'s CHAR48.
- **`char48` in `src/lex.c`** translates every stream after it is read,
  %INCLUDE members included, before the preprocessor and the lexer:
  - the words are taken as whole identifiers;
  - with CHAR48 they are reserved, as in PL/I (F).
- **tests/run_mvs.py** reads `<name>.parm`. It is PL/I (F)'s PARM for
  the test, and `CHAR48` in it passes `-char48` to PL1C/370.

## Deviation

- **A word next to another character:** PL/I (F) requires the reserved
  words to have a blank or a comment on either side, and leaves the
  rest undefined (rule 5). PL1C/370 translates any word that stands
  alone as an identifier would, so `A GT(B)` is `A >(B)`. Not measured.

## The suite

All 224 programs are identical: JOB01218 to JOB01226.
