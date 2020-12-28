pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	outa, #12
	mov	result1, #6
	mov	outb, #6
_demo_ret
	ret

_answer
	mov	result1, arg01
	add	result1, arg01
	add	result1, arg01
_answer_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
