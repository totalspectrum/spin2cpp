pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	rdlong	blah_tmp004_, ptr__dat__
	rdlong	blah_tmp001_, blah_tmp004_
	rdlong	blah_tmp002_, blah_tmp001_
	add	blah_tmp001_, #4
	rdlong	blah_tmp003_, blah_tmp001_
	mov	blah_tmp006_, objptr
	mov	objptr, blah_tmp002_
	call	blah_tmp003_
	mov	objptr, blah_tmp006_
_blah_ret
	ret

__lockreg
	long	0
objptr
	long	@@@objmem
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
	byte	$00, $00, $00, $00
objmem
	long	0[0]
	org	COG_BSS_START
blah_tmp001_
	res	1
blah_tmp002_
	res	1
blah_tmp003_
	res	1
blah_tmp004_
	res	1
blah_tmp006_
	res	1
	fit	496
