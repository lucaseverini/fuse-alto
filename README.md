## fuse-alto

FUSE driver for the Xerox Alto and Alto II file system by Luca Severini <lucaseverini@mac.com>

Fork of buggy fuse-alto by Juergen Buchmueller <pullmoll@t-online.de>

The code is based on L. Stewart's `aar.c` dated 1/18/93.

The in-memory image of the loaded disk(s) is written to a file with the same name
you specified when when mounting, but with a `~` appended.
This is to avoid modifying your original files until the outcome of fuse-alto is reasonable.

Many (most) file operations now work, including renaming, removing,
creating, truncating and reading or writing files. There are bugs, however, which probably
destroy the integrity of a heavily modified disk image. The code is in its alpha stage!

<strong>Note</strong>: Now using commas again to separate double disk filenames.

#### How to build

First you need the FUSE development headers installed.
The FUSE library must be version 26 or newer (>= 2.6.0).
Depending on your system, the package name may be <tt>fuse-dev</tt>
or <tt>fuse-devel</tt>.

Of course you need <tt>cmake</tt> (3.x or newer), a C++ compiler like <tt>g++</tt> and (GNU) <tt>make</tt>.

For cmake projects you should build outside of the source tree, like this:

<pre>mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
cd ..
</pre>
Without <tt>-DCMAKE_BUILD_TYPE=Debug</tt> the default is to build a Release version.
If you intend to install, you can specify <tt>-DCMAKE_INSTALL_PREFIX=/usr/local</tt> or
perhaps <tt>-DCMAKE_INSTALL_PREFIX=$HOME</tt> to install to your own <tt>~/bin</tt> path.

You can now run <tt>build/bin/fuse-alto</tt> or add <tt>make install</tt> or <tt>sudo make install</tt> to the lines above to make <tt>fuse-alto</tt> be installed in the search paths.

#### Examples for using fuse-alto

Running <tt>fuse-alto</tt> without parameters will print some help.

Create a mount point for your test, like
<pre>$ mkdir /tmp/alto</pre>

First try to run in the foreground, because then you don't have to <tt>umount /tmp/alto</tt> after use.
<pre>$ build/bin/fuse-alto /tmp/alto -f someimage.dsk</pre>

In another shell you should now be able to list, remove and rename files in <tt>/tmp/alto</tt>.

When done hit <tt>Ctrl+C</tt> in the first shell to abort <tt>fuse-alto</tt> and auto-unmount <tt>/tmp/alto</tt>.

If you don't want to run in foreground, run without <tt>-f</tt>.

Have fun!

Oh, here's an example output of <tt>ls -ali</tt> in a mounted pair of disk images
<pre>
total 4618
    1 drwxr-xr-x 380 jbu  jbu       0  1. Jan 1970  .
20621 drwxrwxrwt  12 root root    320 14. Aug 04:19 ..
   57 -rw-rw-rw-   0 jbu  jbu    3786  1. Sep 1981  AllocDefs.bcd
   58 -rw-rw-rw-   0 jbu  jbu    1982  1. Sep 1981  AltoDefs.bcd
  125 -rw-rw-rw-   0 jbu  jbu    1015 27. Mai 1981  AltoDefs.mesa
   59 -rw-rw-rw-   0 jbu  jbu    2158  1. Sep 1981  AltoDisplay.bcd
  202 -rw-rw-rw-   0 jbu  jbu   25785  1. Mär 1982  AltoEthernetDriver.mesa
   60 -rw-rw-rw-   0 jbu  jbu    3458  1. Sep 1981  AltoFileDefs.bcd
  315 -rw-rw-rw-   0 jbu  jbu    3826  1. Jul 1981  Animals.al
    4 -rw-rw-rw-   0 jbu  jbu    1780 13. Aug 1981  ApolloBrown.bravo
    3 -rw-rw-rw-   0 jbu  jbu    1626 13. Aug 1981  ApolloHarvard.bravo
  298 -rw-rw-rw-   0 jbu  jbu     147 23. Apr 1981  AssembleDMT.cm
   84 -rw-rw-rw-   0 jbu  jbu   16285 12. Jan 1981  BRAVO.ERROR
   85 -rw-rw-rw-   0 jbu  jbu    6193 12. Jan 1981  BRAVO.MESSAGES
  289 -rw-rw-rw-   0 jbu  jbu    6476  1. Sep 1981  BcdDefs.bcd
  290 -rw-rw-rw-   0 jbu  jbu    6674 29. Aug 1981  BcdDefs.mesa
  292 -rw-rw-rw-   0 jbu  jbu     398 23. Apr 1981  BcplRuntime.br
  293 -rw-rw-rw-   0 jbu  jbu    4668 23. Apr 1981  BcplRuntimeMc.br
  190 -rw-rw-rw-   0 jbu  jbu   23552  2. Apr 1982  Beth.ais
   55 -rw-rw-rw-   0 jbu  jbu  135680 12. Jan 1981  Binder.Image
   61 -rw-rw-rw-   0 jbu  jbu    2678  1. Sep 1981  BitBltDefs.bcd
  260 -rw-rw-rw-   0 jbu  jbu    3950  1. Sep 1981  BootDefs.bcd
  261 -rw-rw-rw-   0 jbu  jbu    2265 28. Aug 1981  BootDefs.mesa
    5 -rw-rw-rw-   0 jbu  jbu   12140 17. Aug 1981  BootDir.mesa
    6 -rw-rw-rw-   0 jbu  jbu    2229 17. Aug 1981  BootDirDefs.mesa
  283 -rw-rw-rw-   0 jbu  jbu    8192  1. Sep 1981  BootMesa.Scratch
  116 -rw-rw-rw-   0 jbu  jbu  109568  1. Sep 1981  Bootmesa.image
   93 -rw-rw-rw-   0 jbu  jbu    5702 22. Mai 1983  Bravo.State
   83 -rw-rw-rw-   0 jbu  jbu  231704 22. Mai 1983  Bravo.run
  174 -rw-rw-rw-   0 jbu  jbu    5234  1. Mär 1982  BroadcastTest.bcd
  140 -rw-rw-rw-   0 jbu  jbu    1599  1. Mär 1982  BroadcastTest.mesa
  208 -rw-rw-rw-   0 jbu  jbu    6890  1. Mär 1982  BufferDefs.bcd
  209 -rw-rw-rw-   0 jbu  jbu    5587 11. Mai 1981  BufferDefs.mesa
  299 -rw-rw-rw-   0 jbu  jbu     183 23. Apr 1981  BuildDMT.cm
   14 -rw-rw-rw-   0 jbu  jbu       7 24. Mai 1983  Com.Cm
  137 -rw-rw-rw-   0 jbu  jbu    5428  1. Mär 1982  CommUtilDefs.bcd
  155 -rw-rw-rw-   0 jbu  jbu    1724 12. Feb 1981  CommandLineDefs.bcd
  129 -rw-rw-rw-   0 jbu  jbu     817 28. Aug 1981  CommandLineDefs.mesa
  158 -rw-rw-rw-   0 jbu  jbu    4224 12. Feb 1981  CommandLineImpl.bcd
  130 -rw-rw-rw-   0 jbu  jbu    1862 28. Aug 1981  CommandLineImpl.mesa
   54 -rw-rw-rw-   0 jbu  jbu  242688 12. Jan 1981  Compiler.image
   30 -rw-rw-rw-   0 jbu  jbu    1536 17. Feb 1981  Compress.bcd
  234 -rw-rw-rw-   0 jbu  jbu    2045  2. Mär 1982  CompressImpl.mesa
  200 -rw-rw-rw-   0 jbu  jbu    6340  1. Sep 1981  ControlDefs.bcd
  287 -rw-rw-rw-   0 jbu  jbu    4182  1. Sep 1981  CoreSwapDefs.bcd
  316 -rw-rw-rw-   0 jbu  jbu  140800  1. Sep 1981  Cursor.boot
  131 -rw-rw-rw-   0 jbu  jbu     450 28. Aug 1981  Cursor.bootmesa
  322 -rw-rw-rw-   0 jbu  jbu     304 28. Aug 1981  Cursor.config
  311 -rw-rw-rw-   0 jbu  jbu     838 28. Aug 1981  CursorImpl.mesa
  308 -rw-rw-rw-   0 jbu  jbu    7434  4. Nov 1981  DMT.BS
  309 -rw-rw-rw-   0 jbu  jbu   14996  4. Nov 1981  DMT.RUN
  310 -rw-rw-rw-   0 jbu  jbu    3554  4. Nov 1981  DMT.SYMS
  119 -rw-rw-rw-   0 jbu  jbu   12331 15. Apr 1981  DMT.TTY
  249 -rw-rw-rw-   0 jbu  jbu   15546  4. Nov 1981  DMT.boot
  301 -rw-rw-rw-   0 jbu  jbu     146 23. Apr 1981  DMT.cm
  302 -rw-rw-rw-   0 jbu  jbu    3448  3. Nov 1981  DMT1Test.BR
  288 -rw-rw-rw-   0 jbu  jbu   24416  3. Nov 1981  DMT1Test.asm
  303 -rw-rw-rw-   0 jbu  jbu    1702  3. Nov 1981  DMT2Test.BR
  291 -rw-rw-rw-   0 jbu  jbu   15688  3. Nov 1981  DMT2Test.asm
  305 -rw-rw-rw-   0 jbu  jbu    1076  3. Nov 1981  DMTDisplay.BR
  295 -rw-rw-rw-   0 jbu  jbu   10795 23. Apr 1981  DMTDisplay.asm
  276 -rw-rw-rw-   0 jbu  jbu    2240  3. Nov 1981  DMTEther.BR
  337 -rw-rw-rw-   0 jbu  jbu   10821  3. Nov 1981  DMTEther.asm
  339 -rw-rw-rw-   0 jbu  jbu    4756  4. Nov 1981  DMTMain.BR
  203 -rw-rw-rw-   0 jbu  jbu   35583  4. Nov 1981  DMTMain.asm
  307 -rw-rw-rw-   0 jbu  jbu      56  3. Nov 1981  DMTMc.BR
  297 -rw-rw-rw-   0 jbu  jbu     172 23. Apr 1981  DMTMc.asm
  324 -rw-rw-rw-   0 jbu  jbu    1820 22. Mai 1981  DMTMemo.bravo
  124 -rw-rw-rw-   0 jbu  jbu    4096 22. Mai 1981  DMTMemo.press
  304 -rw-rw-rw-   0 jbu  jbu    1200  3. Nov 1981  DMTRam.BR
  294 -rw-rw-rw-   0 jbu  jbu    9317  3. Nov 1981  DMTRam.asm
  153 -rw-rw-rw-   0 jbu  jbu     276 24. Mai 1983  Debug.Log
   62 -rw-rw-rw-   0 jbu  jbu    3134  1. Sep 1981  DirectoryDefs.bcd
  169 -rw-rw-rw-   0 jbu  jbu    7526 30. Jun 1981  DiskBoot.asm
  199 -rw-rw-rw-   0 jbu  jbu    5656  1. Sep 1981  DiskDefs.bcd
    7 -r--------   0 jbu  jbu    1250 24. Mai 1983  DiskDescriptor
   63 -rw-rw-rw-   0 jbu  jbu    2704  1. Sep 1981  DisplayDefs.bcd
  206 -rw-rw-rw-   0 jbu  jbu    6282  1. Mär 1982  DriverDefs.bcd
  353 -rw-rw-rw-   0 jbu  jbu    4929  1. Mär 1982  DriverDefs.mesa
  183 -rw-rw-rw-   0 jbu  jbu    4202  1. Mär 1982  DriverTypes.bcd
  354 -rw-rw-rw-   0 jbu  jbu    3536 10. Jun 1981  DriverTypes.mesa
  171 -rw-rw-rw-   0 jbu  jbu      55 11. Mai 1981  DumpEtherBoot.cm
  159 -rw-rw-rw-   0 jbu  jbu     512 13. Mai 1981  Dumper.boot
  334 -rw-rw-rw-   0 jbu  jbu    2896 29. Mai 1981  EFTPBootSend.bcd
  331 -rw-rw-rw-   0 jbu  jbu     426 29. Mai 1981  EFTPBootSend.config
  275 -rw-rw-rw-   0 jbu  jbu    1682 28. Mai 1981  EFTPBootSendDefs.bcd
  154 -rw-rw-rw-   0 jbu  jbu     460 28. Mai 1981  EFTPBootSendDefs.mesa
  277 -rw-rw-rw-   0 jbu  jbu    4714 29. Mai 1981  EFTPBootSendImpl.bcd
  278 -rw-rw-rw-   0 jbu  jbu    1911 29. Mai 1981  EFTPBootSendImpl.mesa
  332 -rw-rw-rw-   0 jbu  jbu    4286 29. Mai 1981  EFTPBootSendTestImpl.bcd
  280 -rw-rw-rw-   0 jbu  jbu    1777 29. Mai 1981  EFTPBootSendTestImpl.mesa
  325 -rw-rw-rw-   0 jbu  jbu    2782 21. Mai 1981  EFTPDefs.bcd
  326 -rw-rw-rw-   0 jbu  jbu    1354 21. Mai 1981  EFTPDefs.mesa
  250 -rw-rw-rw-   0 jbu  jbu    8198 22. Mai 1981  EFTPRecv.bcd
  251 -rw-rw-rw-   0 jbu  jbu    6632 22. Mai 1981  EFTPRecv.mesa
  327 -rw-rw-rw-   0 jbu  jbu    9378 21. Mai 1981  EFTPSend.bcd
  227 -rw-rw-rw-   0 jbu  jbu    7660 22. Mai 1981  EFTPSend.mesa
   70 -rw-rw-rw-   0 jbu  jbu   45626 13. Mai 1981  EMPRESS.RUN
  282 -rw-rw-rw-   0 jbu  jbu     646 23. Apr 1981  ETHERBOOT.BR
  151 -rw-rw-rw-   0 jbu  jbu   15826 11. Mai 1981  EtherBoot.asm
  172 -rw-rw-rw-   0 jbu  jbu      79 11. Mai 1981  EtherBoot.cm
  160 -rw-rw-rw-   0 jbu  jbu     689 11. Mai 1981  EtherBootMain.bcpl
    9 -rw-rw-rw-   0 jbu  jbu   53174 12. Jan 1981  Executive.Run
   87 -rw-rw-rw-   0 jbu  jbu   37188 12. Jan 1981  FONTS.WIDTHS
  362 -rw-rw-rw-   0 jbu  jbu     596 28. Aug 1981  FORM.BORDERED-DISKLABEL
   66 -rw-rw-rw-   0 jbu  jbu    3120  1. Sep 1981  FSPDefs.bcd
  186 -rw-rw-rw-   0 jbu  jbu    4236 18. Mär 1982  FakeCommandLineImpl.bcd
  111 -rw-rw-rw-   0 jbu  jbu    2063 18. Mär 1982  FakeCommandLineImpl.mesa
  149 -rw-rw-rw-   0 jbu  jbu    1624 18. Mai 1983  FakeStreamsDefs.bcd
  134 -rw-rw-rw-   0 jbu  jbu     514 18. Mai 1983  FakeStreamsDefs.mesa
  219 -rw-rw-rw-   0 jbu  jbu    7444 24. Mai 1983  FakeStreamsImpl.bcd
  126 -rw-rw-rw-   0 jbu  jbu    4477 24. Mai 1983  FakeStreamsImpl.mesa
   47 -rw-rw-rw-   0 jbu  jbu    4493 24. Mai 1983  FakeStreamsImpl.mesa$
   37 -rw-rw-rw-   0 jbu  jbu     951 29. Mär 1982  Fanon.bravo
   64 -rw-rw-rw-   0 jbu  jbu    2622  1. Sep 1981  FontDefs.bcd
   38 -rw-rw-rw-   0 jbu  jbu     525 29. Mär 1982  Fortune.bravo
   65 -rw-rw-rw-   0 jbu  jbu    3232  1. Sep 1981  FrameDefs.bcd
  221 -rw-rw-rw-   0 jbu  jbu    1629 19. Mai 1983  FrameDefs.mesa
  233 -rw-rw-rw-   0 jbu  jbu    3416  1. Sep 1981  FrameOps.bcd
  338 -rw-rw-rw-   0 jbu  jbu   11264  2. Jun 1981  Front.run
   13 -rw-rw-rw-   0 jbu  jbu   91894 12. Jan 1981  Ftp.Run
   20 -rw-rw-rw-   0 jbu  jbu     422 23. Mai 1983  Ftp.log
   86 -rw-rw-rw-   0 jbu  jbu    2288 12. Jan 1981  GACHA10.AL
   96 -rw-rw-rw-   0 jbu  jbu    2612 13. Jan 1981  GACHA12.AL
   94 -rw-rw-rw-   0 jbu  jbu    2106 13. Jan 1981  GACHA8.AL
  306 -rw-rw-rw-   0 jbu  jbu    2346  3. Nov 1981  Gacha10.BR
  296 -rw-rw-rw-   0 jbu  jbu   10218 23. Apr 1981  Gacha10.asm
  375 -rw-rw-rw-   0 jbu  jbu    9728 18. Nov 1981  Graphics.bcd
  270 -rw-rw-rw-   0 jbu  jbu    9780 18. Nov 1981  GraphicsDefs.bcd
  361 -rw-rw-rw-   0 jbu  jbu    8132 18. Nov 1981  GraphicsDefs.mesa
   95 -rw-rw-rw-   0 jbu  jbu    2588 13. Jan 1981  HELVETICA10.AL
   97 -rw-rw-rw-   0 jbu  jbu    2922 13. Jan 1981  HELVETICA12.AL
   98 -rw-rw-rw-   0 jbu  jbu    3804 13. Jan 1981  HELVETICA18.AL
   99 -rw-rw-rw-   0 jbu  jbu    2398 13. Jan 1981  HELVETICA8.AL
  100 -rw-rw-rw-   0 jbu  jbu    2254 13. Jan 1981  HIPPO10.AL
   69 -rw-rw-rw-   0 jbu  jbu    3800  1. Sep 1981  IODefs.bcd
  318 -rw-rw-rw-   0 jbu  jbu    2776 29. Mai 1981  Idle.bcd
  329 -rw-rw-rw-   0 jbu  jbu     350 27. Mai 1981  Idle.config
  330 -rw-rw-rw-   0 jbu  jbu    1806 27. Mai 1981  IdleDefs.bcd
  335 -rw-rw-rw-   0 jbu  jbu     616 27. Mai 1981  IdleDefs.mesa
  210 -rw-rw-rw-   0 jbu  jbu    5262 27. Mai 1981  IdleImpl.bcd
  317 -rw-rw-rw-   0 jbu  jbu    2249 27. Mai 1981  IdleImpl.mesa
  320 -rw-rw-rw-   0 jbu  jbu    3142 27. Mai 1981  IdleTestImpl.bcd
  319 -rw-rw-rw-   0 jbu  jbu     908 27. Mai 1981  IdleTestImpl.mesa
   67 -rw-rw-rw-   0 jbu  jbu    4388  1. Sep 1981  ImageDefs.bcd
  214 -rw-rw-rw-   0 jbu  jbu    2653 19. Mai 1983  ImageDefs.mesa
   68 -rw-rw-rw-   0 jbu  jbu    3838  1. Sep 1981  InlineDefs.bcd
  207 -rw-rw-rw-   0 jbu  jbu    4752 26. Jun 1981  Introduction.bravo
  143 -rw-rw-rw-   0 jbu  jbu     996  4. Apr 1981  JobPub.bravo
  145 -rw-rw-rw-   0 jbu  jbu     948  4. Apr 1981  JobTV.bravo
  142 -rw-rw-rw-   0 jbu  jbu    1114  4. Apr 1981  JobTextpub.bravo
  165 -rw-rw-rw-   0 jbu  jbu       9 30. Jun 1981  Key1.cm
  166 -rw-rw-rw-   0 jbu  jbu       9 30. Jun 1981  Key2.cm
  167 -rw-rw-rw-   0 jbu  jbu       9 30. Jun 1981  Key3.cm
   71 -rw-rw-rw-   0 jbu  jbu    4444  1. Sep 1981  KeyDefs.bcd
  374 -rw-rw-rw-   0 jbu  jbu    7846  8. Jul 1981  KeyStreamsA.asm
  373 -rw-rw-rw-   0 jbu  jbu    3016  8. Jul 1981  KeyStreamsB.bcpl
  252 -rw-rw-rw-   0 jbu  jbu    3466  1. Mai 1981  Lawyer.bravo
  168 -rw-rw-rw-   0 jbu  jbu       3 22. Feb 1982  Line.cm
  161 -rw-rw-rw-   0 jbu  jbu   59903 28. Jan 1981  Lister.image
  170 -rw-rw-rw-   0 jbu  jbu      44 11. Mai 1981  LoadEtherBoot.cm
  256 -rw-rw-rw-   0 jbu  jbu    2538  1. Sep 1981  LongCodeOps.bcd
  102 -rw-rw-rw-   0 jbu  jbu    2940 13. Jan 1981  MATH10.AL
  235 -rw-rw-rw-   0 jbu  jbu    4608  1. Sep 1981  MMAltoRam.bcd
  128 -rw-rw-rw-   0 jbu  jbu    4096 28. Aug 1981  MMAltoRam.binary
  237 -rw-rw-rw-   0 jbu  jbu   10316  1. Sep 1981  MMDebug.bcd
   23 -rw-rw-rw-   0 jbu  jbu    9815 28. Aug 1981  MMDebug.mesa
  238 -rw-rw-rw-   0 jbu  jbu   16694  1. Sep 1981  MMDisplay.bcd
   24 -rw-rw-rw-   0 jbu  jbu   19183 28. Aug 1981  MMDisplay.mesa
  201 -rw-rw-rw-   0 jbu  jbu    1070  1. Sep 1981  MMEtherBoot.bcd
  241 -rw-rw-rw-   0 jbu  jbu    7186  1. Sep 1981  MMFSP.bcd
   25 -rw-rw-rw-   0 jbu  jbu    8523 28. Aug 1981  MMFSP.mesa
  236 -rw-rw-rw-   0 jbu  jbu    2800  1. Sep 1981  MMFont.bcd
   18 -rw-rw-rw-   0 jbu  jbu    2390  1. Sep 1981  MMInit.bcd
   26 -rw-rw-rw-   0 jbu  jbu     900 28. Aug 1981  MMInit.mesa
  242 -rw-rw-rw-   0 jbu  jbu    4236  1. Sep 1981  MMInterrupt.bcd
   27 -rw-rw-rw-   0 jbu  jbu    2618 28. Aug 1981  MMInterrupt.mesa
  254 -rw-rw-rw-   0 jbu  jbu    7084  1. Sep 1981  MMKeyboard.bcd
   28 -rw-rw-rw-   0 jbu  jbu    9298 28. Aug 1981  MMKeyboard.mesa
  264 -rw-rw-rw-   0 jbu  jbu    6348  1. Sep 1981  MMMakeBoot.bcd
  321 -rw-rw-rw-   0 jbu  jbu    5983 28. Aug 1981  MMMakeBoot.mesa
  265 -rw-rw-rw-   0 jbu  jbu    7574  1. Sep 1981  MMMemory.bcd
  342 -rw-rw-rw-   0 jbu  jbu    6819 28. Aug 1981  MMMemory.mesa
  266 -rw-rw-rw-   0 jbu  jbu   10334  1. Sep 1981  MMModules.bcd
  343 -rw-rw-rw-   0 jbu  jbu    8813 28. Aug 1981  MMModules.mesa
  365 -rw-rw-rw-   0 jbu  jbu   16896  1. Sep 1981  MMNucleus.bcd
  358 -rw-rw-rw-   0 jbu  jbu     674 28. Aug 1981  MMNucleus.config
  364 -rw-rw-rw-   0 jbu  jbu     254  1. Sep 1981  MMNucleus.errlog
  196 -rw-rw-rw-   0 jbu  jbu    1892  1. Sep 1981  MMOps.bcd
  347 -rw-rw-rw-   0 jbu  jbu     474 28. Aug 1981  MMOps.mesa
  267 -rw-rw-rw-   0 jbu  jbu    8154  1. Sep 1981  MMProcess.bcd
  349 -rw-rw-rw-   0 jbu  jbu    7513 28. Aug 1981  MMProcess.mesa
  198 -rw-rw-rw-   0 jbu  jbu    3914  1. Sep 1981  MMSDEntries.bcd
  376 -rw-rw-rw-   0 jbu  jbu    2174 28. Aug 1981  MMSDEntries.mesa
  281 -rw-rw-rw-   0 jbu  jbu    8194  1. Sep 1981  MMSignals.bcd
  377 -rw-rw-rw-   0 jbu  jbu    8141 28. Aug 1981  MMSignals.mesa
  240 -rw-rw-rw-   0 jbu  jbu   14872  1. Sep 1981  MMStart.bcd
  378 -rw-rw-rw-   0 jbu  jbu    7711 28. Aug 1981  MMStart.mesa
  313 -rw-rw-rw-   0 jbu  jbu   16360  1. Sep 1981  MMTraps.bcd
  379 -rw-rw-rw-   0 jbu  jbu   15646 28. Aug 1981  MMTraps.mesa
  132 -rw-rw-rw-   0 jbu  jbu     193 28. Aug 1981  MakeCursor.cm
  127 -rw-rw-rw-   0 jbu  jbu     435 28. Aug 1981  MakeMM.cm
  357 -rw-rw-rw-   0 jbu  jbu    2125 17. Aug 1981  MakeNX.cm
  192 -rw-rw-rw-   0 jbu  jbu      28 24. Mai 1983  MakeXMDebug.cm
  118 -rw-rw-rw-   0 jbu  jbu     145 24. Mai 1983  Mesa.Typescript
   22 -rw-rw-rw-   0 jbu  jbu   78848 24. Mai 1983  Mesa.image
  121 -rw-rw-rw-   0 jbu  jbu   12213 13. Nov 1981  MesaBoot.bravo
   12 -rw-rw-rw-   0 jbu  jbu  130560 24. Mai 1983  MesaDebugger
   88 -rw-rw-rw-   0 jbu  jbu    2288 12. Jan 1981  MesaFont.al
  230 -rw-rw-rw-   0 jbu  jbu  193083 13. Nov 1981  MiniMesa.dm
   72 -rw-rw-rw-   0 jbu  jbu    2856  1. Sep 1981  MiscDefs.bcd
  148 -rw-rw-rw-   0 jbu  jbu    5896  1. Sep 1981  Mopcodes.bcd
  115 -rw-rw-rw-   0 jbu  jbu     544  6. Mär 1982  NEC.bravo
  351 -rw-rw-rw-   0 jbu  jbu   22944 17. Aug 1981  NXControl.mesa
  312 -rw-rw-rw-   0 jbu  jbu    2671 17. Aug 1981  NXDefs.mesa
  323 -rw-rw-rw-   0 jbu  jbu   23429 17. Aug 1981  NXDisplay.mesa
  328 -rw-rw-rw-   0 jbu  jbu    7130 17. Aug 1981  NXUtil.mesa
   39 -rw-rw-rw-   0 jbu  jbu     471  2. Apr 1982  NatlSemi.bravo
  360 -rw-rw-rw-   0 jbu  jbu     672 17. Aug 1981  NetExec.bootmesa
  359 -rw-rw-rw-   0 jbu  jbu     750 17. Aug 1981  NetExec.config
  257 -rw-rw-rw-   0 jbu  jbu    2420  1. Sep 1981  NovaOps.bcd
  173 -rw-rw-rw-   0 jbu  jbu   20560  3. Nov 1981  OldDMTMain.asm
  372 -rw-rw-rw-   0 jbu  jbu    4807  8. Jul 1981  OsMain.bj
  369 -rw-rw-rw-   0 jbu  jbu   14597  8. Jul 1981  OsNotes.bravo
   73 -rw-rw-rw-   0 jbu  jbu    3056  1. Sep 1981  OsStaticDefs.bcd
  371 -rw-rw-rw-   0 jbu  jbu    1635  8. Jul 1981  OsTop.bj
  255 -rw-rw-rw-   0 jbu  jbu    2534  1. Sep 1981  PSBDefs.bcd
  350 -rw-rw-rw-   0 jbu  jbu    2864 24. Mai 1983  PageClient.bcd
  248 -rw-rw-rw-   0 jbu  jbu     322  2. Mär 1982  PageClient.config
  346 -rw-rw-rw-   0 jbu  jbu    2632  4. Mär 1982  PageClientDefs.bcd
  109 -rw-rw-rw-   0 jbu  jbu    2296  4. Mär 1982  PageClientDefs.mesa
  197 -rw-rw-rw-   0 jbu  jbu    8648  4. Mär 1982  PageClientImpl.bcd
  163 -rw-rw-rw-   0 jbu  jbu    9329  4. Mär 1982  PageClientImpl.mesa
   50 -rw-rw-rw-   0 jbu  jbu    8772 24. Mai 1983  PageClientTestImpl.bcd
   43 -rw-rw-rw-   0 jbu  jbu    7141 24. Mai 1983  PageClientTestImpl.mesa
   48 -rw-rw-rw-   0 jbu  jbu    4120 24. Mai 1983  PageClientTestImpl.mesa$
   82 -rw-rw-rw-   0 jbu  jbu   23552 22. Mai 1983  PageServer.bcd
  176 -rw-rw-rw-   0 jbu  jbu     398  2. Mär 1982  PageServer.config
   81 -rw-rw-rw-   0 jbu  jbu     990 22. Mai 1983  PageServer.errlog
  348 -rw-rw-rw-   0 jbu  jbu    2108 30. Jun 1981  PageServerDefs.bcd
  336 -rw-rw-rw-   0 jbu  jbu    3239 28. Aug 1981  PageServerDefs.mesa
  345 -rw-rw-rw-   0 jbu  jbu   16308 22. Mai 1983  PageServerImpl.bcd
   41 -rw-rw-rw-   0 jbu  jbu   18062 22. Mai 1983  PageServerImpl.mesa
  195 -rw-rw-rw-   0 jbu  jbu   48983 22. Mär 1982  Pascal.pco
  213 -rw-rw-rw-   0 jbu  jbu   42726 22. Mär 1982  Pascalnc.pco
  229 -rw-rw-rw-   0 jbu  jbu   16155 22. Mär 1982  Pasm.pco
  314 -rw-rw-rw-   0 jbu  jbu   13875 22. Mär 1982  Pasmnc.pco
  114 -rw-rw-rw-   0 jbu  jbu    1112 24. Mai 1983  Pbm.bcd
  220 -rw-rw-rw-   0 jbu  jbu     410 22. Mai 1983  Pbm.config
   11 -rw-rw-rw-   0 jbu  jbu     128 28. Aug 1981  PbmCompile.cm
  147 -rw-rw-rw-   0 jbu  jbu   12160 22. Mai 1983  PbmCspImpl.bcd
  194 -rw-rw-rw-   0 jbu  jbu   13405 22. Mai 1983  PbmCspImpl.mesa
  144 -rw-rw-rw-   0 jbu  jbu    8998 21. Mai 1983  PbmDefs.bcd
  133 -rw-rw-rw-   0 jbu  jbu   12248 21. Mai 1983  PbmDefs.mesa
  152 -rw-rw-rw-   0 jbu  jbu   13896 22. Mai 1983  PbmImpl.bcd
  212 -rw-rw-rw-   0 jbu  jbu    9213 22. Mai 1983  PbmImpl.mesa
    8 -rw-rw-rw-   0 jbu  jbu   13904 22. Mai 1983  PbmImplLoc.bcd
  356 -rw-rw-rw-   0 jbu  jbu    9219 22. Mai 1983  PbmImplLoc.mesa
  223 -rw-rw-rw-   0 jbu  jbu   13284 22. Mai 1983  PbmImplRem.bcd
  136 -rw-rw-rw-   0 jbu  jbu    9218 22. Mai 1983  PbmImplRem.mesa
  156 -rw-rw-rw-   0 jbu  jbu    6394 21. Mai 1983  PbmLoaderImpl.bcd
  175 -rw-rw-rw-   0 jbu  jbu    4502 15. Mär 1982  PbmLoaderImpl.mesa
   45 -rw-rw-rw-   0 jbu  jbu    3570 24. Mai 1983  PbmLoc.bcd
   44 -rw-rw-rw-   0 jbu  jbu     433 22. Mai 1983  PbmLoc.config
  157 -rw-rw-rw-   0 jbu  jbu   26016 24. Mai 1983  PbmOpcodesImpl.bcd
  222 -rw-rw-rw-   0 jbu  jbu   31905 24. Mai 1983  PbmOpcodesImpl.mesa
   92 -rw-rw-rw-   0 jbu  jbu   31866 24. Mai 1983  PbmOpcodesImpl.mesa$
   49 -rw-rw-rw-   0 jbu  jbu    3564 24. Mai 1983  PbmPS.bcd
   51 -rw-rw-rw-   0 jbu  jbu     423 22. Mai 1983  PbmPS.config
  225 -rw-rw-rw-   0 jbu  jbu   36864 24. Mai 1983  PbmRem.bcd
  224 -rw-rw-rw-   0 jbu  jbu     430 22. Mai 1983  PbmRem.config
  286 -rw-rw-rw-   0 jbu  jbu  143360 24. Mai 1983  PbmRem.symbols
  179 -rw-rw-rw-   0 jbu  jbu    5492 21. Mai 1983  PbmSegmentsImpl.bcd
  204 -rw-rw-rw-   0 jbu  jbu    5103 18. Mai 1983  PbmSegmentsImpl.mesa
  180 -rw-rw-rw-   0 jbu  jbu    5230 21. Mai 1983  PbmTraceImpl.bcd
  205 -rw-rw-rw-   0 jbu  jbu    2764 18. Mai 1983  PbmTraceImpl.mesa
   74 -rw-rw-rw-   0 jbu  jbu    3022  1. Sep 1981  ProcessDefs.bcd
  258 -rw-rw-rw-   0 jbu  jbu    3492  1. Sep 1981  ProcessOps.bcd
  228 -rw-rw-rw-   0 jbu  jbu     633 11. Mai 1981  Pup.cm
  178 -rw-rw-rw-   0 jbu  jbu    7450  1. Mär 1982  PupDefs.bcd
  269 -rw-rw-rw-   0 jbu  jbu    5910 22. Feb 1982  PupDefs.mesa
   17 -rw-rw-rw-   0 jbu  jbu    6610  2. Mär 1982  PupRouterCold.mesa
   32 -rw-rw-rw-   0 jbu  jbu    4140  2. Mär 1982  PupRouterCool.mesa
  181 -rw-rw-rw-   0 jbu  jbu    4692  1. Mär 1982  PupRouterDefs.bcd
  184 -rw-rw-rw-   0 jbu  jbu    3049  2. Mär 1982  PupRouterDefs.mesa
  185 -rw-rw-rw-   0 jbu  jbu    8690  2. Mär 1982  PupRouterIn.mesa
  188 -rw-rw-rw-   0 jbu  jbu    6360  2. Mär 1982  PupRouterOut.mesa
  340 -rw-rw-rw-   0 jbu  jbu    7672 24. Feb 1982  PupShow.bcd
  215 -rw-rw-rw-   0 jbu  jbu    3884 18. Mai 1983  PupStream.bcd
  216 -rw-rw-rw-   0 jbu  jbu    2578 18. Mai 1983  PupStream.mesa
  187 -rw-rw-rw-   0 jbu  jbu    7424  1. Mär 1982  PupTypes.bcd
  177 -rw-rw-rw-   0 jbu  jbu    5061 22. Feb 1982  PupTypes.mesa
  272 -rw-rw-rw-   0 jbu  jbu    8304 18. Nov 1981  PutAIS.bcd
  271 -rw-rw-rw-   0 jbu  jbu    7119 18. Nov 1981  PutAIS.mesa
  191 -rw-rw-rw-   0 jbu  jbu    4155 23. Apr 1981  Puzzle.pas
  164 -rw-rw-rw-   0 jbu  jbu    6917 22. Mär 1982  Puzzle.pls
  162 -rw-rw-rw-   0 jbu  jbu   18709 22. Mär 1982  Puzzle.prr
  239 -rw-rw-rw-   0 jbu  jbu       0 24. Mai 1983  REM.CM
  141 -rw-rw-rw-   0 jbu  jbu    6460 11. Dez 1981  RegionGrow.bcd
  123 -rw-rw-rw-   0 jbu  jbu    5152  1. Dez 1981  RegionGrow.goodmesa
  231 -rw-rw-rw-   0 jbu  jbu    5152 11. Dez 1981  RegionGrow.mesa
  300 -rw-rw-rw-   0 jbu  jbu     175 23. Apr 1981  ReleaseDMT.cm
  138 -rw-rw-rw-   0 jbu  jbu    5122  4. Apr 1981  ResumeBeth.bravo
  135 -rw-rw-rw-   0 jbu  jbu   10926 16. Feb 1982  ResumeJamie.bravo
   21 -rw-rw-rw-   0 jbu  jbu   26368 10. Dez 1981  RunMesa.Run
  259 -rw-rw-rw-   0 jbu  jbu    3216  1. Sep 1981  SDDefs.bcd
  344 -rw-rw-rw-   0 jbu  jbu    8488  2. Jun 1981  SWAT.HELP
  274 -rw-rw-rw-   0 jbu  jbu    6868 23. Apr 1981  SYS.BK
   52 -rw-rw-rw-   0 jbu  jbu   10195 12. Jan 1981  SYS.ERRORS
  117 -rw-rw-rw-   0 jbu  jbu     447 29. Mär 1982  ScavengerLog
   75 -rw-rw-rw-   0 jbu  jbu    8360  1. Sep 1981  SegmentDefs.bcd
  279 -rw-rw-rw-   0 jbu  jbu   27648  1. Sep 1981  SendPup.bcd
  211 -rw-rw-rw-   0 jbu  jbu     393 28. Aug 1981  SendPup.config
  150 -rw-rw-rw-   0 jbu  jbu   13212  1. Sep 1981  SendPupImpl.bcd
  355 -rw-rw-rw-   0 jbu  jbu   10604 28. Aug 1981  SendPupImpl.mesa
  352 -rw-rw-rw-   0 jbu  jbu     496 22. Mai 1983  Simple.pas
   36 -rw-rw-rw-   0 jbu  jbu    4026  1. Sep 1981  StartUp.bcd
  182 -rw-rw-rw-   0 jbu  jbu    7714  1. Mär 1982  StatsDefs.bcd
  217 -rw-rw-rw-   0 jbu  jbu    4336 18. Mai 1983  Stream.bcd
   76 -rw-rw-rw-   0 jbu  jbu    6044  1. Sep 1981  StreamDefs.bcd
  285 -rw-rw-rw-   0 jbu  jbu    4615 15. Mär 1982  StreamDefs.mesa
   77 -rw-rw-rw-   0 jbu  jbu    4100  1. Sep 1981  StringDefs.bcd
  253 -rw-rw-rw-   0 jbu  jbu    4430  1. Sep 1981  StringsA.bcd
  363 -rw-rw-rw-   0 jbu  jbu    6710  1. Sep 1981  StringsB.bcd
   10 -rw-rw-rw-   0 jbu  jbu  130560 12. Jan 1981  Swat
  246 -rw-rw-rw-   0 jbu  jbu     788 23. Apr 1981  SwatResident.BR
  245 -rw-rw-rw-   0 jbu  jbu   10844 23. Apr 1981  SwatResident.asm
   16 -rw-rw-rw-   0 jbu  jbu  140800 24. Mai 1983  Swatee
   19 -rw-rw-rw-   0 jbu  jbu  130560 11. Mai 1981  Sys.Boot
    2 -r--------   0 jbu  jbu   12146 24. Mai 1983  SysDir
   15 -rw-rw-rw-   0 jbu  jbu    2288  1. Jul 1981  SysFont.al
  368 -rw-rw-rw-   0 jbu  jbu    1236  8. Jul 1981  SysFont.strike
  370 -rw-rw-rw-   0 jbu  jbu    1813  8. Jul 1981  SysInternals.d
   78 -rw-rw-rw-   0 jbu  jbu    2548  1. Sep 1981  SystemDefs.bcd
   40 -rw-rw-rw-   0 jbu  jbu     432  2. Apr 1982  TI.bravo
   42 -rw-rw-rw-   0 jbu  jbu     582 20. Apr 1982  TI2.bravo
  104 -rw-rw-rw-   0 jbu  jbu    2578 13. Jan 1981  TIMESROMAN10.AL
  105 -rw-rw-rw-   0 jbu  jbu    2940 13. Jan 1981  TIMESROMAN12.AL
  106 -rw-rw-rw-   0 jbu  jbu    3184 13. Jan 1981  TIMESROMAN14.AL
  107 -rw-rw-rw-   0 jbu  jbu    2384 13. Jan 1981  TIMESROMAN8.AL
  232 -rw-rw-rw-   0 jbu  jbu    5828  3. Feb 1981  TYPE.RUN
  262 -rw-rw-rw-   0 jbu  jbu   12800 28. Aug 1981  TableCompiler.bcd
  243 -rw-rw-rw-   0 jbu  jbu     888 23. Apr 1981  TeleSwat.BR
  122 -rw-rw-rw-   0 jbu  jbu    6864 23. Apr 1981  TeleSwat.asm
  367 -rw-rw-rw-   0 jbu  jbu    1236  8. Jul 1981  Terminal.strike
   79 -rw-rw-rw-   0 jbu  jbu    2656  1. Sep 1981  TimeDefs.bcd
   33 -rw-rw-rw-   0 jbu  jbu   21504  2. Mär 1982  TinyPup.bcd
  189 -rw-rw-rw-   0 jbu  jbu   73314  2. Mär 1982  TinyPup.symbols
   46 -rw-rw-rw-   0 jbu  jbu     275 16. Mär 1982  Titles.bravo
  226 -rw-rw-rw-   0 jbu  jbu     787 18. Mär 1982  Towers.pas
   35 -rw-rw-rw-   0 jbu  jbu     723 22. Mär 1982  Towers.pco
   31 -rw-rw-rw-   0 jbu  jbu    1549 22. Mär 1982  Towers.pls
   29 -rw-rw-rw-   0 jbu  jbu    1542 22. Mär 1982  Towers.prr
   34 -rw-rw-rw-   0 jbu  jbu    6107 22. Mär 1982  Towers.psm
   80 -rw-rw-rw-   0 jbu  jbu    2400  1. Sep 1981  TrapDefs.bcd
  263 -rw-rw-rw-   0 jbu  jbu    2208  1. Sep 1981  TrapOps.bcd
  284 -rw-rw-rw-   0 jbu  jbu    3658  4. Mär 1982  TypeImpl.bcd
  108 -rw-rw-rw-   0 jbu  jbu    1034  4. Mär 1982  TypeImpl.mesa
  247 -rw-rw-rw-   0 jbu  jbu     150 23. Apr 1981  UpdateTimer.BR
  244 -rw-rw-rw-   0 jbu  jbu    1506 23. Apr 1981  UpdateTimer.asm
   53 -rw-rw-rw-   0 jbu  jbu    3770  3. Sep 1981  User.cm
  110 -rw-rw-rw-   0 jbu  jbu     551  4. Mär 1982  WestDig.bravo
  218 -rw-rw-rw-   0 jbu  jbu   14542 24. Mai 1983  XCoremap.bcd
   56 -rw-rw-rw-   0 jbu  jbu  180736 24. Mai 1983  XDebug.image
  273 -rw-rw-rw-   0 jbu  jbu    5645 19. Mai 1981  XDebug.signals
  193 -rw-rw-rw-   0 jbu  jbu    9632 24. Mai 1983  XMDebug.bcd
  112 -rw-rw-rw-   0 jbu  jbu    2658 24. Mai 1983  XMesa.signals
  113 -rw-rw-rw-   0 jbu  jbu  140860 24. Mai 1983  XMesa.symbols
   90 -rw-rw-rw-   0 jbu  jbu   54272 22. Mai 1983  bravo.scratchbin
   91 -rw-rw-rw-   0 jbu  jbu    5632 22. Mai 1983  bravo.scratchbravo
   89 -rw-rw-rw-   0 jbu  jbu    8192 22. Mai 1983  bravo.ts
  366 -rw-rw-rw-   0 jbu  jbu     512 30. Jun 1981  fontmem.b
  341 -rw-rw-rw-   0 jbu  jbu    1536  1. Feb 1982  hbo.press
  139 -rw-rw-rw-   0 jbu  jbu    3196  6. Mai 1981  inman.bravo
  101 -rw-rw-rw-   0 jbu  jbu     766 13. Jan 1981  logo24.al
  146 -rw-rw-rw-   0 jbu  jbu    3286 15. Apr 1981  runner.al
  268 -rw-rw-rw-   0 jbu  jbu    2858 12. Mai 1981  sailing.al
  120 -rw-rw-rw-   0 jbu  jbu    2462  3. Nov 1981  skier.al
  103 -rw-rw-rw-   0 jbu  jbu    3264 13. Jan 1981  symbol10.al
  333 -rw-rw-rw-   0 jbu  jbu    1220  6. Aug 1981  terminal10.strike
</pre>
# fuse-alto
