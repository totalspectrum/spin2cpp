CON
  _clkfreq = 80_000_000
  A = 2
  B = 3
  
DAT
  %if A == 1
    %if B == 3
      long $13131313
    %else
      long $11111111
    %endif
  %elseif A == 2
    %if B == 1
      long 0
    %elseif B == 2
      long 2
    %else
      long $23232323
    %endif
    long $aaaaaaaa
  %else
    long $1111111a
  %endif

  long $FFFFFFFF
