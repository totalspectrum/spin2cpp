PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_divmod16
	mov	_var_01, arg1
	mov	_var_02, arg2
	mov	muldiva_, _var_01
	mov	muldivb_, _var_02
	call	#divide_
	mov	_var_03, muldivb_
	mov	muldiva_, _var_01
	mov	muldivb_, _var_02
	call	#divide_
	shl	_var_03, #16
	or	_var_03, muldiva_
	mov	result1, _var_03
_divmod16_ret
	ret
' code originally from spin interpreter, modified slightly

divide_
       abs     muldiva_,muldiva_     wc       'abs(x)
       muxc    itmp2_,#%11                    'store sign of x
       abs     muldivb_,muldivb_     wc,wz    'abs(y)
 if_c  xor     itmp2_,#%10                    'store sign of y
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
	ret
DIVCNT
	long	0

_var_01
	long	0
_var_02
	long	0
_var_03
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
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
