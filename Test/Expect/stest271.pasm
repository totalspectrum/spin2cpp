pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_test1up
	mov	_var01, #1
LR__0001
	xor	outa, _var01
	add	_var01, #1
	cmp	_var01, #9 wc
 if_b	jmp	#LR__0001
_test1up_ret
	ret

_test1dn
	mov	_var01, #8
LR__0002
	xor	outa, _var01
	djnz	_var01, #LR__0002
_test1dn_ret
	ret

_test2up
	mov	_var01, #8
LR__0003
	xor	outa, #1
	djnz	_var01, #LR__0003
_test2up_ret
	ret

_test2dn
	mov	_var01, #8
LR__0004
	xor	outa, #1
	djnz	_var01, #LR__0004
_test2dn_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
	fit	496
