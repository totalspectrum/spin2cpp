dat
''
'' Nu code interpreter
'' This is the skeleton from which the actual interpreter is built
'' (opcodes are assigned at compile time so as to minimize the size)
''
'' special characters:
''    ^L = end of initial interpreter
''    ^A0 = clock frequency
''    ^A1 = clock mode
''    ^A2 = initial PC
''    ^A3 = initial object pointer
''    ^A4 = initial frame pointer
''    ^A5 = initial stack pointer
''    ^A6 = heap size in longs
''    ^A7 = variable size in longs

	org 0
	' dummy init code
	nop
	cogid	pa
	coginit	pa, ##@real_init
	orgh	$10
	long	0	' reserved (crystal frequency on Taqoz)
clock_freq
	long	0	' clock frequency ($14)
clock_mode
	long	0	' clock mode	  ($18)
	long	0	' reserved for baud ($1c)
entry_pc
	long	2	' initial pc ($20)
entry_vbase
	long	3	' initial object pointer
entry_dbase
	long	4	' initial frame pointer
entry_sp
	long	5	' initial stack pointer
heap_base
	long	@__heap_base	' heap base ($30)
	orgh	$80	' $40-$80 reserved
	
	org	0
real_init
	' check for coginit startup
	cmp	ptra, #0 wz
  if_nz	jmp	#spininit
  
  	' first time run
	' set clock if frequency is not known
	rdlong	pa, #@clock_freq wz
  if_nz	jmp	#skip_clock
  	mov	pa, ##1	' clock mode
	mov	tmp, pa
	andn	tmp, #3
	hubset	#0
	hubset	tmp
	waitx	##200000
	hubset	pa
	wrlong	pa, #@clock_mode
	mov	pa, ##0	' clock frequency
	wrlong	pa, #@clock_freq
skip_clock
	' set up initial registers
	rdlong	pb, #@entry_pc	' pb serves as PC
	rdlong	vbase, #@entry_vbase
	rdlong	ptra, #@entry_sp
	rdlong	dbase, #@entry_dbase

	jmp	#continue_startup
spininit
	' for Spin startup, stack should contain args, pc, vbase in that order
	rdlong	vbase, --ptra
	rdlong	pb, --ptra		' pb serves as PC
continue_startup
#ifdef SERIAL_DEBUG
       mov	ser_debug_arg1, ##230_400
       call	#ser_debug_init
       mov	ser_debug_arg1, ##@init_msg
       call	#ser_debug_str
#endif       
	' load LUT code
	' main LUT code
	loc	pa, #@start_lut
	setq2	#(end_lut - start_lut)
	rdlong	0, pa
	' user LUT code (various impl_XXX)
	loc	pa, #@IMPL_LUT
	setq2	#$17f  ' fill the rest of LUT
	rdlong	$80, pa
	
	' load COG code
	loc	pa, #@start_cog
	setq	#(end_cog - start_cog)
	rdlong	start_cog, pa
	
	' copy jump table to final location
	loc    pa, #@OPC_TABLE
	setq   #((OPC_TABLE_END-OPC_TABLE)/4)-1
	rdlong OPCODES, pa
	
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
	altgw	pa, #OPCODES
	getword	tmp

	push	#restart_loop
	jmp	tmp
#else
restart_loop
	rdfast	#0, pb
main_loop
	rfbyte	pa
	getptr	pb
	altgw	pa, #OPCODES
	getword	tmp
	push	#main_loop
	jmp	tmp
#endif

impl_DIRECT
	rfword	tmp
	getptr	pb
	jmp	tmp

end_cog

cogstack
	res	32
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
dbase	res    	1
new_pc	res	1
vbase	res    	1
cogsp	res	1
old_dbase res  	1
old_pc	res    	1
old_vbase res  	1
old_cogsp res	1
icache	  res	1
dbg_flag res	1  ' for serial debug
OPCODES res   128
	fit	$1d0  ' inline assembly variables start here

	' reserved:
	' $1e8 = __sendptr
	' $1e9 = __recvptr
	' $1ec-$1f0 for debug
	
	org	$200
start_lut

impl_PUSHI
impl_PUSHA
	call	#\impl_DUP
	rflong	tos
  _ret_ getptr pb

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
' call/enter/ret
' "call" saves the original pc in old_pc, the original vbase in old_vbase,
' and jumps to the new code
' normally this will start with an "enter" which saves old_pc and
' old_vbase, then sets up the new stack frame
' "callm" is like "call" but also pops a new vbase (expects nos==VBASE, tos==PC)
' "ret" undoes the "enter" and then sets pc back to old_pc
'
impl_CALL
	pop	tmp
	push	#restart_loop
	mov	old_pc, pb
	mov	old_vbase, vbase
	mov	pb, tos
	jmp	#impl_DROP
	
impl_CALLM
	pop	tmp
	push	#restart_loop
	mov	old_pc, pb
	mov	old_vbase, vbase
	mov	pb, tos
	mov	vbase, nos
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
	call	#\impl_DROP	' now tos is number of args, nos is # ret values
	mov	nargs, tos
	mov	nrets, nos
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
	pop	tmp
	push	#restart_loop
	' save # return items to pop
	mov	nrets, tos
	mov	nargs, nos
	call	#\impl_DROP2

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

impl_HALT
	waitx	##20000000
	cogid	pa
	cogstop	pa

	fit	$280
end_lut
'' end of main interpreter
	' opcode table goes here
	orgh
OPC_TABLE

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
  
impl_ADD_SP
  _ret_	add	tos, ptra

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
	muxc	tmp, #1
	abs	tos, tos wc
	qdiv	nos, tos
	getqx	nos
	getqy	tos
  if_c	neg	tos, tos
  	test	tmp, #1 wc
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

impl_BRA
	pop	tmp
	push	#restart_loop
	rfword	tmp
	getptr	pb
	signx	tmp, #15
  _ret_	add	pb, tmp

impl_JMPREL
	pop	tmp
	push	#restart_loop
	add	pb, tos
	add	pb, tos
	add	pb, tos		' PC += 3*tos
	jmp	#\impl_DROP

impl_CBEQ
	pop	tmp
	push	#restart_loop
	rfword	tmp
	getptr	pb
	signx	tmp, #15
	cmp	nos, tos wcz
  if_e	add	pb, tmp
  	jmp	#\impl_DROP2

impl_CBNE
	pop	tmp
	push	#restart_loop
	rfword	tmp
	getptr	pb
	signx	tmp, #15
	cmp	nos, tos wcz
  if_ne	add	pb, tmp
  	jmp	#\impl_DROP2

impl_CBLTS
	pop	tmp
	push	#restart_loop
	rfword	tmp
	getptr	pb
	signx	tmp, #15
	cmps	nos, tos wcz
  if_b	add	pb, tmp
  	jmp	#\impl_DROP2

impl_CBLES
	pop	tmp
	push	#restart_loop
	rfword	tmp
	getptr	pb
	signx	tmp, #15
	cmps	nos, tos wcz
  if_be	add	pb, tmp
  	jmp	#\impl_DROP2

impl_CBGTS
	pop	tmp
	push	#restart_loop
	rfword	tmp
	getptr	pb
	signx	tmp, #15
	cmps	nos, tos wcz
  if_a	add	pb, tmp
  	jmp	#\impl_DROP2

impl_CBGES
	pop	tmp
	push	#restart_loop
	rfword	tmp
	getptr	pb
	signx	tmp, #15
	cmps	nos, tos wcz
  if_ae	add	pb, tmp
  	jmp	#\impl_DROP2

impl_CBLTU
	pop	tmp
	push	#restart_loop
	rfword	tmp
	getptr	pb
	signx	tmp, #15
	cmp	nos, tos wcz
  if_b	add	pb, tmp
  	jmp	#\impl_DROP2

impl_CBLEU
	pop	tmp
	push	#restart_loop
	rfword	tmp
	getptr	pb
	signx	tmp, #15
	cmp	nos, tos wcz
  if_be	add	pb, tmp
  	jmp	#\impl_DROP2

impl_CBGTU
	pop	tmp
	push	#restart_loop
	rfword	tmp
	getptr	pb
	signx	tmp, #15
	cmp	nos, tos wcz
  if_a	add	pb, tmp
  	jmp	#\impl_DROP2

impl_CBGEU
	pop	tmp
	push	#restart_loop
	rfword	tmp
	getptr	pb
	signx	tmp, #15
	cmp	nos, tos wcz
  if_ae	add	pb, tmp
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
	setq	#15
	rdlong	$1d0, dbase
	' load code to $0
	setq	tos
	rdlong	$0, nos
	' drop from stack
	call	#\impl_DROP2
	
	' call inline code
	call	#\0-0
	
	' save local variables
	setq	#15
	wrlong	$1d0, dbase
	ret


' final tail stuff for interpreter
	orgh
#ifdef SERIAL_DEBUG
' debug code
#include "spin/ser_debug_p2.spin2"

init_msg
	byte	"Nucode interpreter", 13, 10, 0
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
#endif ' SERIAL_DEBUG

' labels at and of code/data
	alignl
' variable space
3	long	0[7]

__heap_base
	long	0[6]	' A6 replaced by heap size


' stack space
5	long	0	' stack
