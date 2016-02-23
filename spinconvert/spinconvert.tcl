package require Tk

set COMPILE ./bin/spin2cpp
set OUTPUT "--asm"
set EXT ".pasm"
set radioOut 1

proc resetOutputVars { } {
    global OUTPUT
    global EXT
    global radioOut
    global SPINFILE
    global PASMFILE
    
    if { $radioOut == 1 } {
	set OUTPUT "--asm"
	set EXT ".pasm"
    }
    if { $radioOut == 2 } {
	set OUTPUT "--ccode"
	set EXT ".c"
    }
    if { $radioOut == 3 } {
	set OUTPUT "--normalize"
	set EXT ".cpp"
    }
    set PASMFILE ""
    if { [string length $SPINFILE] != 0 } {
	regenOutput $SPINFILE
    }
}

proc loadFileToWindow { fname win } {
    set fp [open $fname r]
    set file_data [read $fp]
    close $fp
    $win replace 1.0 end $file_data
}

proc saveFileFromWindow { fname win } {
    set fp [open $fname w]
    set file_data [$win get 1.0 end]
    puts $fp $file_data
    close $fp
    regenOutput $fname
}

proc regenOutput { spinfile } {
    global COMPILE
    global PASMFILE
    global OUTPUT
    global EXT
    
    set outname $PASMFILE
    if { [string length $outname] == 0 } {
	set outname [file rootname $spinfile]
	set outname "$outname$EXT"
	set PASMFILE $outname
    }
    set errout ""
    set status 0
    if {[catch {exec -ignorestderr $COMPILE $OUTPUT $spinfile 2>@1} errout options]} {
	set status 1
    }
    if { $status != 0 } {
	tk_messageBox -icon error -type ok -message "Compilation failed"
    }
    loadFileToWindow $outname .out.txt
    .bot.txt replace 1.0 end $errout
}

set SpinTypes {
    {{Spin files}   {.spin} }
    {{All files}    *}
}

proc loadNewSpinFile {} {
    global SPINFILE
    global SpinTypes
    set filename [tk_getOpenFile -filetypes $SpinTypes -defaultextension ".spin" ]
    if { [string length $filename] == 0 } {
	return
    }
    loadFileToWindow $filename .orig.txt
    regenOutput $filename
    set SPINFILE $filename
    wm title . $SPINFILE
}

proc saveSpinFile {} {
    global SPINFILE
    global SpinTypes
    
    if { [string length $SPINFILE] == 0 } {
	set filename [tk_getSaveFile -initialfile $SPINFILE -filetypes $SpinTypes -defaultextension ".spin" ]
	if { [string length $filename] == 0 } {
	    return
	}
	set SPINFILE $filename
    }
    
    saveFileFromWindow $SPINFILE .orig.txt
    wm title . $SPINFILE
}

proc saveSpinAs {} {
    global SPINFILE
    global SpinTypes
    set filename [tk_getSaveFile -filetypes $SpinTypes -defaultextension ".spin" ]
    if { [string length $filename] == 0 } {
	return
    }
    set SPINFILE $filename
    wm title . $SPINFILE
    saveSpinFile
}

set aboutMsg {
Convert .spin to PASM/C/C++
Copyright 2011-2016 Total Spectrum Software Inc.
------
This is an incomplete preview version!
There is no warranty and no guarantee that
output will be correct.    
}

proc doAbout {} {
    global aboutMsg
    tk_messageBox -icon info -type ok -message "Spin Converter" -detail $aboutMsg
}

proc doHelp {} {
    toplevel .help
    frame .help.f
    text .help.f.txt -wrap none -yscroll { .help.f.v set } -xscroll { .help.f.h set }
    scrollbar .help.f.v -orient vertical -command { .help.f.txt yview }
    scrollbar .help.f.h -orient horizontal -command { .help.f.txt xview }

    grid columnconfigure .help {0 1} -weight 1
    grid rowconfigure .help 0 -weight 1
    grid .help.f -sticky nsew
    
    grid .help.f.txt .help.f.v -sticky nsew
    grid .help.f.h -sticky nsew
    grid rowconfigure .help.f .help.f.txt -weight 1
    grid columnconfigure .help.f .help.f.txt -weight 1

    loadFileToWindow README.txt .help.f.txt
    wm title .help "Spin Converter help"
}

menu .mbar
. configure -menu .mbar
menu .mbar.file -tearoff 0
menu .mbar.options -tearoff 0
menu .mbar.help -tearoff 0

.mbar add cascade -menu .mbar.file -label File
.mbar.file add command -label "Open Spin..." -accelerator "^O" -command { loadNewSpinFile }
.mbar.file add command -label "Save Spin" -accelerator "^S" -command { saveSpinFile }
.mbar.file add command -label "Save Spin As..." -command { saveSpinAs }
.mbar.file add separator
.mbar.file add command -label Exit -accelerator "^Q" -command { exit }

.mbar add cascade -menu .mbar.options -label Options
.mbar.options add radiobutton -label "Pasm Output" -variable radioOut -value 1 -command { resetOutputVars }
.mbar.options add radiobutton -label "C Output" -variable radioOut -value 2 -command { resetOutputVars }
.mbar.options add radiobutton -label "C++ Output" -variable radioOut -value 3 -command { resetOutputVars }

.mbar add cascade -menu .mbar.help -label Help
.mbar.help add command -label "Help" -command { doHelp }
.mbar.help add separator
.mbar.help add command -label "About..." -command { doAbout }

wm title . "Spin Converter"

frame .orig
text .orig.txt -wrap none -xscroll {.orig.h set} -yscroll {.orig.v set}
scrollbar .orig.v -orient vertical -command {.orig.txt yview}
scrollbar .orig.h -orient horizontal -command {.orig.txt. xview}

frame .out
text .out.txt -wrap none -xscroll {.out.h set} -yscroll {.out.v set}
scrollbar .out.v -orient vertical -command {.out.txt yview}
scrollbar .out.h -orient horizontal -command {.out.txt. xview}

frame .bot
text .bot.txt -wrap none -xscroll {.bot.h set} -yscroll {.bot.v set} -height 4
scrollbar .bot.v -orient vertical -command {.bot.txt yview}
scrollbar .bot.h -orient horizontal -command {.bot.txt. xview}

grid columnconfigure . {0 1} -weight 1
grid rowconfigure . 0 -weight 1

grid .orig -column 0 -row 0 -columnspan 1 -rowspan 1 -sticky nsew
grid .out -column 1 -row 0 -columnspan 1 -rowspan 1 -sticky nsew
grid .bot -column 0 -row 1 -columnspan 2 -sticky nsew

grid .orig.txt .orig.v -sticky nsew
grid .orig.h           -sticky nsew
grid rowconfigure .orig .orig.txt -weight 1
grid columnconfigure .orig .orig.txt -weight 1

grid .out.txt .out.v -sticky nsew
grid .out.h           -sticky nsew
grid rowconfigure .out .out.txt -weight 1
grid columnconfigure .out .out.txt -weight 1

grid .bot.txt .bot.v -sticky nsew
grid .bot.h -sticky nsew
grid rowconfigure .bot .bot.txt -weight 1
grid columnconfigure .bot .bot.txt -weight 1


.orig.txt insert 1.0 "'' Original Spin code"
.out.txt insert 1.0 "'' Converted file"
.bot.txt insert 1.0 "Compilation output goes here"

bind . <Control-o> { loadNewSpinFile }
bind . <Control-s> { saveSpinFile }
bind . <Control-q> { exit }

set PASMFILE ""

if { $::argc > 0 } {
    loadFileToWindow $argv .orig.txt
    regenOutput $argv[1]
} else {
    set SPINFILE ""
}
