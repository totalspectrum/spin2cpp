pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	mov	outb, arg01
_foo_ret
	ret

_get1
	rdlong	result1, ptr__dat__
_get1_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	long	(@@@_foo)<<16
	org	COG_BSS_START
arg01
	res	1
	fit	496
