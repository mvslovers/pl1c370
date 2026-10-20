#import "../bookmaster/bookmaster.typ": *

= Introducing PL1C/370 <pg-intro>

#idx("PL1C/370")
PL1C/370 compiles PL/I for MVS 3.8j on a workstation running macOS or
Linux. The compiler, #cmd("pl1c370"), translates a PL/I source into
System/370 assembler language. The CC/370 assembler, as370, turns that
into an object module, and the CC/370 linkage editor, ld370, links it
with the PL1C/370 run time into an MVS load module. None of these steps
needs a connection to MVS; only running the program does.

== The Parts <pg-intro-parts>

#deflist(width: 1.35in,
  [#cmd("pl1c370")], [The compiler. It reads one PL/I source and writes one
    assembler source, by default the source's name with #cmd(".asm").],
  [#cmd("pl1rt.a")], [The run time: conversions, stream and record
    input/output, conditions and the built-in functions, written in C on
    LIBC/370. The linkage editor takes from it only the members a
    program refers to.],
  [#cmd("pl1abi.h")], [What the compiler and the run time agree on, with its
    interface level. Every compiled program refers to that level, so a
    program and a run time of different levels do not link together.],
)

== The Language <pg-intro-lang>

By default PL1C/370 compiles the language of IBM PL/I for MVS & VM,
without what exists only in Language Environment and without DBCS. With
the option #cmd("-lang=F") it compiles exactly the language of IBM PL/I
(F) Version 5, and refuses anything beyond it. The _PL1C/370 Language
Reference_ describes both.

// Draft: the path of a program from source to SYSPRINT, with a captured
// session under ex/, follows with the next release.
