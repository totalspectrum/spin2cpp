DAT
	org	0

_ez_pulse_in
	mov	_ez_pulse_in_r, #1
	shl	_ez_pulse_in_r, arg1_
	waitpne	_ez_pulse_in_r, _ez_pulse_in_r
	waitpeq	_ez_pulse_in_r, _ez_pulse_in_r
	neg	muldiva_, CNT
	waitpne	_ez_pulse_in_r, _ez_pulse_in_r
	add	muldiva_, CNT
	mov	muldivb_, imm_1000000_
	call	#divide_
	mov	result_, muldivb_
_ez_pulse_in_ret
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

_ez_pulse_in_r
	long	0
arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
imm_1000000_
	long	1000000
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
