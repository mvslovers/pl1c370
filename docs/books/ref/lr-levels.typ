#import "../bookmaster/bookmaster.typ": *

= Language Levels <lr-levels>

#idx("language levels")
Every part of the language that PL1C/370 accepts belongs to one of four
levels:

#deflist(width: 0.8in,
  [F], [IBM PL/I (F) Version 5. Its behaviour is the measure: PL1C/370
    compares its programs with those of PL/I (F).],
  [OPT], [IBM OS PL/I Optimizing Compiler: not in PL/I (F); PL/I for MVS &
    VM has it too.],
  [MVS], [IBM PL/I for MVS & VM: in neither PL/I (F) nor the Optimizing
    Compiler.],
  [ISO], [The standard ANSI X3.53-1976 (ISO 6160) only: in none of the IBM
    compilers above.],
)

By default all four levels are accepted, without what exists only in
Language Environment (the CEE services and LE's messages) and without
DBCS (GRAPHIC and MIXED data). With #cmd("-lang=F"), only the level F is,
and the compilation fails at the first thing beyond it.

This chapter is in preparation. It will list every item above the level
F, and every point where PL1C/370 behaves otherwise than PL/I (F).
