pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_count
	mov	_var01, #0
LR__0001
	mov	outa, _var01
	add	_var01, #1
	cmps	_var01, #4 wc
 if_b	jmp	#LR__0001
_count_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
	fit	496
