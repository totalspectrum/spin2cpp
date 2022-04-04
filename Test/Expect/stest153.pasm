pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	rdlong	blah_tmp004_, ptr__dat__
	rdlong	blah_tmp004_, blah_tmp004_
	rdlong	blah_tmp002_, blah_tmp004_
	add	blah_tmp004_, #4
	rdlong	blah_tmp004_, blah_tmp004_
	mov	blah_tmp006_, objptr
	mov	objptr, blah_tmp002_
	call	blah_tmp004_
	mov	objptr, blah_tmp006_
_blah_ret
	ret

objptr
	long	@@@objmem
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
blah_tmp002_
	res	1
blah_tmp004_
	res	1
blah_tmp006_
	res	1
	fit	496
