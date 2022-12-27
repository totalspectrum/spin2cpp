pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	_var01, #7
LR__0001
	mov	arg01, imm_40000000_
	add	arg01, cnt
	waitcnt	arg01, #0
	djnz	_var01, #LR__0001
LR__0002
	jmp	#LR__0002
_demo_ret
	ret

_exit
LR__0010
	jmp	#LR__0010
_exit_ret
	ret

_PauseABit
	mov	arg01, imm_40000000_
	add	arg01, cnt
	waitcnt	arg01, #0
_PauseABit_ret
	ret

imm_40000000_
	long	40000000
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
