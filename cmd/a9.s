/ a9 -- pdp-11 assembler pass 1

eae = 0

/ key to types

/	0	undefined
/	1	absolute
/	2	text
/	3	data
/	4	bss
/	5	flop freg,dst (movfo, = stcfd)
/	6	branch
/	7	jsr
/	10	rts
/	11	sys
/	12	movf (=ldf,stf)
/	13	double operand (mov)
/	14	flop fsrc,freg (addf)
/	15	single operand (clr)
/	16	.byte
/	17	string (.ascii, "<")
/	20	.even
/	21	.if
/	22	.endif
/	23	.globl
/	24	register
/	25	.text
/	26	.data
/	27	.bss
/	30	mul,div, etc
/	31	sob
/	32	.comm

symtab:

/ special variables

166600; 000000; dotrel: 02; dot:000000 /.
171560; 000000; 01; dotdot:000000 /..

/ register

072270;000000;24;000000 /r0
072340;000000;24;000001 /r1
072410;000000;24;000002 /r2
072460;000000;24;000003 /r3
072530;000000;24;000004 /r4
072600;000000;24;000005 /r5
074500;000000;24;000006 /sp
062170;000000;24;000007 /pc

.if eae

/eae & switches

012717;000000;01;177570 /csw
015176;000000;01;177300 /div
003270;000000;01;177302 /ac
051750;000000;01;177304 /mq
052224;000000;01;177306 /mul
073470;000000;01;177310 /sc
074620;000000;01;177311 /sr
054752;000000;01;177312 /nor
047000;000000;01;177314 /lsh
004500;000000;01;177316 /ash

.endif

/ system calls

021411;076400;01;0000001 /exit
023752;042300;01;0000002 /fork
070511;014400;01;0000003 /read
111231;076710;01;0000004 /write
060105;053600;01;0000005 /open
012257;073610;01;0000006 /close
107761;076400;01;0000007 /wait
012625;004540;01;0000010 /creat
046166;042300;01;0000011 /link
102574;035173;01;0000012 /unlink
021405;011300;01;0000013 /exec
012004;035420;01;0000014 /chdir
077165;017500;01;0000015 /time
050563;015172;01;0000016 /makdir
012015;057140;01;0000017 /chmod
012017;110760;01;0000020 /chown
007525;003770;01;0000021 /break
074741;076400;01;0000022 /stat
073615;042300;01;0000023 /seek
076724;045400;01;0000024 /tell
051655;055240;01;0000025 /mount
102527;102604;01;0000026 /umount
073634;102254;01;0000027 /setuid
026234;102254;01;0000030 /getuid
074751;051010;01;0000031 /stime
066621;076400;01;0000032 /quit
035204;070200;01;0000033 /intr
024214;004540;01;0000034 /fstat
011625;076400;01;0000035 /cemt
050741;076710;01;0000036 /mdate
074764;116100;01;0000037 /stty
027364;116100;01;0000040 /gtty
035047;035203;01;0000041 /ilgins
054353;017500;01;0000042 /nice

/ double operand

051656;000000;13;0010000 /mov
051656;006200;13;0110000 /movb
012330;000000;13;0020000 /cmp
012330;006200;13;0120000 /cmpb
006774;000000;13;0030000 /bit
006774;006200;13;0130000 /bitb
006753;000000;13;0040000 /bic
006753;006200;13;0140000 /bicb
006773;000000;13;0050000 /bis
006773;006200;13;0150000 /bisb
003344;000000;13;0060000 /add
075012;000000;13;0160000 /sub

/ branch

007520;000000;06;0000400 /br
007265;000000;06;0001000 /bne
006531;000000;06;0001400 /beq
006635;000000;06;0002000 /bge
007164;000000;06;0002400 /blt
006654;000000;06;0003000 /bgt
007145;000000;06;0003400 /ble
007414;000000;06;0100000 /bpl
007221;000000;06;0100400 /bmi
006711;000000;06;0101000 /bhi
007157;073300;06;0101400 /blos
007763;000000;06;0102000 /bvc
010003;000000;06;0102400 /bvs
006711;073300;06;0103000 /bhis
006513;000000;06;0103000 /bec
006373;000000;06;0103000 /bcc
007157;000000;06;0103400 /blo
006413;000000;06;0103400 /bcs
006533;000000;06;0103400 /bes

/ single operand

012262;000000;15;0005000 /clr
012262;006200;15;0105000 /clrb
012445;000000;15;0005100 /com
012445;006200;15;0105100 /comb
035163;000000;15;0005200 /inc
035163;006200;15;0105200 /incb
014713;000000;15;0005300 /dec
014713;006200;15;0105300 /decb
054117;000000;15;0005400 /neg
054117;006200;15;0105400 /negb
003343;000000;15;0005500 /adc
003343;006200;15;0105500 /adcb
073423;000000;15;0005600 /sbc
073423;006200;15;0105600 /sbcb
100014;000000;15;0005700 /tst
100014;006200;15;0105700 /tstb
071352;000000;15;0006000 /ror
071352;006200;15;0106000 /rorb
071344;000000;15;0006100 /rol
071344;006200;15;0106100 /rolb
004512;000000;15;0006200 /asr
004512;006200;15;0106200 /asrb
004504;000000;15;0006300 /asl
004504;006200;15;0106300 /aslb
040230;000000;15;0000100 /jmp
075131;006200;15;0000300 /swab

/ jsr

040612;000000;07;0004000 /jsr

/ rts

071663;000000;010;000200 /rts

/ simple operand

075273;000000;011;104400 /sys

/ flag-setting

012243;000000;01;0000241 /clc
012266;000000;01;0000242 /clv
012272;000000;01;0000244 /clz
012256;000000;01;0000250 /cln
073613;000000;01;0000261 /sec
073636;000000;01;0000262 /sev
073642;000000;01;0000264 /sez
073626;000000;01;0000270 /sen

/ floating point ops

011663;011300;01;170000 / cfcc
073634;022600;01;170001 / setf
073634;014400;01;170011 / setd
073634;034100;01;170002 / seti
073634;045400;01;170012 / setl
012262;022600;15;170400 / clrf
054117;022600;15;170700 / negf
003243;022600;15;170600 / absf
100014;022600;15;170500 / tstf
051656;022600;12;172400 / movf
051656;034460;14;177000 / movif
051656;023350;05;175400 / movfi
051656;057260;14;177400 / movof
051656;023730;05;176000 / movfo
003344;022600;14;172000 / addf
075012;022600;14;173000 / subf
052224;022600;14;171000 / mulf
015176;022600;14;174400 / divf
012330;022600;14;173400 / cmpf
051634;022600;14;171400 / modf
024153;000000;24;000000 / fr0
024154;000000;24;000001 / fr1
024155;000000;24;000002 / fr2
024156;000000;24;000003 / fr3
024157;000000;24;000004 / fr4
024160;000000;24;000005 / fr5
/ 11/45 operations

004063;000000;30;072000 /als (ash)
004063;011300;30;073000 /alsc (ashc)
051731;000000;30;070000 /mpy
.if eae-1
052224;000000;30;070000 /mul (=mpy)
015176;000000;30;071000 / div (=dvd)
004500;000000;30;072000 / ash (=als)
004500;011300;30;073000 / ashc
.endif
016164;000000;30;071000 /dvd
114152;000000;07;074000 /xor
075224;000000;15;006700 /sxt
050572;042300;11;006400 /mark
074432;000000;31;077000 /sob

/ specials

166751;076710;16;000000 /.byte
167136;020560;20;000000 /.even
167356;000000;21;000000 /.if
167126;015156;22;000000 /.endif
167244;057034;23;000000 /.globl
170245;114440;25;000000 /.text
167041;076450;26;000000 /.data
166743;073300;27;000000 /.bss
167007;051510;32;000000 /.comm

usymtab:
0;0;0;0

start:
	sys	intr; aexit
	mov	sp,r5
	mov	(r5)+,r0
	cmpb	*2(r5),$'-
	bne	1f
	tst	(r5)+
	dec	r0
	br	2f
1:
	clr	unglob
2:
	movb	r0,nargs
	mov	r5,curarg
	jsr	r5,fcreat; a.tmp1
	movb	r0,pof
	jsr	r5,fcreat; a.tmp2
	movb	r0,fbfil
	jsr	pc,setup
	mov	$1,r0
	sys	write; qi; 2
	jmp	go

setup:
	mov	$symtab,r1
1:
	mov	$symbol,r0
	mov	(r1)+,(r0)+
	beq	3f
	mov	(r1)+,(r0)+
	mov	(r1)+,r2
	bic	$37,r2
	mov	r2,(r0)+
	mov	r1,-(sp)
	jsr	pc,slot
	mov	(sp)+,r1
	mov	r1,(r0)
	sub	$6,(r0)
	tst	(r1)+
	br	1b
3:
	rts	pc

slot:
	mov	symbol,r1
	add	symbol+2,r1
	add	symbol+4,r1
	clr	r0
	dvd	$hshsiz,r0
	asl	r1
	add	$hshtab,r1
1:
	cmp	r1,$hshtab
	bhi	2f
	mov	$2*hshsiz+hshtab,r1
2:
	mov	-(r1),r2
	beq	3f
	mov	$symbol,r3
	cmp	(r2)+,(r3)+
	bne	1b
	cmp	(r2)+,(r3)+
	bne	1b
	mov	(r2)+,r0
	bic	$37,r0
	cmp	r0,(r3)+
	bne	1b
3:
	mov	r1,r0
	rts	pc

end:

symbol,r0
	mov	(r1)+,(r0)+
	beq	3f
	mov	(r1)+,(r0)+
	mov	(r1)+,r2
	bic	$37,r2
	mov	r2,(r0)+
	mov	r1,-(sp)
	jsr	pc,slot
	mov	(sp)+,r1
	mov	r1,(r0)
	sub	$6,(r0)
	tst	(r1)+
	br	1b
3:
	rts	pc

slot:
	mov	symbol,r1
	add	symbol+2,r1
	add	symbol+4,r1
	clr	r0
	dvd	$hshsiz,r0
	asl	r1
	add	$hshtab,r1
1:
	cmp	r1,$hshtab
	bhi	2f
	mov	$2*hshsiz+hshtab,r1
2:
	mov	-(r1),r2
	beq	3f
	mov	$symbol,r3
	cmp	(r2)+,(r3)+
	bne	1b
	cmp	(r