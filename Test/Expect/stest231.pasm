pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	arg03, #0
	mov	arg04, #4
	mov	arg01, imm_4660_
	mov	arg02, imm_22136_
	call	#_shl64
	mov	outa, result1
	mov	outb, result2
_demo_ret
	ret

_i64
	mov	result2, arg02
	mov	result1, arg01
_i64_ret
	ret

_shl64
	shl	arg01, arg04
	mov	result1, #32
	sub	result1, arg04
	mov	result2, arg02
	shr	result2, result1
	or	arg01, result2
	shl	arg02, arg04
	mov	result2, arg02
	mov	result1, arg01
_shl64_ret
	ret

_info
	mov	outa, arg01
	mov	outb, arg02
_info_ret
	ret

imm_22136_
	long	22136
imm_4660_
	long	4660
result1
	long	0
result2
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
arg03
	res	1
arg04
	res	1
	fit	496
