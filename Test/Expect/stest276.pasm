pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_get
	rdlong	result1, ptr__dat__
	add	ptr__dat__, #4
	rdlong	result2, ptr__dat__
	sub	ptr__dat__, #4
_get_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
result2
	long	1
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00, $00, $00, $80, $01, $00, $00, $00
	org	COG_BSS_START
	fit	496
