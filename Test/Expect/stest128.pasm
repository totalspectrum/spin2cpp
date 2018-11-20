pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	mov	_var_01, #2
	mov	_var_00, #4 wz
LR__0001
	rdlong	_tmp001_, ptr__dat__
	sar	_tmp001_, _var_01
	wrlong	_tmp001_, ptr__dat__
	add	ptr__dat__, #4
	rdlong	_tmp001_, ptr__dat__
	shr	_tmp001_, _var_01
	wrlong	_tmp001_, ptr__dat__
	sub	ptr__dat__, #4
	djnz	_var_00, #LR__0001
_main_ret
	ret

ptr__dat__
	long	@@@_dat_
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00, $00, $00, $00, $00, $00, $00, $00
	org	COG_BSS_START
_tmp001_
	res	1
_var_00
	res	1
_var_01
	res	1
	fit	496
