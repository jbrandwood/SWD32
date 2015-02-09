*******************************************************************************

SWD32
=====

  **A file compressor designed for fast decompression in embedded targets**

  Copyright John Brandwood 1992-2015.

  Distributed under the Boost Software License, Version 1.0.

  (See the accompanying file LICENSE_1_0.txt or the copy at
        http://www.boost.org/LICENSE_1_0.txt)

*******************************************************************************

About
-----

This is a simple single-file compressor designed (primarily) for use in video
games.

It is an LZSS-variant (similar to the currently-popular LZ4), with specific
trade-offs made that reduce the compression ratio compared to sophisticated
encoders (NOT LZ4!) in favor of very fast decompression.

It was designed in the early 1990's, and was initially targeted at the Sega
Genesis and the Nintendo SuperNES.

It subsequently went through various stages of evolution through the late
1990's, and was used on the 3DO, the Nintendo 64 and the Gameboy Color.

The following games are known to have shipped with data compressed by one
or other version of this compressor ...

 * SNES - Cal Ripken Jr Baseball
 * GEN  - Greatest Heavyweights of the Ring
 * 3DO  - Slam 'N' Jam 95
 * N64  - NBA Courtside
 * N64  - NBA Courtside 2
 * N64  - Excitebike 64
 * GBC  - NBA 3 on 3
 * GBC  - Disney's Beauty & the Beast

It is provided now (warts, ugliness and all) under the permissive Boost
license so that anyone with an interest can see how things were done back
then, and the crazy coding styles that sometimes "seemed-like-a-good-idea"!

The source code has been modifed from the 1998 version to remove a proprietary
header that included NDA'd information. It has be replaced with <stdint.h>.

A Visual Studio 2010 "solution" has been provided to compile the code.

In my recent informal testing, it usually gets about 10% better compression
than LZ4, with a slightly slower, but comparable, decompression speed.

Sometimes it does better than that ... sometimes it does significantly worse.

It seems to be more data-dependant than LZ4 ... but will usually get better
results on the kind on data seen in 4th and 5th generation game consoles.

The encoding method was specifically tailored to be pretty-damned-quick when
the decoder was written in assembler on the target system. It is friendly to
8-bit processors without losing significant compression on 16-bit/32-bit
targets.

*******************************************************************************

Question
--------

  I already use LZ4 or another compressor, why should I use SWD?

Answer
------

  If you are doing the sort of development where LZ4 is looking like a good
  solution for you ... you may find that you get better compression with SWD,
  without a huge increase in CPU cycles.

  Whether that trade-off is worthwhile in your application is something that
  only you can decide.

  But really, this is mainly of historical interest, particularly since the
  LZSS search tree code is written in 32-bit assembly language for Microsoft's
  MASM, making it currently build only on Windows.

  People doing retro-game coding for old 4th and 5th generation machines may
  find this to be a useful alternative to LZ4, or a starting point for writing
  their own compression codec.

  One thing to note is that this code is written to store the raw LZSS match
  lengths and offsets in an intermediate form in memory before performing the
  final encoding for the output file.

  This means that it is easy to modify for different encodings or to perform
  statistical analysis on LZSS data in order to come up with new encodings.

*******************************************************************************
