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
''
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
	long	2	' initial pc
entry_vbase
	long	3	' initial object pointer
entry_dbase
	long	4	' initial frame pointer
entry_sp
	long	5	' initial stack pointer
	
	long	0[$10]	' more reserved words just in case
	
	org	0
real_init
	' check for coginit startup
	cmp	ptra, #0 wz
  if_nz	jmp	#spininit
  
  	' first time run
	' set clock if frequency is not known
	rdlong	pb, #@clock_freq wz
  if_nz	jmp	#skip_clock
  	mov	pb, ##1	' clock mode
	mov	tmp, pb
	andn	tmp, #3
	hubset	#0
	hubset	tmp
	waitx	##200000
	hubset	pb
	wrlong	pb, #@clock_mode
	mov	pb, ##0	' clock frequency
	wrlong	pb, #@clock_freq
skip_clock
	' set up initial registers
	rdlong	ptrb, #@entry_pc	' ptrb serves as PC
	rdlong	vbase, #@entry_vbase
	rdlong	ptra, #@entry_sp
	rdlong	dbase, #@entry_dbase

	jmp	#continue_startup
spininit
	' for Spin startup, stack should contain args, pc, vbase in that order
	rdlong	vbase, --ptra
	rdlong	ptrb, --ptra		' ptrb serves as PC
continue_startup
#ifdef SERIAL_DEBUG
       mov	ser_debug_arg1, ##230_400
       call	#ser_debug_init
       mov	ser_debug_arg1, ##@init_msg
       call	#ser_debug_str
#endif       
	' load LUT code
	loc    pb, #@start_lut
	setq2  #(end_lut - start_lut)
	rdlong 0, pb
	jmp    #start_lut

	org    $1e0
tos	res    1
nos	res    1
tmp	res    1
tmp2	res    1
vbase	res    1
dbase	res    1
old_pc	res    1
old_vbase res  1
old_dbase res  1

	fit	$1ec	' leave room for 4 debug registers
	org	$200
start_lut
	' initialization code
	mov	old_pc, #0
	' copy jump table to COG RAM
	loc    pa, #@OPC_TABLE
	setq   #(OPC_TABLE_END-OPC_TABLE)-1
	rdlong OPC_TABLE, pa
	
	' interpreter loop
main_loop
#ifdef SERIAL_DEBUG
	call	#dump_regs
#endif	
	rdbyte	pa, ptrb++
	altgw	pa, #OPC_TABLE
	getword	tmp
	call	tmp
	jmp	#main_loop

impl_DUP
	wrlong	nos, ptra++
 _ret_	mov	nos, tos

impl_DROP
	mov	tos, nos
 _ret_	rdlong	nos, --ptra

impl_DROP2
	rdlong	tos, --ptra
 _ret_	rdlong	nos, --ptra

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
' "callm" is like "call" but also pops a new vbase
' "ret" undoes the "enter" and then sets pc back to old_pc
'
impl_CALL
	mov	old_pc, ptrb
	mov	old_vbase, vbase
	mov	ptrb, tos
	jmp	#impl_DROP
	
impl_CALLM
	mov	old_pc, ptrb
	mov	old_vbase, vbase
	mov	ptrb, tos
	mov	vbase, nos
	jmp	#impl_DROP2

' "enter" has to set up a new stack frame
' when we enter a subroutine the arguments are sitting on the stack;
' we need to copy them down to make room for the return values (Spin2
' requires things to be at fixed addresses :( ) and also allocate space
' for local variables
' "enter" therefore takes three arguments:
'    tos is the number of locals (longs)
'    nos is the number of arguments (longs)
'    nnos is the number of return values (again, longs)
'
impl_ENTER
	mov	tmp, tos	' number of locals
	call	#impl_DROP	' now tos is number of args, nos is # ret values
	' find the "stack base" (where return values will go)
	mov	old_dbase, dbase
	shl	tos, #2		' multiply by 4
	sub	ptra, tos	' roll back stack by number of arguments
	shr	tos, #2
	' copy the arguments to local memory
	setq	tos
	rdlong	0-0, ptra
	' save old things onto stack
	wrlong	old_dbase, ptra++
	wrlong	old_pc, ptra++    ' don't really push these, they go in registers
	wrlong	old_vbase, ptra++
	mov	dbase, ptra	' set up dbase
	shl	nos, #2		' # of bytes in ret values
	add	ptra, nos	' skip over return values
	setq	tos
	wrlong	0-0, ptra	' write out the arguments
	shl	tos, #2
	add	ptra, tos	' skip over arguments
	shl	tmp, #2
  _ret_	add	ptra, tmp	' skip over locals



' RET gives number of items on stack to pop off
impl_RET
	djf	tos, #no_ret_values	' subtract 1 from tos, and if -1 then go to void case
	' make sure nos in memory (tos is # items to pop, so not needed)
	wrlong	nos, ptra
	shl	tos, #2		' # of bytes
	sub	ptra, tos	' go back on stack
	shr	tos, #2		' back to # longs
	' save the return values
	setq	tos
	rdlong	0-0, ptra
	' restore the stack
	mov	ptra, dbase
	rdlong	vbase, --ptra
	rdlong	ptrb, --ptra
	rdlong	dbase, --ptra wz
  if_z	jmp	#impl_HALT		' if old dbase was NULL, nothing to return to
  	setq	tos
	wrlong	0-0, ptra++
	
	' need to get tos and nos back into registers
	jmp	#impl_DROP2
no_ret_values
	mov	ptra, dbase
	rdlong	vbase, --ptra
	rdlong	ptrb, --ptra
	rdlong	dbase, --ptra wz
  if_z	jmp	#impl_HALT

  	' need to restore tos and nos
	jmp    #impl_DROP2

	
impl_HALT
	cogid	pa
	cogstop	pa
	
end_lut
'' end of main interpreter
	' opcode table goes here
	org    $140
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
  _ret_	rdbyte	tos, tos

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
  _ret_	add	tos, ptrb
  
impl_ADD_SP
  _ret_	add	tos, ptra

impl_ADD
	add	tos, nos
 _ret_	rdlong	nos, --ptra

impl_SUB
	subr	tos, nos
 _ret_	rdlong	nos, --ptra

impl_AND
	and	tos, nos
 _ret_	rdlong	nos, --ptra

impl_IOR
	or	tos, nos
 _ret_	rdlong	nos, --ptra

impl_XOR
	xor	tos, nos
 _ret_	rdlong	nos, --ptra

impl_SIGNX
	signx	nos, tos
	mov	tos, nos
 _ret_	rdlong	nos, --ptra

impl_ZEROX
	zerox	nos, tos
	mov	tos, nos
 _ret_	rdlong	nos, --ptra

impl_SHL
	shl	nos, tos
	mov	tos, nos
 _ret_	rdlong	nos, --ptra

impl_SHR
	shr	nos, tos
	mov	tos, nos
 _ret_	rdlong	nos, --ptra
 
impl_SAR
	sar	nos, tos
	mov	tos, nos
 _ret_	rdlong	nos, --ptra

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

impl_NEG
  _ret_	neg	tos, tos

impl_NOT
  _ret_	not	tos, tos

impl_ABS
  _ret_	abs	tos, tos

impl_PUSHI32
	call	#\impl_DUP
  _ret_	rdlong	tos, ptrb++

impl_PUSHA
	call	#\impl_DUP
	sub	ptrb, #1	' back up
  	rdlong	tos, ptrb++
  _ret_ shr	tos, #8		' remove opcode

impl_PUSHI16
	call	#\impl_DUP
	rdword	tos, ptrb++
  _ret_	signx	tos, #15

impl_PUSHI8
	call	#\impl_DUP
	rdbyte	tos, ptrb++
  _ret_	signx	tos, #7

impl_WAITX
	waitx	tos
	jmp	#\impl_DROP

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

impl_BRA
	rdword	tmp, ptrb++
	signx	tmp, #15
  _ret_	add	ptrb, tmp


' final tail stuff for interpreter

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
	
	alignl
dump_regs
	mov	ser_debug_arg1, ##pc_msg
	call	#ser_debug_str
	mov	ser_debug_arg1, ptrb
	call	#ser_debug_hex
	
	mov	ser_debug_arg1, ##sp_msg
	call	#ser_debug_str
	mov	ser_debug_arg1, ptra
	call	#ser_debug_hex

	mov	ser_debug_arg1, ##tos_msg
	call	#ser_debug_str
	mov	ser_debug_arg1, tos
	call	#ser_debug_hex

	mov	ser_debug_arg1, ##dbase_msg
	call	#ser_debug_str
	mov	ser_debug_arg1, dbase
	call	#ser_debug_hex

	jmp	#ser_debug_nl
#endif ' SERIAL_DEBUG

' labels at and of code/data
	alignl
5	long	0	' stack
