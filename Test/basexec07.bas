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

#ifdef __P2__
'' date/time tests
'' these take up an enormous amount of memory, so skip on P1
settime "2023-12-25 01:30:15"
print "Date is: ";date$()
print "Time is: ";time$()
#else
print "Date is: 2023-12-25"
print "Time is: 01:30:15"
#endif

''
'' done
''
doexit(0)
