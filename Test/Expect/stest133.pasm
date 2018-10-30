PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_foo
	mov	_var_02, #0
LR__0001
	rdbyte	_tmp001_, arg1 wz
 if_ne	add	arg1, #1
 if_ne	add	_var_02, #1
 if_ne	jmp	#LR__0001
	mov	result1, _var_02
_foo_ret
	ret

_bar
	mov	arg1, ptr_L__0025_
	call	#_foo
_bar_ret
	ret

ptr_L__0025_
	long	@@@LR__0002
result1
	long	0
COG_BSS_START
	fit	496

LR__0002
	byte	"hello",13,10,"there"
	byte	0
	org	COG_BSS_START
_tmp001_
	res	1
_var_02
	res	1
arg1
	res	1
	fit	496
