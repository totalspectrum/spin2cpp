PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_checkcmd
	mov	_var_00, #1
LR__0001
LR__0002
	cmps	_var_00, #0 wz
 if_ne	jmp	#LR__0002
	rdlong	_tmp002_, objptr
	add	_tmp002_, #1
	wrlong	_tmp002_, objptr
	mov	_var_00, #0
	jmp	#LR__0001
_checkcmd_ret
	ret

_cmd2
LR__0003
	mov	OUTA, arg1
	mov	arg1, #0
	jmp	#LR__0003
_cmd2_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[1]
	org	COG_BSS_START
_tmp002_
	res	1
_var_00
	res	1
arg1
	res	1
	fit	496
