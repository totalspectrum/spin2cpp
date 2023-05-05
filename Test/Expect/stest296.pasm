con
	_clkfreq = 80000000
	A = 2
	B = 3
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	mov	outa, #1
	mov	outa, #0
	mov	_var01, #2
	mov	outb, #1
	mov	result1, _var01
_main_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
	fit	496
