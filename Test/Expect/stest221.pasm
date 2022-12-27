con
	_clkfreq = 160000000
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	arg01, #2
LR__0001
	mov	outb, arg01
	mov	outb, ptr_L__0006_
	djnz	arg01, #LR__0001
_demo_ret
	ret

ptr_L__0006_
	long	@@@LR__0010
COG_BSS_START
	fit	496

LR__0010
	byte	"goodbye"
	byte	0
	org	COG_BSS_START
arg01
	res	1
	fit	496
