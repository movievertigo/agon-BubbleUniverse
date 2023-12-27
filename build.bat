@echo off
set name=bubbleuniverse
cd obj
..\buildtools\ez80cc -cpu:ez80f92 -optspeed ..\agonlib.c
..\buildtools\ez80asm -cpu:ez80f92 agonlib.src
..\buildtools\ez80asm -cpu:ez80f92 ..\agonmos.asm
..\buildtools\ez80asm -cpu:ez80f92 ..\%name%.asm
..\buildtools\ez80cc -cpu:ez80f92 -optspeed ..\main.c
..\buildtools\ez80asm -cpu:ez80f92 main.src
..\buildtools\ez80link CHANGE STRSECT is CODE CHANGE DATA is CODE CHANGE BSS is CODE CHANGE TEXT is CODE LOCATE CODE AT 262144 -format=srec %name%=%name%.obj,main.obj,agonmos.obj,agonlib.obj,..\components\frameset.obj,..\components\frameset0.obj,..\components\ladd.obj,..\components\lsub.obj,..\components\lneg.obj,..\components\lcmpu.obj,..\components\lmulu.obj,..\components\ldivs.obj,..\components\ldivu.obj,..\components\lshrs.obj,..\components\lshl.obj,..\components\itol.obj,..\components\ineg.obj,..\components\imulu.obj,..\components\ishrs.obj,..\components\iand.obj,..\components\idivs.obj,..\components\idivu.obj,..\components\fpadd.obj,..\components\fpsub.obj,..\components\fpneg.obj,..\components\fpcmp.obj,..\components\fpmul.obj,..\components\fpdiv.obj,..\components\fppack.obj,..\components\fpupop1.obj,..\components\fpupop2.obj,..\components\fpftol.obj,..\components\stoi.obj,..\components\stoiu.obj
..\buildtools\srec_cat -Disable_Sequence_Warnings %name%.srec -offset -0x40000 -o ..\bin\%name%.bin -Binary
