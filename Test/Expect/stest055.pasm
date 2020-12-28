pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fetchx
	add	ptr__dat__, #4
	rdlong	result1, ptr__dat__
	sub	ptr__dat__, #4
_fetchx_ret
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
	byte	$44, $33, $22, $11, $88, $77, $66, $55, $cc, $bb, $aa, $99
	org	COG_BSS_START
	fit	496
