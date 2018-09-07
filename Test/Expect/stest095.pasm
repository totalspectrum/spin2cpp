CON
	pin = 0
PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_start
LR__0001
	mov	CTRA, imm_1073741824_
	mov	FRQA, #1
	mov	PHSA, #0
	jmp	#LR__0001
_start_ret
	ret

imm_1073741824_
	long	1073741824
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
arg5
	res	1
	fit	496
