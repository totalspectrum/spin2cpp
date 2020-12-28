con
	x = 13
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	result1, ptr_L__0001_
_demo_ret
	ret

__lockreg
	long	0
ptr_L__0001_
	long	@@@LR__0001
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496

LR__0001
	byte	"hello"
	byte	13
	byte	0
	org	COG_BSS_START
	fit	496
