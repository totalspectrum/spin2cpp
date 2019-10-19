pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_ndiv
	mov	muldiva_, arg01
	mov	muldivb_, #10
	call	#unsdivide_
	mov	result1, muldivb_
_ndiv_ret
	ret

_nmod
	mov	muldiva_, arg01
	mov	muldivb_, #10
	call	#unsdivide_
	mov	result1, muldiva_
_nmod_ret
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
 if_z  jmp     #divbyzero__
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
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
muldiva_
	res	1
muldivb_
	res	1
	fit	496
