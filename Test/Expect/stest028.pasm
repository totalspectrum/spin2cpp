DAT
	org	0

_divmod16
	mov	_divmod16_x, arg1
	mov	_divmod16_y, arg2
	mov	muldiva_, _divmod16_x
	mov	muldivb_, _divmod16_y
	call	#divide_
	mov	_divmod16_q, muldivb_
	mov	muldiva_, _divmod16_x
	mov	muldivb_, _divmod16_y
	call	#divide_
	shl	_divmod16_q, #16
	or	_divmod16_q, muldiva_
	mov	result1, _divmod16_q
_divmod16_ret
	ret
' code originally from spin interpreter, modified slightly

divide_
       abs     muldiva_,muldiva_     wc       'abs(x)
       muxc    itmp2_,#%11                    'store sign of x
       abs     muldivb_,muldivb_     wc,wz    'abs(y)
 if_c  xor     itmp2_,#%10                    'store sign of y
        mov     itmp1_,#0                    'unsigned divide
        mov     CNT,#32             ' use CNT shadow register
mdiv__
        shr     muldivb_,#1        wc,wz
        rcr     itmp1_,#1
 if_nz   djnz    CNT,#mdiv__
mdiv2__
        cmpsub  muldiva_,itmp1_        wc
        rcl     muldivb_,#1
        shr     itmp1_,#1
        djnz    CNT,#mdiv2__
        test    itmp2_,#1        wc       'restore sign, remainder
        negc    muldiva_,muldiva_ 
        test    itmp2_,#%10      wc       'restore sign, division result
        negc    muldivb_,muldivb_
divide__ret
	ret

_divmod16_q
	long	0
_divmod16_x
	long	0
_divmod16_y
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
itmp1_
	long	0
itmp2_
	long	0
muldiva_
	long	0
muldivb_
	long	0
result1
	long	0
	fit	496
