pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_waitcycles
	mov	_var01, cnt
	add	_var01, arg1
LR__0001
	cmps	cnt, _var01 wc,wz
 if_b	jmp	#LR__0001
_waitcycles_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg1
	res	1
	fit	496
