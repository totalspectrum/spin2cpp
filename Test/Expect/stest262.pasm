pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_func1
	mov	_var01, ina
	and	_var01, #1
	shl	_var01, #2
	add	_var01, ptr__dat__
	rdlong	result1, _var01
_func1_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	long	@@@_dat_ + 12
	long	@@@_dat_ + 18
	byte	$00, $00, $00, $00, $68, $65, $6c, $6c, $6f, $00, $67, $6f, $6f, $64, $62, $79
	byte	$65, $00, $00, $00
	org	COG_BSS_START
_var01
	res	1
	fit	496
