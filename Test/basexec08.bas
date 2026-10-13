'' test for BASIC's signed/unsigned comparison rules

''
'' send a special exit status for propeller-load
'' FF 00 xx, where xx is the exit status
''

sub doexit(status)
  print \255; \0; \status;
  ' just loop here so that quickstart board does not
  ' let the pins float
  do
  loop
end sub

dim ui as uinteger
dim i as integer

sub checkiu(i as integer, ui as uinteger)
    print "i = "; i; " ui = "; ui
    if (i < ui) and (ui > i) then
       print "  i < ui"
    endif
    if (i <= ui) and (ui >= i) then
       print "  i <= ui"
    endif
    if (i == ui) and (ui == i) then
       print "  i == ui"
    endif
    if (i <> ui) and (ui <> i) then
       print "  i <> ui"
    endif
    if (i > ui) and (ui < i) then
       print "  i > ui"
    endif
    if (i >= ui) and (ui <= i) then
       print "  i >= ui"
    endif
end sub

checkiu(1, 1)
checkiu(1, 2)
checkiu(32768, 7)
checkiu(-1, 1)
checkiu(1, -1)
checkiu(-1, -1)
print "** done ** "
doexit(0)
