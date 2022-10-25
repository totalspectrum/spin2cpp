{
con
  _clkfreq = 0
  clock_freq_addr = $14
  clock_mode_addr = $18
  
dat
	org 0
	' dummy init code
	nop
	cogid	pa
	coginit	pa, ##@real_init
	orgh	$10
	long	0	' reserved (crystal frequency on Taqoz)
	long	0	' clock frequency ($14)
	long	0	' clock mode	  ($18)
	long	0	' reserved for baud ($1c)

	orgh	$80	' $40-$80 reserved
}	
''
'' Nu code interpreter
'' This is the skeleton from which the actual interpreter is built
'' (opcodes are assigned at compile time so as to minimize the size)
''
'' NOTE: the compiler expects line feeds (only), not CR+LF. If you are
'' rebuilding on Windows, take care to use an xxd and sed which will
'' do CR+LF -> LF conversion, and/or to use an editor that will not
'' insert any extra CRs into this file.
''
'' special characters:
''    ^L = end of initial interpreter
''    ^A0 = clock frequency
''    ^A1 = clock mode
''    ^A2 = initial PC
''    ^A3 = initial object pointer
''    ^A4 = initial frame pointer
''    ^A5 = initial stack pointer (probably not valid any more)
''    ^A6 = heap size in longs
''    ^A7 = variable size in longs

	org	0
real_init
	' check for coginit startup
	cmp	ptra, #0 wz
  if_nz	jmp	#spininit
  
  	' first time run
	' set clock if frequency is not known
	rdlong	pa, #clock_freq_addr wz
  if_nz	jmp	#skip_clock
  	mov	pa, ##1	' clock mode
	mov	tmp, pa
	andn	tmp, #3
	hubset	#0
	hubset	tmp
	waitx	##200000
	hubset	pa
	wrlong	pa, #clock_mode_addr
	mov	pa, ##0	' clock frequency
	wrlong	pa, #clock_freq_addr
skip_clock
	' zero out variables and heap
	loc	ptrb, #3
	loc	pb, #(7 + 6)
	rep	@.endclr, pb
	wrlong	#0, ptrb++
.endclr
	' set up initial registers
	loc	pb, #2	' pb serves as PC, set it to entry pc
	loc	ptra, #3 + 4*(7+6) + 8   ' initial stack pointer, plus pop space
	mov	vbase, ##3	' set initial vbase from interpreter parameters
	mov	dbase, ##4	' set initial dbase from interpreter parameters
	jmp	#continue_startup
spininit
	' for Spin startup, stack should contain args, pc, vbase in that order
	rdlong	vbase, --ptra
	rdlong	pb, --ptra		' pb serves as PC
	mov	dbase, #0
continue_startup
#ifdef SERIAL_DEBUG
       mov	ser_debug_arg1, ##230_400
       call	#ser_debug_init
       mov	ser_debug_arg1, ##@init_msg
       call	#ser_debug_str
#endif
	' set up tos and nos
	rdlong	 tos, --ptra
	rdlong	 nos, --ptra

	' load LUT code
	' user LUT code (various impl_XXX)
	loc	pa, #@IMPL_LUT
	setq2	#$ff  ' fill the rest of LUT
	rdlong	$100, pa
	
	' load COG code
	loc	pa, #@start_cog
	setq	#(end_cog - start_cog)
	rdlong	start_cog, pa
	
	' copy jump table to final location at start of LUT
	loc    pa, #@OPC_TABLE
	setq2   #((OPC_TABLE_END-OPC_TABLE)/4)-1
	rdlong 0, pa
	
	' more initialization code
	mov	old_pc, #0
	mov	cogsp, #0
#ifdef SERIAL_DEBUG
       mov	dbg_flag, #0
#endif
	' cogstack_inc will act in alts/altd to increment the D field
	mov	cogstack_inc, ##(cogstack) | (1<<9)
	' cogstack_dec will act in alts/altd to decrement D, and act like it's  predecrement
	mov	cogstack_dec, ##((cogstack-1) | ($fffffe00))
	' similar for looping over 0 addresses
	mov	zero_inc, ##(1<<9)

	jmp	#start_cog
	fit	$100
	
	org	$100
start_cog
	
	' interpreter loop
#ifdef SERIAL_DEBUG
restart_loop
	rdfast	#0, pb
main_loop
	cmp	dbg_flag, #0 wz
  if_z	jmp	#.skipdbg
  	call	#dump_regs
	rdfast	#0, pb
.skipdbg
	rfbyte	pa
	getptr	pb
	cmp	pa, #$ff wz
  if_z	xor	dbg_flag, #1
  if_z	jmp	#restart_loop
  	rdlut	tmp, pa

	push	#restart_loop
	execf	tmp
#else
restart_loop
	rdfast	#0, pb
	push	#$1ff		' start xbyte loop
  _ret_	setq	#0		' use table at start of LUT
  	jmp	#restart_loop
#endif

	' hook for jumping into HUB
trampoline
	skipf	#0
	push	#restart_loop	' return to restart (hubexec uses the streamer)
	rdlut	tmp, pa		' retrieve original word
	shr	tmp, #10
	jmp	tmp		' jump to HUB address
	
impl_DIRECT
	rfword	tmp
	getptr	pb
	jmp	tmp

impl_PUSHI
	call	#\impl_DUP
#ifdef SERIAL_DEBUG	
       rflong	tos
 _ret_ getptr	pb
#else
  _ret_	rflong	tos
#endif
  
impl_PUSHI16
	call	#\impl_DUP
#ifdef SERIAL_DEBUG	
       rfword	tos
 _ret_ getptr	pb
#else
  _ret_	rfword	tos
#endif

impl_PUSHI8
	call	#\impl_DUP
#ifdef SERIAL_DEBUG	
       rfbyte	tos
 _ret_ getptr	pb
#else
  _ret_	rfbyte	tos
#endif

impl_PUSHA
	call	#\impl_DUP
	rfword	tos
	rfbyte	tmp
	shl	tmp, #16
#ifdef SERIAL_DEBUG
	or	tos, tmp
  _ret_ getptr	pb
#else  
  _ret_	or	tos, tmp
#endif

impl_GETHEAP
	call	#\impl_DUP
  _ret_	mov	tos, ##3 + 4*(7)	' set heap base

impl_DUP2
	' A B -> A B A B
	altd	cogsp, cogstack_inc
	mov	0-0, nos
	altd	cogsp, cogstack_inc
  _ret_	mov	0-0, tos

impl_SWAP2
	' A B C -> C A B
	alts	cogsp, #cogstack-1
	mov	tmp, 0-0            ' tmp = A
	altd	cogsp, #cogstack-1
  	mov	0-0, tos	    ' save C on stack
	mov	tos, nos            ' tos = B
  _ret_	mov	nos, tmp	    ' nos = A

impl_DUP
	' A B -> A B B
	altd	cogsp, cogstack_inc
	mov	0-0, nos
  _ret_	mov	nos, tos

impl_OVER
	' A B -> A B A
	altd	cogsp, cogstack_inc
	mov	0-0, nos
	mov	tmp, nos
	mov	nos, tos
  _ret_	mov	tos, tmp

impl_POP
	mov	popval, tos
impl_DROP
	mov	tos, nos
impl_DOWN
	alts	cogsp, cogstack_dec
 _ret_	mov	nos, 0-0

impl_DROP2
	alts	cogsp, cogstack_dec
 	mov	tos, 0-0
	alts	cogsp, cogstack_dec
 _ret_	mov	nos, 0-0

impl_SWAP
	mov	tmp, tos
	mov	tos, nos
 _ret_	mov	nos, tmp

'
' perform relative branch
' tmp contains offset to add to pb
'
do_relbranch_drop1
	call	#\impl_DROP
	jmp	#do_relbranch
do_relbranch_drop2
	call	#\impl_DROP2
do_relbranch
  	getptr	pb
	add	pb, tmp
	jmp	#\restart_loop

'
' call/enter/ret
' "call" saves the original pc in old_pc, the original vbase in old_vbase,
' and jumps to the new code
' normally this will start with an "enter" which saves old_pc and
' old_vbase, then sets up the new stack frame
' "callm" is like "call" but also pops a new vbase (expects nos==VBASE, tos==PC)
' "ret" undoes the "enter" and then sets pc back to old_pc
'
impl_CALLA
	mov	old_vbase, vbase
	rfword	pb
	rfbyte	tmp
	getptr	old_pc
	shl	tmp, #16
	or	pb, tmp
	jmp	#restart_loop
	
impl_CALL
	mov	old_pc, pb
	mov	old_vbase, vbase
	mov	pb, tos
	push	#restart_loop
	jmp	#impl_DROP
	
impl_CALLM
	mov	old_pc, pb
	mov	old_vbase, vbase
	mov	pb, tos
	mov	vbase, nos
	push	#restart_loop
	jmp	#impl_DROP2

' "enter" has to set up a new stack frame; it takes 3 arguments:
'    tos is the number of locals (longs)
'    nos is the number of arguments (longs)
'    nnos is the number of return values (again, longs)
'
' when we enter a subroutine the arguments are sitting on the stack;
' we need to save whatever things we need and make room for return values
' and local variables
'
impl_ENTER
	mov	nlocals, tos	' number of locals
	getbyte	nargs, nos, #0
	getbyte	nrets, nos, #1

do_enter
	' find the "stack base" (where return values will go)
	mov	old_dbase, dbase

	' push some important things onto the stack
	mov	nos, old_dbase
	mov	tos, old_pc
	call	#\impl_DUP2
	mov	nos, old_vbase
	mov	tos, cogsp
	add	tos, #2		' adjust for upcoming push
	call	#\impl_DUP2

	' write out stack
	sub	cogsp, #1
	setq	cogsp
	wrlong	cogstack, ptra++
	mov	dbase, ptra
	sub	dbase, #16	' roll back over 4 reserved words

	shl	nargs, #2
	sub	dbase, nargs	' roll back over arguments
	
	' reset local stack
	mov	cogsp, #0

	' figure out total # of locals
	add	nlocals, nrets
	
	' initialize return values to 0
	djf	nrets, #.no_ret_vals
	setq	nrets
	wrlong	#0, ptra
.no_ret_vals
	shl	nlocals, #2
  _ret_	add	ptra, nlocals	' skip over locals

' RET gives number of items on stack to pop off, and number of arguments initially
impl_RET
impl_ret_body
	push	#restart_loop
	' save # return items to pop
	getbyte	 nargs, tos, #0
	getbyte	 nrets, tos, #1
	call	#\impl_DROP

	' save the return values in 0..N
	mov	tmp, nrets wz
  if_z	jmp	#.poprets_end
	sub	tmp, #1
.poprets
	call	#\impl_POP
	altd	tmp, #0
	mov	0-0, popval
	djnf	tmp, #.poprets
.poprets_end

	' restore the stack
  	mov	ptra, dbase
	shl	nargs, #2
	add	ptra, nargs
	shr	nargs, #2
	setq	#3
	rdlong	dbase, ptra
	mov	pb, new_pc
	cmp	dbase, #0 wz
	
  if_z	jmp	#impl_HALT		' if old dbase was NULL, nothing to return to

  	' roll back stack
	sub	cogsp, #4		' remove 4 items from stack
	shl	cogsp, #2		' multiply by 4
	sub	ptra, cogsp
	shr	cogsp, #2

	' drop arguments
	sub	cogsp, nargs

	' restore local stack
	sub	cogsp, #1
	setq	cogsp
	rdlong	cogstack, ptra
	add	cogsp, #1
	
	' need to get tos and nos back into registers
	call	#\impl_DROP2
	
	' push return values
	djf	nrets, #.skip_pushrets
	mov	tmp2, ##1<<9
.pushret_loop
	call	#\impl_DUP
	alts	tmp2, zero_inc
	mov	tos, 0-0
	djnf	nrets, #.pushret_loop
	
.skip_pushrets

	ret

impl_BREAK
	getptr	pb
	setd	.brkinst, tos
	call	#\impl_DROP
.brkinst brk	#0
	jmp	#restart_loop
	
impl_HALT
#ifdef SERIAL_DEBUG
	mov	ser_debug_arg1, ##@halt_msg
	call	#ser_debug_str
#endif	
	waitx	##20000000
	cogid	pa
	cogstop	pa
end_cog

cogstack
	res	28
cogstack_inc
	res	1
cogstack_dec
	res	1
zero_inc
	res	1
nlocals	res	1
nargs	res	1
nrets	res	1
nos	res    	1
tos	res    	1
popval	res	1
tmp	res    	1
tmp2	res    	1
tmpcnt	res	1
old_dbase res  	1
old_pc	res    	1
old_vbase res  	1
old_cogsp res	1
#ifdef SERIAL_DEBUG
dbg_flag res	1  ' for serial debug
#endif

#ifndef SERIAL_DEBUG
'	fit	$1d0  ' inline assembly variables start here
	fit	$1cc  ' inline assembly variables start here
	org	$1cc
inline_vars
	res	20
#endif
	fit	$1e0
	org	$1e0  ' special locals here
dbase	res    	1     ' $1e0
new_pc	res	1     ' $1e1
vbase	res    	1     ' $1e2
cogsp	res	1     ' $1e3
__abortchain	      ' $1e4
	res	1
__abortresult
	res	1     ' $1e5
__super
	res	1	' $1e6
__reserved
	res	1	' $1e7

	' reserved:
	' $1e8 = __sendptr
	' $1e9 = __recvptr
	' $1ea = __temp_reg1
	' $1eb = __temp_reg2
	' $1ec-$1f0 for debug

	org	$200
OPCODES
	res	256

'' end of main interpreter

	orgh
impl_LDB
  _ret_	rdbyte tos, tos

impl_LDW
  _ret_ rdword tos, tos

impl_LDL
  _ret_ rdlong tos, tos

impl_LDD
	call	#\impl_DUP
	rdlong	nos, nos
	add	tos, #4
  _ret_	rdlong	tos, tos

impl_STB
	wrbyte	nos, tos
  _ret_	jmp	#\impl_DROP2

impl_STW
	wrword	nos, tos
	jmp	#\impl_DROP2

impl_STL
	wrlong	nos, tos
	jmp	#\impl_DROP2

impl_STD
	mov	tmp, tos
	call	#\impl_DROP
	wrlong	nos, tmp
	add	tmp, #4
	wrlong	tos, tmp
	jmp	#\impl_DROP2

impl_LDREG
	alts	tos
  _ret_	mov	tos, 0-0

impl_STREG
	altd	tos
  	mov	0-0, nos
	jmp	#\impl_DROP2

impl_ADD_VBASE
  _ret_	add	tos, vbase

impl_ADD_DBASE
  _ret_	add	tos, dbase

impl_ADD_PC
  _ret_	add	tos, pb
  
impl_ADD_SUPER
  _ret_	add	tos, __super

impl_ADD_SP
	add	tos, #8	  ' leave room for nos and tos
  _ret_	add	tos, ptra

impl_SET_SP
	mov	ptra, tos
	jmp	#\impl_DROP

impl_ADD
	add	tos, nos
	jmp	#\impl_DOWN

impl_SUB
	subr	tos, nos
	jmp	#\impl_DOWN

impl_AND
	and	tos, nos
	jmp	#\impl_DOWN

impl_IOR
	or	tos, nos
	jmp	#\impl_DOWN

impl_XOR
	xor	tos, nos
	jmp	#\impl_DOWN

impl_MOVBYTS
	movbyts	nos, tos
	mov	tos, nos
	jmp	#\impl_DOWN

impl_SIGNX
	signx	nos, tos
	mov	tos, nos
	jmp	#\impl_DOWN

impl_ZEROX
	zerox	nos, tos
	mov	tos, nos
	jmp	#\impl_DOWN

impl_ENCODE
	' ENCODE should return 0-32, but note Spin2 returns 0-31 with
	' ENCOD(0) == 0
	encod	tos, tos wc
  if_c	add	tos, #1		' if src nonzero, add 1
	ret

impl_ENCODE2
  _ret_	encod	tos, tos


impl_SHL
	shl	nos, tos
	jmp	#\impl_DROP

impl_SHR
	shr	nos, tos
	jmp	#\impl_DROP

impl_SAR
	sar	nos, tos
	jmp	#\impl_DROP

impl_ROL
	rol	nos, tos
	jmp	#\impl_DROP

impl_ROR
	ror	nos, tos
	jmp	#\impl_DROP

impl_MINS
	fges	tos, nos
	jmp	#\impl_DOWN

impl_MAXS
	fles	tos, nos
	jmp	#\impl_DOWN

impl_MINU
	fge	tos, nos
	jmp	#\impl_DOWN

impl_MAXU
	fle	tos, nos
	jmp	#\impl_DOWN

impl_SQRT64
	qsqrt	nos, tos
	call	#\impl_DROP
  _ret_	getqx	tos

impl_MULU
	qmul	nos, tos
	getqx	nos
 _ret_	getqy	tos

impl_MULS
	qmul	nos, tos
	mov	tmp, #0
	cmps	nos, #0 wc
  if_c	add	tmp, tos
	cmps	tos, #0 wc
  if_c	add	tmp, nos
	getqx	nos
	getqy	tos
  _ret_	sub	tos, tmp

impl_DIVU
	qdiv	nos, tos
	getqx	nos
 _ret_	getqy	tos

impl_DIVS
	abs	nos, nos wc
	muxc	tmp, #3
	abs	tos, tos wcz
  if_c	xor	tmp, #2
	qdiv	nos, tos
	getqx	nos
	getqy	tos
	test	tmp, #1 wc
  if_c	neg	tos, tos
  	test	tmp, #2 wc
  _ret_	negc	nos, nos

impl_MULDIV64
	' 3 things on stack: nnos=mult1, nos=mult2, tos=divisor
	mov	tmp, tos
	call	#\impl_DROP	' now nos=mult1, tos=mult2, tmp=divisor
	qmul	nos, tos
	getqy	nos
	getqx	tos
	setq	nos
	qdiv	tos, tmp
	call	#\impl_DROP
  _ret_	getqx	tos

impl_DIV64
	' 3 things on stack: nnos=hi, nos=lo, tos=divisor
	mov	tmp, tos
	call	#\impl_DROP	' now nos=hi, tos=lo, tmp=divisor
	setq	nos
	qdiv	tos, tmp
	getqx	nos
 _ret_	getqy	tos

impl_NEG
  _ret_	neg	tos, tos

impl_NOT
  _ret_	not	tos, tos

impl_ABS
  _ret_	abs	tos, tos

impl_ONES
  _ret_	ones	tos, tos

impl_MERGEW
  _ret_	mergew	tos, tos

impl_SPLITW
  _ret_	splitw	tos, tos

impl_MERGEB
  _ret_	mergeb	tos, tos

impl_SPLITB
  _ret_	splitb	tos, tos

impl_SEUSSF
  _ret_	seussf	tos, tos

impl_SEUSSR
  _ret_	seussf	tos, tos

impl_REV
  _ret_	rev	tos

impl_QEXP
	qexp	tos
  _ret_	getqx	tos

impl_QLOG
	qlog	tos
  _ret_	getqx	tos

impl_GETCTHL
	call	#\impl_DUP2
	getct	tos wc
  _ret_ getct	nos

impl_WAITX
	waitx	tos
	jmp	#\impl_DROP

impl_WAITCNT
	addct1	tos, #0
	waitct1
	jmp	#\impl_DROP

impl_HUBSET
	hubset	tos
	jmp	#\impl_DROP

impl_COGID
	call	#\impl_DUP
  _ret_	cogid	tos

impl_COGSTOP
	cogstop	tos
  _ret_	jmp	#\impl_DROP

impl_COGINIT
	call	#\impl_POP	' popval == param
	setq	popval
	coginit	nos, tos wc
  if_c	neg	nos, #1
  _ret_	jmp	#\impl_DROP

impl_COGATN
	cogatn	tos
	jmp	#\impl_DROP

impl_POLLATN
	call	#\impl_DUP
	pollatn	wc
  _ret_	subx	tos, #0		' turns c bit into -1

impl_BYTEMOVE
	call	#\impl_POP	' nos == dst, tos == src, popval == count
	cmp	popval, #0 wz
  if_z	jmp	#\impl_DROP	' return if 0 count
	cmp	nos, tos wcz	' cmp dst, src wcz
  if_b	jmp	#.forwards
  	add	tos, popval
	cmp	nos, tos wcz
  if_be	jmp	#.backwards
  	sub	tos, popval
.forwards
	' copy forward by longs as much as possible
	mov	tmpcnt, popval
	shr	tmpcnt, #2 wz
  if_z	jmp	#.tail
  	rep	@.fwd_lp_long, tmpcnt
	rdlong	tmp2, tos
	wrlong	tmp2, nos
	add	tos, #4
	add	nos, #4
.fwd_lp_long
.tail
	and	popval, #3 wz
  if_z	jmp	#\impl_DROP2		' return if no tail
  	rep	@.fwd_lp_tail, popval
	rdbyte	tmp2, tos
	wrbyte	tmp2, nos
	add	tos, #1
	add	nos, #1
.fwd_lp_tail
	jmp	#\impl_DROP2
.backwards
	add	nos, popval
	rep	@.back_lp, popval
	sub	tos, #1
	sub	nos, #1
	rdbyte	tmp2, tos
	wrbyte	tmp2, nos
.back_lp
	jmp	#\impl_DROP2

impl_LOCKMEM
	cogid	tmp
	or	tmp, #$100
.chk
	rdlong	tmp2, tos wz
  if_z	wrlong	tmp, tos
  if_z	rdlong	tmp2, tos
  if_z	rdlong	tmp2, tos
  	cmp	tmp2, tmp wcz
  if_nz	jmp	#.chk
  	ret

impl_LOCKNEW
	call	#\impl_DUP
  _ret_	locknew	tos

impl_LOCKRET
	lockret	tos
	jmp	#\impl_DROP

impl_LOCKTRY
	locktry tos wc
	neg	tmp, #1
  _ret_	muxc	tos, tmp

' LOCKSET is the same as LOCKTRY but with opposite return value
impl_LOCKSET
	locktry	tos wc
	neg	tmp, #1
  _ret_	muxnc	tos, tmp

impl_LOCKCLR
	lockrel	tos wc
	neg	tmp, #1
  _ret_	muxc	tos, tmp

impl_DRVL
	drvl	tos
	jmp	#\impl_DROP

impl_DRVH
	drvh	tos
	jmp	#\impl_DROP

impl_DRVNOT
	drvnot	tos
	jmp	#\impl_DROP

impl_DRVRND
	drvrnd	tos
	jmp	#\impl_DROP

impl_DRVWR
	test	nos, #1 wc
	drvc	tos
	jmp	#\impl_DROP2

impl_FLTL
	fltl	tos
	jmp	#\impl_DROP

impl_DIRL
	dirl	tos
	jmp	#\impl_DROP

impl_DIRH
	dirh	tos
	jmp	#\impl_DROP

impl_PINR
	testp	tos wc
  _ret_	wrc	tos

' NOTE: parameters are reversed from instruction for wrpin, wxpin, etc.
impl_WRPIN
	wrpin	tos, nos
	jmp	#\impl_DROP2

impl_WXPIN
	wxpin	tos, nos
	jmp	#\impl_DROP2

impl_WYPIN
	wypin	tos, nos
	jmp	#\impl_DROP2

impl_RDPIN
  _ret_	rdpin	tos, tos

impl_RQPIN
  _ret_	rqpin	tos, tos

impl_AKPIN
	akpin	tos
	jmp	#\impl_DROP

impl_XORO
	xoro32	tos
  _ret_	mov	tos, tos

impl_XYPOL
	qvector	nos, tos
	getqx	nos
  _ret_	getqy	tos

impl_ROTXY
	mov	tmp, tos	' tmp = angle
	call	#\impl_DROP	' tos = y, nos = x
	setq	tos
	qrotate	nos, tmp
	getqx	nos
  _ret_	getqy	tos


impl_ADD64
	mov	tmp, tos	' bhi
	mov	tmp2, nos	' blo
	call	#\impl_DROP2
	add	nos, tmp2 wc
  _ret_	addx	tos, tmp

impl_SUB64
	mov	tmp, tos	' bhi
	mov	tmp2, nos	' blo
	call	#\impl_DROP2
	sub	nos, tmp2 wc
  _ret_	subx	tos, tmp

impl_CMP64U
	mov	tmp, tos	' bhi
	mov	tmp2, nos	' blo
	call	#\impl_DROP2
	cmp	nos, tmp2 wcz
	cmpx	tos, tmp  wcz
  if_z  mov	nos, #0
  if_nz negc	nos, #1
  	jmp	#\impl_DROP

impl_CMP64S
	mov	tmp, tos	' bhi
	mov	tmp2, nos	' blo
	call	#\impl_DROP2
	cmp	nos, tmp2 wcz
	cmpsx	tos, tmp  wcz
  if_z  mov	nos, #0
  if_nz negc	nos, #1
  	jmp	#\impl_DROP

' setjmp/longjmp
' these are called like:
'   pub setjmp(jmpbuf) : rval, wasjmp
'   pub longjmp(jmpbuf, newrval, ignore_if_not_caught)
'
impl_SETJMP
	' tos points to the jump buffer
	call	#\impl_DUP  	 ' flush nos into cogstack
	wrlong	__abortchain, nos	' save link
	add	nos, #4
	wrlong	ptra, nos		' save stack pointer
	add	nos, #4
	mov	new_pc, pb		' save PC
	setq	#4-1
	wrlong	dbase, nos	' save main interpreter state
	' save COG stack onto RAM stack
	mov	tmp, cogsp
  	djf	tmp, #.nocogstack
	setq	tmp
	wrlong	cogstack, ptra++
.nocogstack
	mov	nos, tos	' return original buffer
  _ret_	mov	tos, #0

impl_LONGJMP
	call	#\impl_POP	' popval is ignore_if_not_caught flag
	cmp	nos, #0 wz	' nos contains jmpbuf, tos contains new rval
  if_z	jmp	#.nocatch
	'
	' restore interpreter state
	'
	rdlong	__abortchain, nos
	add	nos, #4
	rdlong	ptra, nos
	add	nos, #4
	setq	#4-1
	rdlong	dbase, nos
	' restore COG stack
	mov	tmp, cogsp
  	djf	tmp, #.nocogstack
	setq	tmp
	rdlong	cogstack, ptra
.nocogstack
	mov	nos, tos	' set new return value
	mov	tos, #1		' indicate we jumped
	mov	pb, new_pc
	jmp	#\restart_loop
	
  	'
	' come to .nocatch if the jmpbuf is <null>
	'
.nocatch
	cmp	popval, #0 wz	' OK to ignore the missing jmpbuf
  if_z	jmp	#\impl_HALT	' no - halt
  	jmp	#\impl_DROP2	' yes - just throw away the args and return

' relative branches
impl_BRA
	rfword	tmp
	signx	tmp, #15
	jmp	#\do_relbranch

impl_JMPREL
	mov	tmp, tos
	add	tmp, tos
	add	tmp, tos
	jmp	#\do_relbranch

impl_BZ
	rfword	tmp
	signx	tmp, #15
	cmp	tos, #0 wcz
  if_z	jmp	#\do_relbranch_drop1
	jmp	#\impl_DROP

impl_BNZ
	rfword	tmp
	signx	tmp, #15
	cmp	tos, #0 wcz
  if_nz	jmp	#\do_relbranch_drop1
	jmp	#\impl_DROP

impl_DJNZ
	rfword	tmp
	signx	tmp, #15
	rdlong	tmp2, tos
	sub	tmp2, #1 wz
	wrlong	tmp2, tos
 if_nz	jmp	#\do_relbranch_drop1
 	jmp	#\impl_DROP

impl_CBEQ
	rfword	tmp
	signx	tmp, #15
	cmp	nos, tos wcz
  if_z	jmp	#\do_relbranch_drop2
  	jmp	#\impl_DROP2

impl_CBNE
	rfword	tmp
	signx	tmp, #15
	cmp	nos, tos wcz
  if_nz	jmp	#\do_relbranch_drop2
  	jmp	#\impl_DROP2

impl_CBLTS
	rfword	tmp
	signx	tmp, #15
	cmps	nos, tos wcz
  if_b	jmp	#\do_relbranch_drop2
  	jmp	#\impl_DROP2

impl_CBLES
	rfword	tmp
	signx	tmp, #15
	cmps	nos, tos wcz
  if_be	jmp	#\do_relbranch_drop2
  	jmp	#\impl_DROP2

impl_CBGTS
	rfword	tmp
	signx	tmp, #15
	cmps	nos, tos wcz
  if_a	jmp	#\do_relbranch_drop2
  	jmp	#\impl_DROP2

impl_CBGES
	rfword	tmp
	signx	tmp, #15
	cmps	nos, tos wcz
  if_ae	jmp	#\do_relbranch_drop2
  	jmp	#\impl_DROP2

impl_CBLTU
	rfword	tmp
	signx	tmp, #15
	cmp	nos, tos wcz
  if_b	jmp	#\do_relbranch_drop2
  	jmp	#\impl_DROP2

impl_CBLEU
	rfword	tmp
	signx	tmp, #15
	cmp	nos, tos wcz
  if_be	jmp	#\do_relbranch_drop2
  	jmp	#\impl_DROP2

impl_CBGTU
	rfword	tmp
	signx	tmp, #15
	cmp	nos, tos wcz
  if_a	jmp	#\do_relbranch_drop2
  	jmp	#\impl_DROP2

impl_CBGEU
	rfword	tmp
	signx	tmp, #15
	cmp	nos, tos wcz
  if_ae	jmp	#\do_relbranch_drop2
  	jmp	#\impl_DROP2

'
' GOSUB:
' similar to CALL + ENTER, sets up stack so RET gets us back to here
' tos is address for GOSUB
' nos is number of locals... this is awkward, strictly we'd like to
' copy these, but for now punt and assume it does not matter
'
impl_GOSUB
	pop	tmp
	push	#restart_loop
	mov	old_pc, pb
	mov	old_vbase, vbase
	mov	pb, tos
	mov	nlocals, nos
	mov	nargs, #0		' nargs
	mov	nrets, #0		' nrets
	jmp	#\do_enter

' load inline assembly and jump to it
impl_INLINEASM
	pop	tmp
	push	#restart_loop
	' load local variables
	setq	#19
	rdlong	inline_vars, dbase
	' load code to $0
	setq	tos
	rdlong	$0, nos
	' drop from stack
	call	#\impl_DROP2
	
	' call inline code
	call	#\0-0
	
	' save local variables
	setq	#19
	wrlong	inline_vars, dbase
	ret


' final tail stuff for interpreter
	orgh
#ifdef SERIAL_DEBUG
' debug code
#include "spin/ser_debug_p2.spin2"

init_msg
	byte	"Nucode interpreter", 13, 10, 0
halt_msg
	byte	"Interpreter halt", 13, 10, 0
pc_msg
	byte	" pc: ", 0
sp_msg
	byte	" sp: ", 0
tos_msg
	byte	" tos: ", 0
nos_msg
	byte	" nos: ", 0
dbase_msg
	byte	" dbase: ", 0
cogsp_msg
	byte	" cogsp: ", 0
stack_msg
	byte    "   stack:", 0
	alignl
dump_regs
	mov	ser_debug_arg1, ##pc_msg
	call	#ser_debug_str
	mov	ser_debug_arg1, pb
	call	#ser_debug_hex
	
	mov	ser_debug_arg1, ##sp_msg
	call	#ser_debug_str
	mov	ser_debug_arg1, ptra
	call	#ser_debug_hex

	mov	ser_debug_arg1, ##dbase_msg
	call	#ser_debug_str
	mov	ser_debug_arg1, dbase
	call	#ser_debug_hex

	mov	ser_debug_arg1, ##cogsp_msg
	call	#ser_debug_str
	mov	ser_debug_arg1, cogsp
	call	#ser_debug_hex

	mov	ser_debug_arg1, ##tos_msg
	call	#ser_debug_str
	mov	ser_debug_arg1, tos
	call	#ser_debug_hex

	mov	ser_debug_arg1, ##nos_msg
	call	#ser_debug_str
	mov	ser_debug_arg1, nos
	call	#ser_debug_hex

	call	#ser_debug_nl

#ifdef NEVER
	mov	ser_debug_arg1, ##stack_msg
	call	#ser_debug_str
	sub	dbase, #8
	mov	ser_debug_arg1, dbase
	call	#ser_debug_hex
	mov	ser_debug_arg1, #":"
	call	#ser_debug_tx
	
	rdlong	ser_debug_arg1, dbase
	call	#ser_debug_hex

	add	dbase, #4
	rdlong	ser_debug_arg1, dbase
	call	#ser_debug_hex
	add	dbase, #4
	rdlong	ser_debug_arg1, dbase
	call	#ser_debug_hex
	add	dbase, #4
	rdlong	ser_debug_arg1, dbase
	call	#ser_debug_hex
	add	dbase, #4
	rdlong	ser_debug_arg1, dbase
	call	#ser_debug_hex
	add	dbase, #4
	rdlong	ser_debug_arg1, dbase
	call	#ser_debug_hex
	add	dbase, #4
	rdlong	ser_debug_arg1, dbase
	call	#ser_debug_hex
	sub	dbase, #16

	call	#ser_debug_nl
#endif	
	ret

	' dump buffer pointed to by tmp2 (4 longs)
dump_buf
	mov	ser_debug_arg1, tmp2
	call	#ser_debug_hex
	mov	ser_debug_arg1, #":"
	call	#ser_debug_tx
	
	rdlong	ser_debug_arg1, tmp2
	add	tmp2, #4
	call	#ser_debug_hex
	rdlong	ser_debug_arg1, tmp2
	add	tmp2, #4
	call	#ser_debug_hex
	rdlong	ser_debug_arg1, tmp2
	add	tmp2, #4
	call	#ser_debug_hex
	rdlong	ser_debug_arg1, tmp2
	add	tmp2, #4
	call	#ser_debug_hex
	
	jmp	#ser_debug_nl

#endif ' SERIAL_DEBUG

' labels at and of code/data
	alignl
#ifdef OLDWAY	
' variable space
3	long	0[7]

__heap_base
	long	0[6]	' A6 replaced by heap size


' stack space
5	long	0	' stack

#else
3
#endif
