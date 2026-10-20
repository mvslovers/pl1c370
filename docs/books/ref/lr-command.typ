#import "../bookmaster/bookmaster.typ": *

= The pl1c370 Command <lr-command>

#idx("pl1c370", "command")
#cmd("pl1c370") [_options_] _source_

The command compiles one PL/I source into one assembler source. It ends
with 0 when the source compiled and with 8 when it did not. The messages
go to the standard error: an error in the source with the source's name
and line, as _name_(_line_); an error in the options with
#cmd("pl1c370:").

#tab(caption: [The options of pl1c370])[
  #table(columns: (1.3in, 1fr),
    [Option], [Effect],
    [#cmd("-o") _file_], [The assembler source to write. By default the
      source's name with #cmd(".asm") in place of its extension.],
    [#cmd("-I") _dir_], [A directory to search for %INCLUDE members; up to
      16.],
    [#cmd("-MD")], [Also write a make dependency file: the output's name
      with #cmd(".d"), naming the source and every %INCLUDE member read.],
    [#cmd("-MF") _file_], [The name of the dependency file.],
    [#cmd("-lang=F")], [The language of PL/I (F) Version 5 only.],
    [#cmd("-langlvl=SPROG"), #cmd("-langlvl=NOSPROG")], [PL/I for MVS & VM's
      LANGLVL(SPROG): pointer arithmetic, POINTERADD, POINTERVALUE,
      BINARYVALUE. NOSPROG is the default.],
    [#cmd("-not=")_chars_, #cmd("-or=")_chars_], [One to seven characters
      that stand for the NOT and the OR symbol, as the NOT and OR options of
      PL/I for MVS & VM.],
    [#cmd("-char48")], [The 48-character set, as PL/I (F)'s CHAR48 option.],
    [#cmd("-plist")], [The main procedure receives register 1 unchanged, as
      with PL/I (F)'s entry points IHESAPB and IHESAPD.],
    [#cmd("-interrupt")], [The Optimizing Compiler's INTERRUPT option. Not
      with #cmd("-lang=F").],
    [#cmd("-stack=")_n_], [The size of the run time's stack: 0, or 4096 to
      8388608 bytes; with K, kilobytes (up to 8192K). 0 keeps the default
      of LIBC/370.],
    [#cmd("--version")], [Print the version and the run time's interface
      level, and end.],
  )
]
