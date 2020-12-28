con
	pin = 0
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_start
LR__0001
	mov	ctra, imm_1073741824_
	mov	frqa, #1
	mov	phsa, #0
	jmp	#LR__0001
_start_ret
	ret

__lockreg
	long	0
imm_1073741824_
	long	1073741824
ptr___lockreg_
	long	@@@__lockreg
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
arg05
	res	1
	fit	496
