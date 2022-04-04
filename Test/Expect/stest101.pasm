pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_checkcmd
	mov	_var01, #1
LR__0001
LR__0002
	cmp	_var01, #0 wz
 if_ne	jmp	#LR__0002
	rdlong	_var01, objptr
	add	_var01, #1
	wrlong	_var01, objptr
	mov	_var01, #0
	jmp	#LR__0001
_checkcmd_ret
	ret

_cmd2
LR__0003
	mov	outa, arg01
	mov	arg01, #0
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
_var01
	res	1
arg01
	res	1
	fit	496
