' *** Spin2/PASM Debugger
' *** Sets up debugger and then launches applicaion
'
CON	sav_addr	=	$FEC00
	dbg_addr	=	$FF400
'
'
' ********************
' *  Debugger Setup  *
' ********************
'
'
' Protected hub RAM usage ($FC000..$FFFFF)
'
' $FC000 - DEBUG data				addresses fixed in software
'
' $FEC00 - Cog N reg $010..$1F7 buffer
' $FF400 - Cog N reg $007..$1F7 debugger program and data
'
' $FFC00 - Cog 7 reg $000..$00F buffer		addresses fixed in silicon
' $FFC40 - Cog 7 reg $000..$00F debug ISR
'
' $FFC80 - Cog 6 reg $000..$00F buffer
' $FFCC0 - Cog 6 reg $000..$00F debug ISR
'
' $FFD00 - Cog 5 reg $000..$00F buffer
' $FFD40 - Cog 5 reg $000..$00F debug ISR
'
' $FFD80 - Cog 4 reg $000..$00F buffer
' $FFDC0 - Cog 4 reg $000..$00F debug ISR
'
' $FFE00 - Cog 3 reg $000..$00F buffer
' $FFE40 - Cog 3 reg $000..$00F debug ISR
'
' $FFE80 - Cog 2 reg $000..$00F buffer
' $FFEC0 - Cog 2 reg $000..$00F debug ISR
'
' $FFF00 - Cog 1 reg $000..$00F buffer
' $FFF40 - Cog 1 reg $000..$00F debug ISR
'
' $FFF80 - Cog 0 reg $000..$00F buffer
' $FFFC0 - Cog 0 reg $000..$00F debug ISR
'
'
DAT		org
'
'
' Set clock mode
'
t		hubset	_clkmode1_	'start external clock, remain in rcfast mode	(reused as t)
		waitx	##20_000_000/100'allow 10ms for external clock to stabilize
		hubset	_clkmode2_	'switch to external clock mode

		waitx	_delay_		'do any initial delay
'
'
' Allocate lock[15]
'
		rep	#1,#16		'allocate lock[0..15]
		locknew	t

		mov	t,#14		'return lock[14..0], leaves lock[15] allocated
.return		lockret	t
		djnf	t,#.return
'
'
' Write debugger ISR to all eight cogs' ISR buffers
'
		neg	t,#$40		'initial address of $FFFC0
		rep	#3,#8
		setq	#$00F-$000
		wrlong	@debug_isr/4,t
		sub	t,#$80
'
'
' Move debugger program into position
'
		loc	ptra,#@debugger
		loc	ptrb,#dbg_addr
		loc	pa,#$800
		call	#move
'
'
' Move DEBUG data to $FC000
'
		loc	ptra,#@debugger_end
		loc	ptrb,#$FC000
		rdword	t,ptra
		callpa	t,#move
'
'
' Protect upper hub RAM and enable debugging on cogs
'
		hubset	_hubset_
'
'
' Move application code to $00000
'
		loc	ptra,#@debugger_end
		add	ptra,t
		loc	ptrb,#0
		callpa	_appsize_,#move
'
'
' Launch application
'
		coginit	#0,#$00000	'relaunch cog 0 from $00000
'
'
' Move pa bytes from @ptra to @ptrb
'
move		add	pa,#3		'round bytes up to long
		shr	pa,#2

.block		decod	pb,#8		'determine number of longs in block ($100 max)
		fle	pb,pa
		sub	pa,pb		'adjust remaining-longs count

		sub	pb,#1		'get setq long count

		setq2	pb		'read block of longs
		rdlong	0,ptra++
		setq2	pb		'write block of longs
		wrlong	0,ptrb++

	_ret_	tjnz	pa,#.block	'another block?
'
'
' Data set by compiler
'
_clkmode1_	long	0		'@0A0: clkmode with rcfast
_clkmode2_	long	0		'@0A4: clkmode with external clock
_delay_		long	0		'@0A8: initial delay (default is 0)
_appsize_	long	0		'@0AC: application size in bytes
_hubset_	long	$2003_00FF	'@0B0: cog-debug-enable bits in low byte (default is $FF)
'
'
' *******************************************************
' *  Debug ISR						*
' *							*
' *	- on debug interrupt, jmp #$1F8 executes	*
' *	- registers $000..$00F copy to hub		*
' *	- registers $000..$00F load from hub		*
' *	- jmp #0 (debug_isr) executes			*
' *	- debug_isr code runs, ending with jmp #$1FD	*
' *	- registers $000..$00F load from hub		*
' *	- reti0 executes, returns to main code		*
' *							*
' *******************************************************
'
		org
debug_isr
'
'
' Get ct and status data - routine must be located within $000.$00F
'
cth		getct	cth	wc	'get cth
ctl		getct	ctl		'get ctl
brkcz		getbrk	brkcz	wcz	'get status wcz
brkc		getbrk	brkc	wc	'get status wc
brkz		getbrk	brkz	wz	'get status wz
fptr		getptr	fptr		'get fast ptr
regh		mov	regh,##sav_addr	'get regh

.wait		locktry	#15	wc	'wait for lock[15] so that we own the save buffer and the tx pin
	if_nc	jmp	#.wait

		setq	#$1F7-$010	'save registers $010..$1F7
		wrlong	$010,regh

		setq	#$1F7-$007	'load debugger into registers $007..$1F7
		rdlong	$007,##dbg_addr	'(preserve cth..regh at $000..$006)

		jmp	#debug_entry	'jump to debug_entry (jmp is already pipelined)
'
'
' ***********************************************
' *  Debugger					*
' *						*
' *	- cth..regh saved at $000..$006		*
' *	- debugger loads at $007..$1F7		*
' *						*
' ***********************************************
'
		org	$007
debugger
'
'
' Debugger exit - routine must be located within $007..$00F
'
debug_exit	mov	cth,dlyms  wz	'get exit delay, z=1 if 0
		mov	ctl,_waitxms_	'save waitx ms value

		setq	#$1F7-$010	'restore registers $010..$1F7
		rdlong	$010,regh

		lockrel	#15		'release lock[15] so that other cogs can own the save buffer and tx pin

.dly	if_nz	waitx	ctl		'if dlyms was not 0, do exit delay
	if_nz	djnz	cth,#.dly

		brk	brkcz		'enable BRK-instruction interrupt

		jmp	#$1FD		'restore $000..$00F, return from debug interrupt
'
'
' Data set by compiler
'
_txpin_		long	62		'@118: tx pin (default is 62, msb = timestamp)
_txmode_	long	0		'@11C: tx mode (2_000_000-8-N-1)
_waitxms_	long	0		'@120: waitx value for 1ms delay
'
'
' Debugger entry
'
debug_entry	sub	ctl,#62	wc	'trim 64-bit ct
regl		subx	cth,#0

ptr		cogid	regl		'calculate register $000..$00F buffer address
msb		not	regl
dig		shl	regl,#7

ptra_		mov	ptra_,ptra	'save ptra
ptrb_		mov	ptrb_,ptrb	'save ptrb

stk0		pop	stk0		'save stack
stk1		pop	stk1
stk2		pop	stk2
stk3		pop	stk3
stk4		pop	stk4
stk5		pop	stk5
stk6		pop	stk6
stk7		pop	stk7

iret		mov	iret,$1FF-0	'save debug interrupt return address

		brk	##$800		'show ina/inb (must be disabled before RETI0)

w		nop
.wait		rdpin	w, _txpin_ wc   'wait for tx not busy
	if_c	jmp	#.wait
		fltl	_txpin_		'reset and configure the tx pin
x		wrpin	#%01_11110_0,_txpin_
y		wxpin	_txmode_,_txpin_
z		drvl	_txpin_
'
'
' If COGINIT then show message
'
ma		testb	brkcz,#23  wc	'cog started or BRK-instruction interrupt?
xa	if_nc	jmp	#debug_id

mb		call	#cognout	'output "CogN  " with possible timestamp (msb = 31)

xb		callpb	#_ini,#rstrout	'output "INIT "

expa		mov	x,ptrb_		'output ptrb
expb		call	#hexout

		callpa	#" ",#txout	'output " "

		mov	x,ptra_		'output ptra
		call	#hexout

		testb	brkcz,#0   wc	'load or jump?
	if_nc	callpb	#_lod,#rstrout	'output " load"
	if_c	callpb	#_jmp,#rstrout	'output " jump"
'
'
' Debug done
'
debug_done	testb	_com,#31  wc	'if tx output has occurred, output new line
	if_c	callpa	#13,#txout
	if_c	callpa	#10,#txout

.wait		rdpin	w,_txpin_  wc	'wait for tx not busy
	if_c	jmp	#.wait

		fltl	_txpin_		'disable tx pin

		push	stk7		'restore stack
		push	stk6
		push	stk5
		push	stk4
		push	stk3
		push	stk2
		push	stk1
		push	stk0

		mov	ptra,ptra_	'restore ptra
		mov	ptrb,ptrb_	'restore ptrb

		mov	brkcz,#$10	'enable next debug interrupt

		jmp	#debug_exit	'exit debugger
'
'
' Get BRK-instruction identifier and point to DEBUG data
'
debug_id	getbyte	ptr,brkcz,#3
		shl	ptr,#1
		bith	ptr,#14 addbits 5
		rdword	ptr,ptr
		bith	ptr,#14 addbits 5
'
'
' Process DEBUG bytecodes
'
debug_byte	callpa	#z,#getdeb	'get DEBUG bytecode

		test	z,#$E0	wz	'argument command?
	if_nz	jmp	#arg_cmd
'
'
' Simple command
'
		altgb	z,#.table	'end/chr/str/asm
		getbyte	w
		jmp	w-0

.table		byte	debug_done	'end of DEBUG commands
		byte	.asm		'set asm mode
		byte	.if		'IF(cond)
		byte	.if		'IFNOT(cond)
		byte	.cogn		'output "CogN  " with possible timestamp
		byte	.chr		'output chr
		byte	.str		'output string
		byte	.dly		'DLY(ms)

.asm		ijnz	asm,#debug_byte	'set asm mode

.if		call	#getval		'IF/IFNOT(cond)
		testb	z,#0	wc
	if_nc	tjz	x,#debug_done
	if_c	tjnz	x,#debug_done
		jmp	#debug_byte

.cogn		call	#cognout	'output "CogN  " with possible timestamp
		jmp	#debug_byte

.chr		call	#getval		'output chr
		callpa	x,#txout
		jmp	#debug_byte

.str		call	#dstrout	'output string
		jmp	#debug_byte

.dly		call	#getval		'DLY(ms), get ms
		mov	dlyms,x
		jmp	#debug_done
'
'
' Argument command
'
arg_cmd		testb	z,#0	wz	'if %x0, output ", "
  if_nz		callpb	#_com,#rstrout

		testb	z,#1	wz	'if %0x, output expression_string + " = "
  if_nz		call	#dstrout
  if_nz		callpb	#_equ,#rstrout

		call	#getval		'get ptr/val argument
		mov	ptrb,x

		testb	z,#4	wz	'get len argument?
  if_z		call	#getval
  if_z		mov	y,x

		mov	pa,z		'ZSTR/LSTR command?
		and	pa,#$EC
		cmp	pa,#$24	wz
		testb	z,#4	wc	'value or array command?
  if_nz_and_nc	jmp	#value_cmd
  if_nz_and_c	jmp	#array_cmd
'
'
' ZSTR(ptr)
' LSTR(ptr,len)
'
		testb	z,#1	wc	'if %0x, output quote
  if_nc		callpa	#$22,#txout

		testb	z,#4	wc	'c=0 if ZSTR, c=1 if LSTR
  if_c		tjz	y,#.done	'if LSTR and len is zero then done
.loop		rdbyte	pa,ptrb++  wz	'get next chr
  if_c_or_nz	call	#txout		'if LSTR or chr <> 0 then output chr
  if_c		sub	y,#1	wz	'if LSTR, dec len
  if_nz		jmp	#.loop		'if LSTR and len <> 0 or ZSTR and chr <> 0 then loop
.done
		testb	z,#1	wc	'if %0x, output quote
  if_nc		callpa	#$22,#txout

		jmp	#debug_byte
'
'
' Value command
'
value_cmd	testb	z,#3	wc	'determine msb
		testb	z,#2	wz
		mov	msb,#31		'long/reg/minimum
  if_10		mov	msb,#15		'word
  if_01		mov	msb,#7		'byte

		test	z,#$C0	wz	'if fp, don't make absolute
  if_nz		testbn	z,#5	wz	'non-fp signed?
  if_z		zerox	x,msb		'if not, zero-extend from msb (no effect on fp)
  if_nz		signx	x,msb		'if so, sign-extend from msb
  if_nz		abs	x	wc	'..make absolute
  if_nz_and_c	callpa	#"-",#txout	'..if was negative then output "-"

		test	z,#$1C	wz	'minimum-sized?
  if_z		encod	msb,x

		testb	z,#7	wc	'output fp/dec/hex/bin
		testb	z,#6	wz
  if_00		mov	w,#fpout
  if_01		mov	w,#decout
  if_10		mov	w,#hexout
  if_11		mov	w,#binout
		call	w-0

		testb	z,#4	wz	'if value command then done, else continue array
  if_nz		jmp	#debug_byte	'(followed by array_ret)
'
'
' Array command
'
array_ret	djz	y,#debug_byte	'done?

		callpb	#_com,#rstrout	'output ", "

array_val	testb	z,#3	wc	'read array element and advance address
		testb	z,#2	wz
	if_11	rdlong	x,ptrb++
	if_10	rdword	x,ptrb++
	if_01	rdbyte	x,ptrb++
	if_00	mov	x,ptrb
	if_00	add	ptrb,#1
	if_00	call	#getreg

		jmp	#value_cmd	'output value


array_cmd	tjnz	y,#array_val	'if array not empty then output values

		callpb	#_nil,#rstrout	'if array empty then output "nil"
		jmp	#debug_byte
'
'
' *****************
' *  Subroutines  *
' *****************
'
'
' Get DEBUG byte - pa must hold register to receive byte
'
getdeb		altd	pa
		rdbyte	0-0,ptr
	_ret_	add	ptr,#1
'
'
' Get DEBUG value according to mode into x
'
getval		tjnz	asm,#getasm

	_ret_	rdlong	x,++ptra
'
'
' Get ASM value from DEBUG data into x
'
' %1000_00xx, %xxxx_xxxx					10-bit register, big-endian
' %00xx_xxxx, %xxxx_xxxx					14-bit constant, big-endian
' %0100_0000, %xxxx_xxxx, %xxxx_xxxx, %xxxx_xxxx, %xxxx_xxxx	32-bit constant, little-endian
'
getasm		rdword	x,ptr		'get initial word
		add	ptr,#2

		movbyts	x,#%%3201	'swap two lower bytes

		testb	x,#15	wc	'10-bit register?
	if_c	jmp	#getreg

		testb	x,#14	wc	'14-bit or 32-bit constant?
	if_c	sub	ptr,#1
	if_c	rdlong	x,ptr
	if_c	add	ptr,#4
		ret
'
'
' Get register x into x
'
' $000..$00F = from @regl
' $010..$1F7 = from @regh
' $1F8..$1F9 = ptra_, ptrb_
' $1FA..$1FF = direct
' $200..$3FF = lut
'
getreg		signx	x,#9	wc	'lut register? (signx simplifies comparisons)
	if_c	jmp	#.lut

		cmp	x,#$010	wc	'$000..$00F ?
	if_c	shl	x,#2
	if_c	add	x,regl
	if_c	jmp	#.reg

		cmp	x,#$1F8	wc	'$010..$1F7 ?
	if_c	sub	x,#$010
	if_c	shl	x,#2
	if_c	add	x,regh
	if_c	jmp	#.reg

		cmp	x,#$1FA	wc	'$1FA..$1FF ?
	if_c	sub	x,#$1F8 - ptra_	'$1F8..$1F9 ?
		alts	x
	_ret_	mov	x,0-0		'reg $1F8..$1FF

.reg	_ret_	rdlong	x,x		'reg $000..$1F7

.lut	_ret_	rdlut	x,x		'lut $200..$3FF
'
'
' Decimal output x
'
decout		mov	pb,#9*4	wz	'start with billions, z=0
		mov	dig,#2		'set initial underscore trip
		sets	doutn,#2	'underscore every 3 digits
		setd	doutc,#"_"	'set underscore character

doutdig		loc	pa,#dbg_addr+(@teni-@debugger)
		add	pa,pb
		rdlong	w,pa

		mov	pa,#0		'determine decimal digit
doutsub		cmpsub	x,w	wc
  if_c		cmp	pa,pa	wz	'on first non-zero digit, z=1
  if_c		ijnz	pa,#doutsub

doutn		incmod	dig,#2	wc	'if underscore after digit, c=1

  if_nz		cmp	pb,#0	wz	'always output lowest digit, but not leading zeros

  if_z		add	pa,#"0"		'output a digit
  if_z		call	#txout

  if_z		tjz	pb,#doutnext	'output an underscore?
doutc
  if_z_and_c	callpa	#"_",#txout

doutnext	sub	pb,#4		'next digit
  _ret_		tjns	pb,#doutdig
'
'
' Hex output x
'
hexout		callpa	#"$",#txout	'output "$"

hexout_digits	mov	dig,msb		'determine number of digits
		shr	dig,#2

.loop		altgn	dig,#x		'output a hex digit
		getnib	pa
		cmp	pa,#10	wc
	if_c	add	pa,#"0"
	if_nc	add	pa,#"A"-10
		call	#txout
		cmp	dig,#4	wz	'output an underscore before lower group of 4 digits
	if_z	callpa	#"_",#txout
	_ret_	djnf	dig,#.loop	'next digit
'
'
' Binary output x
'
binout		callpa	#"%",#txout	'output "%"

		mov	dig,msb

.loop		testb	x,dig	wz	'output a binary digit
	if_nz	callpa	#"0",#txout
	if_z	callpa	#"1",#txout
		tjz	dig,#.skip	'output an underscore before groups of 8 digits
		test	dig,#7	wz
	if_z	callpa	#"_",#txout
.skip	_ret_	djnf	dig,#.loop	'next digit
'
'
' Debug string output
'
dstrout_loop	call	#txout
dstrout		callpa	#pa,#getdeb
	_ret_	tjnz	pa,#dstrout_loop
'
'
' Output "CogN  " with possible timestamp
'
cognout		cogid	pb		'get cog number into string
		setnib	_cog,pb,#6
		callpb	#_cog,#rstrout	'output "CogN  "

		mov	msb,#31		'enable 32-bit output

	_ret_	tjs	_txpin_,#.time	'timestamp?

.time		mov	x,cth		'output cth
		call	#hexout
		callpa	#"_",#txout	'output "_"
		mov	x,ctl		'output ctl
		call	#hexout_digits
		mov	pb,#_cog+1	'output "  " (followed by rstrout)
'
'
' Register z-string output @pb
'
rstrout		shl	pb,#2		'convert register address to byte index

.loop		altgb	pb		'get byte
		getbyte	pa

	_ret_	tjnz	pa,#.out	'if byte = 0, done

.out		call	#txout		'output byte

		ijnz	pb,#.loop	'next byte, always branches
'
'
' Transmit byte in pa
'
txout		rdpin	w,_txpin_  wc	'wait until tx not busy
	if_c	jmp	#txout

		wypin	pa,_txpin_	'output byte

		bith	_com,#31	'set flag to indicate tx output has occurred

		ret		wcz	'return with preserved flags
'
'
' Defined data
'
asm		long	0		'0 = spin, 1 = asm
dlyms		long	0		'exit delay in ms

_cog		byte	"Cog0  ",0,0	'strings
_ini		byte	"INIT ",0,0,0
_lod		byte	" load",0,0,0
_jmp		byte	" jump",0,0,0
_com		byte	", ",0,0	'bit 31 of _com is used as a flag
_equ		byte	" = ",0
_nil		byte	"nil",0
_nan		byte	"NaN",0
'
'
' Floating-point output x
'
fpout		mov	ma,x			'get float
		bitl	ma,#31	wcz		'make positive, negative flag in z

		cmpr	ma,##$7F800000	wc	'if NaN, output "NaN" and exit
	if_c	callpb	#_nan,#rstrout
	if_c	ret

	if_z	callpa	#"-",#txout		'if negative, output "-"

		call	#.unpack		'unpack float
		mov	xb,xa
		mov	mb,ma	wz

	if_z	callpa	#"0",#txout		'if zero, output "0" and exit
	if_z	ret

		mov	expa,xb			'estimate base-10 exponent
		muls	expa,#77		'77/256 = 0.301 = log(2)
		sar	expa,#8

		mov	expb,#0			'if base-10 exponent < -32, bias up by 13
		cmps	expa,##-32  wc
	if_nc	jmp	#.notsmall
		mov	ma,.ten13
		call	#.unpack
		call	#.mul
		mov	xb,xa
		mov	mb,ma
		mov	expb,#13
		add	expa,expb
.notsmall
		shl	expa,#2			'determine exact base-10 exponent
.reduce		loc	pa,#dbg_addr+(@tenf-@debugger-6*4)
		add	pa,expa
		rdlong	ma,pa			'look up power-of-ten and multiply
		call	#.unpack
		call	#.mul
		subr	xa,#29			'round result
		shr	ma,xa	wc
		addx	ma,#0
		cmp	ma,##10_000_000	wc	'if over seven digits, reduce power-of-ten
	if_nc	add	expa,#4
	if_nc	jmp	#.reduce
		sar	expa,#2
		sub	expa,expb		'get final base-10 exponent

		mov	x,ma			'output 7-digit mantissa
		mov	pb,#6*4	wz		'start with millions, z=0
		mov	dig,#7			'set period to trip after first digit
		sets	doutn,#7
		setd	doutc,#"."
		call	#doutdig
		callpa	#"e",#txout		'output "e"
		abs	expa	wc		'make base-10 exponent absolute
	if_nc	callpa	#"+",#txout		'output "+" or "-"
	if_c	callpa	#"-",#txout
		mov	x,expa			'ready base-10 exponent
		cmp	x,x	wz		'z=1 to output leading 0's
		mov	pb,#1*4			'start with tens
		jmp	#doutdig		'output 2-digit base-10 exponent and exit


.unpack		mov	xa,ma			'unpack floating-point number
		shr	xa,#32-1-8  wz		'get exponent

		zerox	ma,#22			'get mantissa

	if_nz	bith	ma,#23			'if exponent <> 0 then insert leading one
	if_nz	shl	ma,#29-23		'...bit29-justify mantissa

	if_z	encod	xa,ma			'if exponent = 0 then get magnitude of mantissa
	if_z	ror	ma,xa			'...bit29-justify mantissa
	if_z	ror	ma,#32-29
	if_z	sub	xa,#22			'...adjust exponent to -22..0

	_ret_	sub	xa,#127			'unbias exponent


.mul		mov	pa,ma			'multiply floating-point numbers
		mov	ma,#0

		rep	#3,#30			'multiply mantissas
		shr	pa,#1	wc
		shr	ma,#1
	if_c	add	ma,mb

	_ret_	add	xa,xb			'add exponents


.ten13		long	1e+13
'
'
' Powers of ten accessed via rdlong
'
teni		long	            1
		long	           10
		long	          100
		long	        1_000
		long	       10_000
		long	      100_000
		long	    1_000_000
		long	   10_000_000
		long	  100_000_000
		long	1_000_000_000

	        long    	      1e+38, 1e+37, 1e+36, 1e+35, 1e+34, 1e+33, 1e+32, 1e+31
        	long	1e+30, 1e+29, 1e+28, 1e+27, 1e+26, 1e+25, 1e+24, 1e+23, 1e+22, 1e+21
        	long	1e+20, 1e+19, 1e+18, 1e+17, 1e+16, 1e+15, 1e+14, 1e+13, 1e+12, 1e+11
        	long	1e+10, 1e+09, 1e+08, 1e+07, 1e+06, 1e+05, 1e+04, 1e+03, 1e+02, 1e+01
tenf    	long	1e+00, 1e-01, 1e-02, 1e-03, 1e-04, 1e-05, 1e-06, 1e-07, 1e-08, 1e-09
        	long	1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15, 1e-16, 1e-17, 1e-18, 1e-19
        	long	1e-20, 1e-21, 1e-22, 1e-23, 1e-24, 1e-25, 1e-26, 1e-27, 1e-28, 1e-29
        	long	1e-30, 1e-31, 1e-32, 1e-33, 1e-34, 1e-35, 1e-36, 1e-37, 1e-38


debugger_end
'
'
'********************
'*  DEBUG Commands  *
'********************
'
'	00000000	end				end of DEBUG commands
'	00000001	asm				set asm mode
'	00000010	IF(cond)			abort if cond = 0
'	00000011	IFNOT(cond)			abort if cond <> 0
'	00000100	cogn				output "CogN  " with possible timestamp
'	00000101	chr				output chr
'	00000110	str				output string
'	00000111	DLY(ms)				delay for ms
'
'	00001000					debug interrupt
'
'	______00	', ' + zstr + ' = ' + data	specifiers for ZSTR..SBIN_LONG_ARRAY
'	______01	       zstr + ' = ' + data
'	______10	               ', ' + data
'	______11	                      data
'
'	001000__
'	001001__	ZSTR(ptr)			z-string
'	001010__
'	001011__	FDEC(val)			floating point
'	001100__	FDEC_REG_ARRAY(ptr,len)		floating point
'	001101__	LSTR(ptr,len)			length-string
'	001110__
'	001111__	FDEC_ARRAY(ptr,len)		floating point
'
'	010000__	UDEC(val)			unsigned decimal
'	010001__	UDEC_BYTE(val)
'	010010__	UDEC_WORD(val)
'	010011__	UDEC_LONG(val)
'	010100__	UDEC_REG_ARRAY(ptr,len)
'	010100__	UDEC_BYTE_ARRAY(ptr,len)
'	010110__	UDEC_WORD_ARRAY(ptr,len)
'	010111__	UDEC_LONG_ARRAY(ptr,len)
'
'	011000__	SDEC(val)			signed decimal
'	011001__	SDEC_BYTE(val)
'	011010__	SDEC_WORD(val)
'	011011__	SDEC_LONG(val)
'	011100__	SDEC_REG_ARRAY(ptr,len)
'	011101__	SDEC_BYTE_ARRAY(ptr,len)
'	011110__	SDEC_WORD_ARRAY(ptr,len)
'	011111__	SDEC_LONG_ARRAY(ptr,len)
'
'	100000__	UHEX(val)			unsigned hex
'	100001__	UHEX_BYTE(val)
'	100010__	UHEX_WORD(val)
'	100011__	UHEX_LONG(val)
'	100100__	UHEX_REG_ARRAY(ptr,len)
'	100101__	UHEX_BYTE_ARRAY(ptr,len)
'	100110__	UHEX_WORD_ARRAY(ptr,len)
'	100111__	UHEX_LONG_ARRAY(ptr,len)
'
'	101000__	SHEX(val)			signed hex
'	101001__	SHEX_BYTE(val)
'	101010__	SHEX_WORD(val)
'	101011__	SHEX_LONG(val)
'	101100__	SHEX_REG_ARRAY(ptr,len)
'	101101__	SHEX_BYTE_ARRAY(ptr,len)
'	101110__	SHEX_WORD_ARRAY(ptr,len)
'	101111__	SHEX_LONG_ARRAY(ptr,len)
'
'	110000__	UBIN(val)			unsigned binary
'	110001__	UBIN_BYTE(val)
'	110010__	UBIN_WORD(val)
'	110011__	UBIN_LONG(val)
'	110100__	UBIN_REG_ARRAY(ptr,len)
'	110101__	UBIN_BYTE_ARRAY(ptr,len)
'	110110__	UBIN_WORD_ARRAY(ptr,len)
'	110111__	UBIN_LONG_ARRAY(ptr,len)
'
'	111000__	SBIN(val)			signed binary
'	111001__	SBIN_BYTE(val)
'	111010__	SBIN_WORD(val)
'	111011__	SBIN_LONG(val)
'	111100__	SBIN_REG_ARRAY(ptr,len)
'	111101__	SBIN_BYTE_ARRAY(ptr,len)
'	111110__	SBIN_WORD_ARRAY(ptr,len)
'	111111__	SBIN_LONG_ARRAY(ptr,len)
'
'
' For PASM mode, each argument can be register or #immediate
'


'
' To Host:
'
'	debug_int(cog)
'
' From Host:
'
'	get_status : cth_curr, ctl_curr, brkcz, brkc, brkz, fptr, stack[7:0], pc, ina, inb
'	get_regs : $000..$3FF
'	get_hub(start, count) : bytes
'	get_rqpin : longs_64 cbits_64
'	write_regs(start, count, longs...)
'	write_hub(start, count, bytes...)
'	write_stk(stack[0..7])
'	write_pc(pc) - does SKIP #0 to clear skip pattern
'	async_bp(cog)
'	wait(clocks) - wait and send new alert
'	set_brk(brk_value) - returns to code
'
'
'
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'	FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
'
'	Execution						
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ	PC- FFFFF
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ	STK0- FFFFF
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ	STK1- FFFFF
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ	STK2- FFFFF
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ	STK3- FFFFF
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ	STK4- FFFFF
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ	STK5- FFFFF
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ	STK6- FFFFF
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ	STK7- FFFFF
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ	FPTR- FFFFF
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ	C = 0
'	000- DEADBEEF   IF_NC_OR_NZ	QROTATE $0F8, #$1FF  WCZ	Z = 1
'
'	Skip: 00000000000000000000000000000000
'	FASTRD/FASTWR
'
'	INT1 = Wait XFI
'	INT2 = None CT1
'	INT3 = Exec CT2
'
'	Events
'	QMT 0
'	
'	
'
'
'