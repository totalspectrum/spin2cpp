PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_calc1
	mov	muldiva_, arg1
	mov	muldivb_, arg2
	call	#divide_
	wrlong	muldivb_, objptr
	add	arg2, #1
	mov	muldiva_, arg1
	mov	muldivb_, arg2
	call	#divide_
	add	objptr, #4
	wrlong	muldiva_, objptr
	sub	objptr, #4
_calc1_ret
	ret

_calc2
	mov	muldiva_, arg1
	mov	muldivb_, arg2
	call	#multiply_
	wrlong	muldiva_, objptr
	add	objptr, #8
	wrlong	muldivb_, objptr
	sub	objptr, #4
	wrlong	muldiva_, objptr
	sub	objptr, #4
_calc2_ret
	ret

multiply_
	mov	itmp2_, muldiva_
	xor	itmp2_, muldivb_
	abs	muldiva_, muldiva_
	abs	muldivb_, muldivb_
	mov	result1, #0
	mov	itmp1_, #32
	shr	muldiva_, #1 wc
mul_lp_
 if_c	add	result1, muldivb_ wc
	rcr	result1, #1 wc
	rcr	muldiva_, #1 wc
	djnz	itmp1_, #mul_lp_
	shr	itmp2_, #31 wz
 if_nz	neg	result1, result1
 if_nz	neg	muldiva_, muldiva_ wz
 if_nz	sub	result1, #1
	mov	muldivb_, result1
multiply__ret
	ret
' code originally from spin interpreter, modified slightly

unsdivide_
       mov     itmp2_,#0
       jmp     #udiv__

divide_
       abs     muldiva_,muldiva_     wc       'abs(x)
       muxc    itmp2_,#%11                    'store sign of x
       abs     muldivb_,muldivb_     wc,wz    'abs(y)
 if_c  xor     itmp2_,#%10                    'store sign of y
udiv__
        mov     itmp1_,#0                    'unsigned divide
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
        test    itmp2_,#1        wc       'restore sign, remainder
        negc    muldiva_,muldiva_ 
        test    itmp2_,#%10      wc       'restore sign, division result
        negc    muldivb_,muldivb_
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
arg1
	res	1
arg2
	res	1
muldiva_
	res	1
muldivb_
	res	1
	fit	496
