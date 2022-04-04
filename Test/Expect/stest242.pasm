pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_lfs_trunc
	rdlong	result1, arg01
	and	result1, #255
	add	ptr__dat__, #4
	rdlong	lfs_trunc_tmp003_, ptr__dat__
	sub	ptr__dat__, #4
	add	result1, lfs_trunc_tmp003_
	wrlong	result1, arg01
	add	arg01, #4
	rdlong	lfs_trunc_tmp001_, arg01
	and	lfs_trunc_tmp001_, #1
	wrlong	lfs_trunc_tmp001_, arg01
_lfs_trunc_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	org	COG_BSS_START
arg01
	res	1
lfs_trunc_tmp001_
	res	1
lfs_trunc_tmp003_
	res	1
	fit	496
