pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main1
	mov	_var01, #2
LR__0001
	mov	_var02, _var01
	add	_var01, #1
	abs	outb, _var02
	jmp	#LR__0001
_main1_ret
	ret

_main2
	mov	_var01, #2
LR__0002
	add	_var01, #1
	abs	outb, _var01
	jmp	#LR__0002
_main2_ret
	ret

_main3
	mov	_main3_s, #2
LR__0003
	mov	arg01, _main3_s
	call	#__system___lfsr_backward
	mov	_main3_s, result1
	abs	outb, _main3_s
	jmp	#LR__0003
_main3_ret
	ret

_main4
	mov	_var01, #2
LR__0004
	mov	_var02, _var01
	mov	_var01, inb
	abs	outb, _var02
	jmp	#LR__0004
_main4_ret
	ret

__system___lfsr_backward
	cmp	arg01, #0 wz
 if_e	mov	arg01, #1
	mov	_var01, #32
LR__0005
	test	arg01, #23 wc
	rcr	arg01, #1
	djnz	_var01, #LR__0005
	mov	result1, arg01
__system___lfsr_backward_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_main3_s
	res	1
_var01
	res	1
_var02
	res	1
arg01
	res	1
	fit	496
