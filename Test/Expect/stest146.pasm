pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_getA
	add	ptr__dat__, #8
	rdlong	result1, ptr__dat__
	sub	ptr__dat__, #8
_getA_ret
	ret

_getB
	add	ptr__dat__, #16
	rdlong	result1, ptr__dat__
	sub	ptr__dat__, #16
_getB_ret
	ret

_getC
	add	ptr__dat__, #20
	rdlong	result1, ptr__dat__
	sub	ptr__dat__, #20
_getC_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00[24]
	org	COG_BSS_START
	fit	496
