pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_calc1
	mov	muldiva_, arg01
	mov	muldivb_, arg02
	call	#divide_
	wrlong	muldivb_, objptr
	add	arg02, #1
	mov	muldiva_, arg01
	mov	muldivb_, arg02
	call	#divide_
	add	objptr, #4
	wrlong	muldiva_, objptr
	sub	objptr, #4
_calc1_ret
	ret

_calc2
	mov	muldiva_, arg01
	mov	muldivb_, arg02
	call	#multiply_
	wrlong	muldiva_, objptr
	add	objptr, #8
	wrlong	muldivb_, objptr
	sub	objptr, #4
	wrlong	muldiva_, objptr
	sub	objptr, #4
_calc2_ret
	ret

unsmultiply_
       jmpret $, #do_multiply_ nr,wc

multiply_
       abs    muldiva_, muldiva_ wc
       negnc  itmp1_,#1
       abs    muldivb_, muldivb_ wc
       muxnc  itmp1_,#1 wc
do_multiply_
       rcr    muldiva_, #1 wc
       mov    itmp2_, #0
       mov    itmp1_, #32
mul_lp_
 if_c  add    itmp2_, muldivb_ wc
       rcr    itmp2_, #1 wc
       rcr    muldiva_, #1 wc
       djnz    itmp1_, #mul_lp_
       negc   muldivb_, itmp2_
 if_c  neg    muldiva_, muldiva_ wz
 if_c_and_nz sub    muldivb_, #1
multiply__ret
unsmultiply__ret
    ret
' code originally from spin interpreter, modified slightly

unsdivide_
       mov     itmp2_,#0
       jmp     #udiv__

divide_
       abs     muldiva_,muldiva_     wc       'abs(x)
       muxc    itmp2_,divide_haxx_            'store sign of x (mov x,#1 has bits 0 and 31 set)
       abs     muldivb_,muldivb_     wc,wz    'abs(y)
 if_z  jmp     #divbyzero__
 if_c  xor     itmp2_,#1                      'store sign of y
udiv__
divide_haxx_
        mov     itmp1_,#1                    'unsigned divide (bit 0 is discarded)
        mov     DIVCNT,#32
mdiv__
        shr     muldivb_,#1        wc,wz
        rcr     itmp1_,#1
 if_nz   djnz    DIVCNT,#mdiv__
mdiv2__
        cmpsub  muldiva_,itmp1_        wc
        rcl     muldivb_,#1
        shr     itmp1_,#1
        djnz    DIVCNT,#mdiv2__
        shr     itmp2_,#31       wc,wz    'restore sign
        negnz   muldiva_,muldiva_         'remainder
        negc    muldivb_,muldivb_ wz      'division result
divbyzero__
divide__ret
unsdivide__ret
	ret
DIVCNT
	long	0

itmp1_
	long	0
itmp2_
	long	0
objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[3]
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
muldiva_
	res	1
muldivb_
	res	1
	fit	496
