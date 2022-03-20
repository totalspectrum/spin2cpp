pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_rx
LR__0001
	call	#_rxcheck
	mov	_rx__temp__0000, result1
	mov	_rx_rxbyte, _rx__temp__0000
	cmps	_rx__temp__0000, #0 wc
 if_b	jmp	#LR__0001
	mov	result1, _rx_rxbyte
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
_rx__temp__0000
	res	1
_rx_rxbyte
	res	1
_var01
	res	1
	fit	496
