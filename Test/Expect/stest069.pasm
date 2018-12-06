pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	_demo__idx__0001, #7
LR__0001
	mov	arg01, imm_40000000_
	add	arg01, cnt
	waitcnt	arg01, #0
	djnz	_demo__idx__0001, #LR__0001
LR__0002
	jmp	#LR__0002
_demo_ret
	ret

_exit
LR__0003
	jmp	#LR__0003
_exit_ret
	ret

_pauseabit
	mov	arg01, imm_40000000_
	add	arg01, cnt
	waitcnt	arg01, #0
_pauseabit_ret
	ret

imm_40000000_
	long	40000000
COG_BSS_START
	fit	496
	org	COG_BSS_START
_demo__idx__0001
	res	1
arg01
	res	1
	fit	496
