pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_rx
LR__0001
	call	#_rxcheck
	cmps	result1, #0 wc
 if_b	jmp	#LR__0001
_rx_ret
	ret

_rxcheck
	mov	_var01, outa
	and	_var01, #255 wz
 if_e	neg	result1, #1
 if_ne	mov	result1, _var01
_rxcheck_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
	fit	496
