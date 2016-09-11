PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_simplemul
	mov	_var_03, #0
	mov	_var_r, #0
L__0001
	cmps	_var_03, arg1 wc,wz
 if_b	add	_var_r, arg2
 if_b	add	_var_03, #1
 if_b	jmp	#L__0001
	mov	result1, _var_r
_simplemul_ret
	ret

_var_03
	long	0
_var_r
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
