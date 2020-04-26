pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fetch
	mov	_fetch_x, arg01
	mov	_fetch_x + 1, arg02
	mov	result1, _fetch_x + 1
_fetch_ret
	ret

_fetch2
	mov	_fetch2_B_0001 + 1, arg02
	mov	_fetch_x, arg01
	mov	_fetch_x + 1, arg02
	mov	result1, _fetch_x + 1
_fetch2_ret
	ret

_fetchu
	rdlong	_fetch_x, ptr__dat__
	add	ptr__dat__, #4
	rdlong	arg02, ptr__dat__
	sub	ptr__dat__, #4
	mov	_fetch_x + 1, arg02
	mov	result1, _fetch_x + 1
_fetchu_ret
	ret

_copy
	add	ptr__dat__, #8
	rdlong	_var01, ptr__dat__
	add	ptr__dat__, #4
	rdlong	_var02, ptr__dat__
	sub	ptr__dat__, #12
	wrlong	_var01, ptr__dat__
	add	ptr__dat__, #4
	wrlong	_var02, ptr__dat__
	sub	ptr__dat__, #4
_copy_ret
	ret
wrcog
    mov    0-0, 0-0
wrcog_ret
    ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	org	COG_BSS_START
_fetch2_B_0001
	res	2
_fetch2_y
	res	1
_fetch_x
	res	2
_tmp004_
	res	1
_var01
	res	1
_var02
	res	1
arg01
	res	1
arg02
	res	1
fetch2_tmp004_
	res	1
	fit	496
