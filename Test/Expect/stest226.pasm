pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_f1
	rdlong	outa, ptr__dat__
	add	ptr__dat__, #4
	rdlong	outb, ptr__dat__
	sub	ptr__dat__, #4
_f1_ret
	ret

_f2
	add	ptr__dat__, #8
	rdlong	outa, ptr__dat__
	add	ptr__dat__, #4
	rdlong	outb, ptr__dat__
	sub	ptr__dat__, #12
_f2_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
ptr__dat__
	long	@@@_dat_
COG_BSS_START
	fit	496
	long
_dat_
	byte	$01, $00, $00, $00, $02, $00, $00, $00, $03, $00, $00, $00, $04, $00, $00, $00
	org	COG_BSS_START
	fit	496
