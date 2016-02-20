package require Tk

set COMPILE ../build/spin2cpp

proc loadFileToWindow { fname win } {
    set fp [open $fname r]
    set file_data [read $fp]
    close $fp
    $win replace 1.0 end $file_data
}

proc regenOutput { spinfile } {
    global COMPILE
    set outname [file rootname $spinfile]
    set outname "$outname.pasm"
    set errout [exec -ignorestderr $COMPILE --asm $spinfile 2>@1]
    loadFileToWindow $outname .out.txt
    .bot.txt replace 1.0 end $errout
}

proc loadNewSpinFile {} {
    set types {
	{{Spin files}   {.spin} }
	{{All files}    *}
    }
    set filename [tk_getOpenFile -filetypes $types -defaultextension ".spin" ]
    if { [string length $filename] == 0 } {
	return
    }
    loadFileToWindow $filename .orig.txt
    regenOutput $filename
}

menu .mbar
. configure -menu .mbar
menu .mbar.file -tearoff 0
.mbar add cascade -menu .mbar.file -label File -underline 0
.mbar.file add command -label Open... -accelerator "^O" -command { loadNewSpinFile }
.mbar.file add separator
.mbar.file add command -label Exit -accelerator "^Q" -command { exit }

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


.orig.txt insert 1.0 "Original Spin code"
.out.txt insert 1.0 "Converted assembly"

bind . <Control-o> { loadNewSpinFile }
bind . <Control-q> { exit }

if { $::argc > 0 } {
    loadFileToWindow $argv .orig.txt
    regenOutput $argv[1]
} else {
    loadNewSpinFile
}

