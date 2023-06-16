pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	rdlong	blah_tmp004_, ptr__dat__
	rdlong	blah_tmp004_, blah_tmp004_
	mov	blah_tmp003_, blah_tmp004_
	shr	blah_tmp003_, #16
	mov	blah_tmp005_, objptr
	mov	objptr, blah_tmp004_
	call	blah_tmp003_
	mov	objptr, blah_tmp005_
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
blah_tmp003_
	res	1
blah_tmp004_
	res	1
blah_tmp005_
	res	1
	fit	496
