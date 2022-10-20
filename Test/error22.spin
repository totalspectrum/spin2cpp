'
' test for tab/space mixing
'
dat

y long 0

pub main()
        all_tabs(1)
	all_spaces(2)   ' warning started with tabs
        tester()	' now warn we're back to spaces

pub all_tabs(x)
	if x
		OUTA := 1	' comment
	        y++  		' mixed tabs / spaces
		OUTB := x
        OUTB := 2	' should trigger warning
        INA := 3	' comment
  
pub all_spaces(z)
        OUTA := 2
        repeat z
            OUTA++		' tabs OK
	    OUTB++		' tabs not OK
        INA := 1
	y++			' this line has tabs (BAD)
        INB := 2

pub tester()
	        y++  		' mixed tabs / spaces
    all_tabs(1)
