pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	mov	result1, #99
	cmp	arg01, #0 wz
 if_e	jmp	#LR__0001
	cmp	arg01, #1 wz
 if_e	jmp	#LR__0002
	jmp	#LR__0003
LR__0001
LR__0002
	mov	result1, #2
LR__0003
_foo_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
