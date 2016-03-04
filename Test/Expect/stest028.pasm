DAT
	org	0
divmod16
	mov	divmod16_x_, arg1_
	mov	divmod16_y_, arg2_
	mov	muldiva_, divmod16_x_
	mov	muldivb_, divmod16_y_
	call	#divide_
	mov	divmod16_q_, muldivb_
	mov	muldiva_, divmod16_x_
	mov	muldivb_, divmod16_y_
	call	#divide_
	shl	divmod16_q_, #16
	or	divmod16_q_, muldiva_
	mov	result_, divmod16_q_
divmod16_ret
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

arg1_
	long	0
arg2_
	long	0
divmod16_q_
	long	0
divmod16_x_
	long	0
divmod16_y_
	long	0
itmp1_
	long	0
itmp2_
	long	0
muldiva_
	long	0
muldivb_
	long	0
result_
	long	0
