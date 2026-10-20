#import "bookmaster/bookmaster.typ": *

#show: book.with(
  title: "PL1C/370 PL/I Compiler for MVS 3.8j",
  subtitle: "Language Reference",
  short-title: "PL1C/370 Language Reference",
  product: "PL1C/370",
  number: "ML10-0002-0",
  date: "October 2026",
  authors: ("Mike Großmann",),
  edition: [
    #text(font: head-font, weight: "bold", size: 11pt)[Draft (October 2026)]

    This edition applies to PL1C/370 0.4.0 and to all subsequent
    releases and modifications until otherwise indicated in new editions.

    *Draft.* Most chapters are outlines; the text follows with the
    releases of the compiler.

    Comments on this book may be addressed to the issue tracker of
    the mvslovers/pl1c370 repository on GitHub.

    © Copyright Mike Großmann 2026. All rights reserved.
  ],
)

#part("index.html", title: [PL1C/370 Language Reference])[
#titlepage()
#contents()
#figures()

#heading(numbering: none)[About This Book] <about>

This book describes the language that PL1C/370 (pl1c370) compiles and the
options of the pl1c370 command. It takes IBM PL/I (F) Version 5 as the
yardstick: what PL/I (F) accepts, PL1C/370 accepts with the same meaning,
and this book writes out where the language goes beyond PL/I (F) and
where PL1C/370 behaves differently.

How to compile and run a program is the subject of the companion volume,
the _PL1C/370 Programmer's Guide_.

== Who Should Use This Book

This book is for programmers who know PL/I and want to know exactly what
PL1C/370 accepts and what it does with it.

== How This Book Is Organized

#deflist(width: 1.35in,
  [Chapter 1], [“The pl1c370 Command”: its options.],
  [Chapter 2], [“Language Levels”: PL/I (F), the Optimizing Compiler, PL/I
    for MVS & VM, and the ISO extensions.],
)

== Related Publications

#deflist(width: 1.35in,
  [ML10-0001], [_PL1C/370 Programmer's Guide_],
)

#mainmatter()
]
#set page(numbering: "1")

#part("lr-command.html", include "ref/lr-command.typ")
#part("lr-levels.html", include "ref/lr-levels.typ")

#part("index-terms.html", title: [Index])[
#heading(numbering: none)[Index]
#make-index()
]
