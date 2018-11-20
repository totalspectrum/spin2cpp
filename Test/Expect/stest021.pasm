pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_waitcycles
	mov	_var_01, cnt
	add	_var_01, arg1
LR__0001
	cmps	cnt, _var_01 wc,wz
 if_b	jmp	#LR__0001
_waitcycles_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_01
	res	1
arg1
	res	1
	fit	496
