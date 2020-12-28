pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_mylen
	mov	__system____builtin_strlen_r, #0
LR__0001
	rdbyte	_tmp001_, arg01 wz
 if_ne	add	__system____builtin_strlen_r, #1
 if_ne	add	arg01, #1
 if_ne	jmp	#LR__0001
	mov	result1, __system____builtin_strlen_r
_mylen_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
__system____builtin_strlen_r
	res	1
_tmp001_
	res	1
arg01
	res	1
	fit	496
