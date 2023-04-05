pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_total
	mov	result1, #0
	mov	_var01, ptr__dat__
	mov	_var02, #4
LR__0001
	rdlong	_var03, _var01
	add	result1, _var03
	add	_var01, #4
	djnz	_var02, #LR__0001
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
_var01
	res	1
_var02
	res	1
_var03
	res	1
	fit	496
