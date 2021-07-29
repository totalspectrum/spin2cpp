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
	rdlong	pc, #@entry_pc
	rdlong	vbase, #@entry_vbase
	rdlong	ptra, #@entry_sp
	rdlong	dbase, #@entry_dbase
	jmp	#continue_startup
spininit
	' for Spin startup, stack should contain args, pc, vbase in that order
	rdlong	vbase, --ptra
	rdlong	pc, --ptra
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
pc	res    1
vbase	res    1
dbase	res    1
old_pc	res    1
	
	org    $200
start_lut
	drvnot	#56
	waitx	##10_000_000
	jmp	#start_lut
end_lut

dup
	wrlong	nos, ptra++
 _ret_	mov	nos, tos

drop
	mov	tos, nos
 _ret_	rdlong	nos, --ptra

drop2
	rdlong	tos, --ptra
 _ret_	rdlong	nos, --ptra

swap
	mov	tmp, tos
	mov	tos, nos
 _ret_	mov	nos, tmp
 
	' jump table goes here
	' this needs to be in the same order as the opcodes in nuir.h
JUMP_TABLE
	long   $abcdef01	' magic marker so we can find the opcodes; illegal instruction slot
	jmp	#\ldb_impl
	jmp	#\ldw_impl
	jmp	#\ldl_impl
	jmp	#\ldd_impl
	jmp	#\stb_impl
	jmp	#\stw_impl
	jmp	#\stl_impl
	jmp	#\std_impl
	jmp	#\ldreg_impl
	jmp	#\streg_impl

	jmp	#\add_vbase_impl
	jmp	#\add_dbase_impl
	jmp	#\add_sp_impl
	jmp	#\add_pc_impl

	jmp	#\add_impl
	jmp	#\sub_impl
	jmp	#\and_impl
	jmp	#\ior_impl
	jmp	#\xor_impl
	
	jmp	#\signx_impl
	jmp	#\zerox_impl
	jmp	#\shl_impl
	jmp	#\shr_impl
	jmp	#\sar_impl

	jmp	#\mulu_impl
	jmp	#\muls_impl
	jmp	#\divu_impl
	jmp	#\divs_impl

	jmp	#\neg_impl
	jmp	#\not_impl
	jmp	#\abs_impl

	orgh
ldb_impl
  _ret_	rdbyte tos, tos
ldw_impl
  _ret_ rdword tos, tos
ldl_impl
  _ret_ rdlong tos, tos
ldd_impl
	call	#\dup
	rdlong	nos, nos
	add	tos, #4
  _ret_	rdbyte	tos, tos

stb_impl
	wrbyte	nos, tos
  _ret_	jmp	#\drop2
  
stw_impl
	wrword	nos, tos
  _ret_	jmp	#\drop2
  
stl_impl
	wrlong	nos, tos
  _ret_	jmp	#\drop2
  
std_impl
	mov	tmp, tos
	call	#\drop
	wrlong	nos, tmp
	add	tmp, #4
	wrlong	tos, tmp
  _ret_	jmp	#\drop2

ldreg_impl
	alts	tos
  _ret_	mov	tos, 0-0

streg_impl
	altd	tos
  	mov	0-0, nos
  _ret_	jmp	#\drop2
	
add_vbase_impl
  _ret_	add	tos, vbase
add_dbase_impl
  _ret_	add	tos, dbase
add_pc_impl
  _ret_	add	tos, pc
add_sp_impl
  _ret_	add	tos, sp

add_impl
	add	tos, nos
 _ret_	rdlong	nos, --ptra

sub_impl
	subr	tos, nos
 _ret_	rdlong	nos, --ptra

and_impl
	and	tos, nos
 _ret_	rdlong	nos, --ptra
ior_impl
	or	tos, nos
 _ret_	rdlong	nos, --ptra
xor_impl
	xor	tos, nos
 _ret_	rdlong	nos, --ptra

signx_impl
	signx	nos, tos
	mov	tos, nos
 _ret_	rdlong	nos, --ptra

zerox_impl
	zerox	nos, tos
	mov	tos, nos
 _ret_	rdlong	nos, --ptra

shl_impl
	shl	nos, tos
	mov	tos, nos
 _ret_	rdlong	nos, --ptra
shr_impl
	shr	nos, tos
	mov	tos, nos
 _ret_	rdlong	nos, --ptra
sar_impl
	sar	nos, tos
	mov	tos, nos
 _ret_	rdlong	nos, --ptra

mulu_impl
	qmul	nos, tos
	getqx	nos
 _ret_	getqy	tos

muls_impl
	qmul	nos, tos
	mov	tmp, #0
	cmps	nos, #0 wc
  if_c	add	tmp, tos
	cmps	tos, #0 wc
  if_c	add	tmp, nos
	getqx	nos
	getqy	tos
  _ret_	sub	tos, tmp
	
divu_impl
	qdiv	nos, tos
	getqx	nos
 _ret_	getqy	tos

divs_impl
	abs	nos, nos wc
	muxc	tmp, #1
	abs	tos, tos wc
	qdiv	nos, tos
	getqx	nos
	getqy	tos
  if_c	neg	tos, tos
  	test	tmp, #1 wc
  _ret_	negc	nos, nos
  
neg_impl
  _ret_	neg	tos, tos
not_impl
  _ret_	not	tos, tos
abs_impl
  _ret_	abs	tos, tos

pushi32_impl
	call	#\dup
	rdlong	tos, pc
  _ret_	add	pc, #4

pushi16_impl
	call	#\dup
	rdword	tos, pc
	signx	tos, #15
  _ret_	add	pc, #2

pushi8_impl
	call	#\dup
	rdbyte	tos, pc
	signx	tos, #7
  _ret_	add	pc, #1
