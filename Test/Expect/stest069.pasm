pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	_demo__idx__0000, #7
LR__0001
	mov	arg01, imm_40000000_
	add	arg01, cnt
	waitcnt	arg01, #0
	djnz	_demo__idx__0000, #LR__0001
LR__0002
	jmp	#LR__0002
_demo_ret
	ret

_exit
LR__0003
	jmp	#LR__0003
_exit_ret
	ret

_PauseABit
	mov	arg01, imm_40000000_
	add	arg01, cnt
	waitcnt	arg01, #0
_PauseABit_ret
	ret

__lockreg
	long	0
imm_40000000_
	long	40000000
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
_demo__idx__0000
	res	1
arg01
	res	1
	fit	496
