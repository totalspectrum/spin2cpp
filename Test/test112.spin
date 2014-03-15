'' test for org and label interaction

CON
   neworg = $70

PUB me
    return main_begin

DAT
	org
first
	long	neworg
	long	main_begin
	long	foo2

main_begin org neworg
	nop
foo2
	long 1
