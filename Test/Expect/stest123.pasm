PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_total
	mov	_var_01, #0
	mov	_var_04, #0
	mov	_tmp002_, ptr__dat__
	add	_var_04, _tmp002_
	mov	_var_02, #4
LR__0001
	rdlong	_tmp002_, _var_04
	add	_var_01, _tmp002_
	add	_var_04, #4
	djnz	_var_02, #LR__0001
	mov	result1, _var_01
_total_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$02, $00, $00, $00, $04, $00, $00, $00, $fd, $ff, $ff, $ff, $fc, $ff, $ff, $ff
	org	COG_BSS_START
_tmp002_
	res	1
_var_01
	res	1
_var_02
	res	1
_var_04
	res	1
	fit	496
