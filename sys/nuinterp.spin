dat
''
'' Nu code interpreter
'' This is the skeleton from which the actual interpreter is built
'' (opcodes are assigned at compile time so as to minimize the size)
''

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
entry_pc
	long	0	' initial pc
entry_vbase
	long	0	' initial object pointer
entry_dbase
	long	0	' initial frame pointer
entry_sp
	long	0	' initial stack pointer
	
	long	0[$10]	' more reserved words just in case
	
	org	0
real_init
	' check for coginit startup
	cmp	ptra, #0 wz
  if_nz	jmp	#spininit
  
  	' first time run
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
	' load LUT code
	loc    ptrb, #@start_lut
	setq2  #(end_lut - start_lut)
	rdlong 0, ptrb
	jmp    #start_lut

	org    $1e8
tos	res    1
nos	res    1
tmp	res    1
tmp2	res    1
vbase	res    1
dbase	res    1
old_pc	res    1
	
	org    $200
start_lut
	' initialization code
	mov	old_pc, #0
	' copy jump table to COG RAM
	loc    pa, #@OPC_TABLE
	setq   #(OPC_TABLE_END-OPC_TABLE)-1
	rdlong OPC_TABLE, pa
	
	' interpreter loop
main_loop
	rdbyte	pa, ptrb++
	altgw	pa, #OPC_TABLE
	getword	tmp
	call	tmp
	jmp	#main_loop
end_lut

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
