static const int sNumSignatures = 790;
static const char *sSignatures[] = {
"",
"",
"var value",
"var value",
"var",
"",
"",
"",
"[text] or Exit [#exc] [text] (from Python)",
"[r1] [r2] [r3] [r4] r[5] [r6]",
"name [#N] [#S] [args]",
"",
"",
"text",
"text",
"[key]",
"script_version_number",
"text",
"",
"",
"# [var]",
"var #S #E [#I]",
"expression",
"",
"",
"expression",
"",
"",
"[#]",
"",
"",
"[text]",
"label",
"#",
"text",
"# [arg1 arg2 ...]",
"name [args]",
"#",
"# [arg1 arg2 ...]",
"var [#E]",
"[#]",
"var [#E]",
"name",
"[#]",
"expression",
"var item",
"var #",
"var",
"var1 var2 [var3]",
"",
"",
"",
"var [#]",
"var",
"var #T #E",
"var [#T] [#R #E]",
"var [var] ...",
"",
"name func [args]",
"",
"[#T] [#D] [rgb]",
"",
"[#R] [#X #Y]",
"",
"[#R] [#X #Y]",
"#A [#R] [#X #Y]",
"#A [#R] [#X #Y]",
"#",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"[#]",
"#D [#P] [#C] [#S]",
"",
"",
"[#]",
"buf",
"# # [# #]",
"# #",
"",
"#N [#D] [#S]",
"",
"#T [#S] [#E] [#D]",
"#S #I #N [#D] [#O] [#W] [#F] [#X #Y]",
"var #F [#D] [#P]",
"#B [#S] [#X #Y] [#G] [#D]",
"file",
"",
"#I #X #Y",
"var",
"varX varY",
"",
"[#R] [#X #Y]",
"[#R] [#X #Y]",
"#T [#B] [#R] [#X #Y]",
"",
"",
"#",
"#",
"[#N] [#C] [#O] [#D] [#S] [#E] [#F] [#B] [#H] [#R2] [#N2] [#O2]",
"#A [#C]",
"",
"",
"buf [#N] [#T] [#P] [#S] [#X] [#Y] [#W] [#B]",
"buf #R [#S]",
"#",
"buf #C #R",
"",
"[#M] [#A]",
"#",
"# #",
"# #",
"[#]",
"[#]",
"#T [#S] [#C]",
"#T #C #A [#L] [#S]",
"[#M] [#L] [#S]",
"",
"",
"",
"",
"",
"[buf] [#F]",
"# [buf]",
"# buf file",
"#",
"buf typ cmp file",
"#S #T feat typ cmp file",
"file",
"#X #Y file",
"suffix",
"#B file",
"",
"[prompt]",
"# var",
"var file",
"var file",
"var file",
"ID mode #K file",
"ID [line]",
"ID var",
"ID var",
"ID",
"ID",
"ID [file]",
"[prompt]",
"",
"#T [#S]",
"file",
"",
"[#]",
"file",
"[#]",
"",
"[#]",
"",
"pattern",
"pattern",
"#",
"dir",
"dir",
"",
"file",
"",
"#",
"",
"key value",
"",
"key value",
"",
"",
"",
"filename",
"[#]",
"key value",
"key value",
"[#]",
"",
"[newFile]",
"[#]",
"[#O newFile]",
"[#]",
"",
"name value",
"name [var]",
"name [var]",
"name val [#K]",
"buf1 buf2",
"buf",
"#",
"#",
"#",
"#",
"[#]",
"",
"#",
"#",
"",
"#",
"#",
"#",
"",
"#M [#D]",
"#",
"#",
"#",
"#",
"#",
"#",
"#",
"#|mode",
"# units",
"[#B] [#A] [#H #M] [#V]",
"#D [#R]",
"",
"",
"",
"",
"#X #Y [#D] [#A]",
"#X #Y [#D] [#A]",
"#X #Y [#D] [#A]",
"#X #Y [#D] [#A]",
"",
"#X #Y #U",
"[#]",
"[#]",
"",
"",
"set #E",
"",
"",
"#X #Y",
"#X #Y",
"#X #Y",
"#X #Y",
"",
"",
"#X #Y [#D] [#A]",
"",
"",
"#X #Y",
"#D",
"#D #X #Y",
"",
"#",
"",
"",
"[#]",
"[#]",
"",
"#",
"",
"",
"",
"",
"#M",
"#M",
"#M",
"#M",
"#M",
"#M",
"#M",
"#M",
"#M",
"#M",
"#M",
"#M",
"",
"",
"",
"name [#M]",
"name [#M]",
"",
"#S",
"",
"#X #Y [#Z]",
"#X #Y [#Z]",
"#X #Y [#B] [#R]",
"#X #Y",
"",
"#X #Y #S",
"[#R]",
"#A [#S]",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"name",
"name",
"#L #S",
"#L #V [#R]",
"#L",
"",
"",
"#",
"#",
"",
"",
"[buf] [#]",
"",
"#X0 #X1 #Y0 #Y1",
"#X #Y #R",
"[buf]",
"[buf]",
"buf #X0 #X1 #Y0 #Y1",
"buf #X #Y",
"buf #",
"buf",
"buf",
"buf [#B]",
"buf sig1 rad1 rad2 sig2 [obuf]",
"buf1 buf2 [obuf]",
"buf1 buf2 [obuf]",
"buf1 buf2 [obuf]",
"buf1 buf2 [obuf]",
"buf #S #A [obuf] [#K]",
"buf #M #N [#F] [#B] [#P0] [#P9] [#S] [#A]",
"[buf]",
"[buf]",
"[buf]",
"[#N] [#F1] [#F2]",
"[buf] [#H]",
"#X1 #Y1 #X2 #Y2 #X3 #Y3",
"",
"[text]",
"[text]",
"[text]",
"[text]",
"text",
"#",
"[#S] [#O]",
"#V [#D]",
"#V [#D]",
"",
"name",
"",
"",
"[text]",
"text",
"[text]",
"[text]",
"text",
"text",
"#V #D text",
"var text",
"header choice1 [choice2] [choice3] buttons",
"var text",
"var text",
"var1 var2",
"text",
"text",
"text",
"[#N]",
"[#B] [#R]",
"#",
"[#W] [#V]",
"",
"#",
"#",
"[#]",
"[#R] [unit] [#M] [#I] [#G] [type] [#C] [#E] [#B]",
"[#]",
"",
"",
"[#C] [#M]",
"[#M] [#P]",
"",
"",
"#",
"#",
"#",
"#",
"#",
"#",
"#",
"#",
"",
"#",
"#",
"[#]",
"#",
"#",
"",
"#",
"",
"",
"name",
"",
"#",
"#",
"#",
"[#]",
"[#I] [#T] [#R] [#F]",
"#",
"set #E [#D]",
"set #B",
"set area",
"set #B #X #Y",
"#M [#K]",
"set #",
"set #P",
"set #F",
"set #F",
"set #M ",
"set #D [#S] [#A] [#M] [#C] ",
"#",
"#",
"",
"",
"[#]",
"[#S] [#A]",
"#",
"set #D [#D] [#D] ...",
"[set]",
"[set]",
"",
"#T [#P] [#M] [#N] [#R] [#O]",
"",
"#B [#K] [#S] [#A] [#R] [#G]",
"#G [#T] [#H] [#R1] [#R2] [#R3]",
"",
"dir",
"[#]",
"",
"#O [#P]",
"",
"#",
"",
"[#C] [#M]",
"",
"#",
"",
"#",
"",
"#",
"[#]",
"#R [#C] [#S #N #Z] [#P]",
"#I #R [#C] [#S #N #Z] [#P]",
"#I #R",
"",
"",
"#",
"[buf]",
"# [buf]",
"",
"[#]",
"[#I] [#A]",
"text",
"text",
"#",
"#",
"#I #V [text]",
"#I #V",
"#I [#S]",
"[#]",
"",
"[#M] [#S] [#E]",
"",
"#",
"[name]",
"",
"name",
"name",
"",
"[name]",
"#I #R",
"#I #C",
"#I text",
"#I [text]",
"#I #D",
"#I #S #E [#B]",
"[#T]",
"[#I] [#T]",
"[#]",
"[#]",
"",
"suffix [suffix] [suffix] [...]",
"#",
"[#]",
"[#]",
"",
"[#I] [buf]",
"[#N text]",
"",
"",
"#",
"#X #Y [#R] [#M]",
"#C #R [#L]",
"",
"#I",
"[buf]",
"[#A] [#L] [#U] [#S] [#B] [#E] [#H]",
"",
"#S [#O] [#X #Y]",
"",
"",
"#S [#OX #OY #FX #FY #N #B #M]",
"#",
"",
"#D [#P] [#N]",
"area",
"#",
"#",
"area #D [#R]",
"area",
"",
"area",
"area [#K]",
"[area]",
"[#]",
"title",
"#B [#T] [text]",
"#",
"op # [op #...]",
"#P #H",
"command",
"arg [arg] [...]",
"command",
"#",
"#N|name",
"",
"",
"",
"",
"#U #Z",
"",
"",
"#X #Y [#R]",
"#Z [#R]",
"var value [ #P]",
"[arg1 arg2 ...]",
"",
"text",
"",
"#",
"#X #Y",
"",
"#X #Y",
"",
"#I [#D]",
"#",
"[#]",
"",
"[#S] [#R] [#M] [#L] [label]",
"[#]",
"buf var",
"",
"",
"#X #Y",
"set",
"set",
"set",
"set",
"set",
"#",
"#K #F #D name",
"buf #S [#T] [#D]",
"",
"#F title",
"#",
"",
"name",
"#",
"",
"var [#T]",
"[text]",
"[#W] [#M]",
"",
"[#]",
"#",
"#S name",
"buf [#L #H]",
"buf1 buf2 buf3 [#V]",
"buf #S",
"buf #Y",
"[#R] [#S] [#D]",
"file",
"",
"buf",
"#",
"name",
"",
"# [addr]",
"",
"",
"#",
"[buf]",
"[buf]",
"buf #X #Y [#Z] [#I] [#D]",
"buf xvar yvar [#Z] [#D]",
"#SX #SY [#IX #IY] [#C] [#M] [#T]",
"#X #Y #Z [#I] [#D]",
"xvar yvar #Z [#D]",
"",
"var #F token/#I",
"#",
"buf #M #N [#AP] [#P] [#O] [#T] [#C] [#S] [#E]",
"command",
"",
"#S #E #N [#C]",
"xvar yvar [#M] [#T]",
"var",
"name",
"",
"",
"buf1 buf2 [obuf] [#C]",
"#T var1 [var2 ...]",
"#T",
"text",
"text",
"# [# ...]",
"# [# ...]",
"# [# ...]",
"opt [#] [opt] [#]...",
"#P #C #W",
"",
"",
"var",
"buf",
"#I xvar yvar [zvar]",
"#I buf xvar yvar",
"file",
"#O file",
"buf [#R] [#H] [#T] [#S] [#L] [#P]",
"[#]",
"",
"",
"#X1 #Y1 #X2 #Y2 [#M] [#T] [#X3] [#Y3]",
"[#D] [#R] [#F]",
"#D [#C]",
"name",
"#B #C name",
"#",
"#",
"#I [#C]",
"#D #S #H [#M] [fvar] [tvar]",
"#",
"#I #S",
"[#]",
"#",
"#P #LX #UX #LY #UY",
"#P [#N]",
"#L text",
"#L",
"#L #O",
"[#L] [#U] [#M] [#S] [#I] [#B]",
"#N #W [#O...]",
"",
"#I #O file",
"[#]",
"#X #Y #Z [buf]",
"buf key",
"buf key",
"",
"#I #G",
"",
"#C name [var]",
"# name value",
"from area",
"",
"[#]",
"#S #E #D [#E] [xvar yvar]",
"buf #I",
"buf #I",
"buf #I #X #Y",
"buf #I #X #Y",
"#",
"#R [#X #Y]",
"var",
"var",
"#",
"#",
"buf [file]",
"dir",
"name #",
"#",
"# var",
"#",
"",
"",
"",
"[#I] [#T]",
"#X #Y [#S]",
"[#X #Y]",
"#",
"",
"",
"#",
"dir",
"#K [#X] [#Y]",
"#T file",
"[#]",
"var #D text",
"name [#]",
"",
"#V [#A #B #M]",
"#",
"buf #M [obuf] [#CX] {#CY] [#XS] [#YS] [#B] [#C]",
"buf1 buf2 [obuf] [#CX] [#CY] [#XS] [#YS]",
"buf #CX #CY #M [#S] [#A] [#I]",
"#N [#C] [#O] [#D] [#S] [#E] [#F] [#B] [#H] [#R2] [#N2] [#O2] [#X #Y] [#DH]",
"#",
"#I xvar yvar",
"name",
"",
"#F [#P]",
"#S [#I] [#G #B]",
"#S [#M] [#S] [#A]",
"#",
"",
"#",
"#I [#D]",
"#",
"",
"",
"name #X #Y #I",
"kind [file]",
"opt val [opt val ...]",
"#I #E [#Z] [#R] [#T]",
"#K #S #B",
"",
"name func [,#D #D #D #D #D #D #D]",
"name func text",
"name func #D text",
"#X #Y",
"",
"#C",
"buf #P #M #R #S",
"#E #X #Y #B",
"[#D] [#T] [#S] [#I]",
"",
"keys",
"[buf]",
"#S #E #C [#A] [#R] [#H] [#T] [#S] [#L]",
"",
"area",
"area #O",
"[#]",
"buf #M #S [#B] [#O] [title]",
"",
"buf #Z [#X #Y]",
"#",
"#",
"#O #K [#T] [#D] [#U] [#M] [#L] [#H] [TC] [HC] [MT] [MM]",
"buf #D [#C] [#S]",
"#M #D [#S]",
"",
"",
"#",
"",
"var",
"[#L] [#M] [#S] [#P]",
"#I set [#M] [#X] [#Y]",
"",
"#",
"",
"#",
"",
"name",
"name",
"[#]",
"buf #I #N #X",
"",
"",
"#"
};
static const char *sKeyLetters = {
"A to autosave log on debug output (save file must already be defined)\r\n"
"B for beam blanking\r\n"
"C for output about how focus calibration is derived, or multi-hole combining (add * for flood of details on latter)\r\n"
"D for &nbsp;Direct Electron camera, add * to get properties list on startup\r\n"
"E for Thermo/FEI camera and other debug output from FEIScopePlugin, add * for UTAPI services\r\n"
"F for Refine ZLP\r\n"
"G for verbose GIF and magnification lookups (during JEOL updates)\r\n"
"H for Hitachi\r\n"
"I (upper case i) for intensity changes in state changes/autocentering\r\n"
"J for complete listing of JEOL calls\r\n"
"K for socket interface\r\n"
"L for more verbose low dose\r\n"
"M for Moran montaging time stamps\r\n"
"N for multi-grid routines\r\n"
"P reserved for general plugins\r\n"
"R for camera retraction\r\n"
"S for stage and montaging information\r\n"
"T for Tietz setup/reset time, camera coords, autoalign time\r\n"
"V for vacuum gauge status output\r\n"
"W for Winkler LD axis position bug/autoloader/preventing DE retraction on exit/more output from TestNextMultiShot\r\n"
"X for EMSIS and DECTRIS cameras\r\n"
"Z for general camera debug mode<br>"
"a for alignment correlation coefficients and peak erasing details; GPU frame alignment choices/memory usage\r\n"
"b for beam shift\r\n"
"c for for output during some calibration routines and output about how some calibrations are derived\r\n"
"d for reason why no electron dose\r\n"
"e for JEOL event reports\r\n"
"f for STEM autofocus\r\n"
"g for GIF and STEM state setting/tracking\r\n"
"h for navHelper Realign to Item details including choice of map to use\r\n"
"i for image shift-related items\r\n"
"j for JEOL camera\r\n"
"k for keystrokes\r\n"
"l (lower case L) for low dose\r\n"
"m to output all SEMMessageBox messages to log\r\n"
"n for Navigator map transform and other items\r\n"
"p for AlignWithScaling and FindBeamCenter output\r\n"
"q for multiple grid operations\r\n"
"r for gain and dark references\r\n"
"s for STEM in general\r\n"
"t for exposure time changes in tasks and for continuous mode timing\r\n"
"u for update items when polling JEOL, or time of scope update for all scopes\r\n"
"w for output on stage ready status\r\n"
"y for reports when background saving starts and ends\r\n"
"[ for output about running a Python script\r\n"
"* for more verbose output, available for some situations\r\n"
"% list script commands other than Set that allow arithmetic when program starts\r\n"
};
