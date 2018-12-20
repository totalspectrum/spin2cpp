pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	mov	_var01, #99
	mov	_var02, arg01 wz
 if_e	jmp	#_case__0003
	cmp	_var02, #1 wz
 if_e	jmp	#_case__0004
	jmp	#_case__0005
_case__0003
_case__0004
	mov	_var01, #2
	jmp	#_endswitch_0002
_case__0005
_endswitch_0002
	mov	result1, _var01
_foo_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg01
	res	1
	fit	496
