#import "../bookmaster/bookmaster.typ": *

= Compiling and Linking <pg-compile>

#idx("compiling")
This chapter is in preparation. It will cover the compile step, the
%INCLUDE members and their search path (#cmd("-I")), make dependency
files (#cmd("-MD"), #cmd("-MF")), the assembly and the link with the run
time, and building with MBT.

== The Source File <pg-source>

#idx("source file")
#idx("margins")
PL1C/370 reads columns 2 to 72 of every line as one continuous stream, as
PL/I (F) does with its default margins SORMGIN=(2,72). A line shorter than
72 columns counts as padded with blanks, a tab counts as one blank, and a
string, comment or identifier may run on into the next line. Column 1 and
columns 73 and beyond are ignored, except for a #cmd("*PROCESS")
statement that begins in column 1.

#idx("character set")
The file is read one byte to a character, as Latin-1. A UTF-8 sequence for
a character from U+0080 to U+00FF counts as that one character, so the
margins count characters, not bytes: a line with #cmd("Côte") in a comment
ends in the same column in a UTF-8 file as in a Latin-1 file. The NOT sign
#cmd("¬") (U+00AC) is read as #cmd("^"). Any other byte is taken as the
Latin-1 character it is. In the generated program, characters are in
EBCDIC code page 037.
