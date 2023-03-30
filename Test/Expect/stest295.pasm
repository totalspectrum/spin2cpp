con
	_clkfreq = 80000000
	A = 2
	B = 3
pub main
  coginit(0, @entry, 0)
dat
	org	0
	long
_dat_
	byte	$23, $23, $23, $23, $aa, $aa, $aa, $aa, $ff, $ff, $ff, $ff
