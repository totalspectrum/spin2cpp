pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	mov	_var01, #0
LR__0001
	rdbyte	result1, arg01 wz
 if_ne	add	arg01, #1
 if_ne	add	_var01, #1
 if_ne	jmp	#LR__0001
	mov	result1, _var01
_foo_ret
	ret

_bar
	mov	arg01, ptr_L__0004_
	call	#_foo
_bar_ret
	ret

ptr_L__0004_
	long	@@@LR__0010
result1
	long	0
COG_BSS_START
	fit	496

LR__0010
	byte	"hello",13,10,"there"
	byte	0
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
