'=================================================================================================================================
' NOTICE:  M.I.T Copyright applies.  See end of file for details
'=================================================================================================================================
'
' LIST OF FUNCTIONS:
'
'*__strs_cl			(private class)
'*Bin$(n%)			Returns a binary string representation of N%
'*Chr$(n%)			Returns a string character with ASCII code N%
' CountStr(x$, s$)		Counts the number of occurances of string S in string X
'*Decuns$(n%)			Returns a decimal string representation of N%
' Delete$(t$,o%,c%)		Deletes characters in T$ starting at offset O% and continuing for C% characters
'*Hex$(n%)			Returns a hexadecimal string representation of N%
' Insert$(b$,i$,o%)		Inserts I$ into B$ at offset O% 
' Instr(o%,s$,t$)		Returns the first occurance of T$ in S$ beginning at offset O%. Search direction is left-to-right
' InstrRev(o%,s$,t$		Reverse-acting version of INSTR$(). Search direction is right-to-left
'*Left$(x$,c%)			Returns the leftmost C% chars of X$
' LCase$(x$)			Changes all upper case characters in X$ to lower case
' LPad$(x$,w%,ch$)		Returns a string of W length consisting of X$ padded on the left side with the character CH$
' LTrim$(x$)			Removes leading spaces from X$
'*Mid$(x$,o%,c%)		Returns a substring of X$ starting at offset O% consisting of C% characters
'*Number			(private function)
'*Oct$(n%)			Returns an octal string representation of N%
'*Pfunc				(private function)
' RemoveChar$(x$, s$) 		Removes all occurances of character S in string X
' ReplaceChar$(x$,s$,r$)	Replaces all occurances of character S in string X with character R
' Reverse$(x$)			Reverses the position of the characters in X$ (swaps first for last, etc)
'*Right$(x$,c%)			Returns the rightmost C% characters from X$
' RPad$(x$,w%,ch$)		Returns a string of W length consisting of X$ padded on the right side with the character CH$
' RTrim$(x$)			Removes trailing spaces from X$
' Space$(count%)		Returns a string of CNT length consisting of space characters
'*Str$(n%)			Returns a string representation of N% (ie converts N% to a string)
' String$(cnt, x$)		Returns a string of CNT length consisting of the character X$
' STRInt$(num%)			Integer-only version of STR$() that avoids floating-point math.  About 8x faster than STR$()
' Trim$(x$) 				Removes spaces from the left and right side of string X
' UCase$(x$)				Changes all lower case characters in X$ to upper case
'
' NOTE: "*" denotes original ERSmith code copied in unmodified form from the V5.0.7 FlexProp distribution dated 14-Jan-2021.
' All other code by JRoark, except as noted:

' Changes for FlexProp 5.0.8 release:
'    Count renamed CountStr and made to handle arbitrary string matches
'    Remove$ renamed RemoveChar$ and slightly optimized
'    Replace$ renamed ReplaceChar$ and slightly optimized
'=================================================================================================================================
'
FUNCTION CountStr(x as string, s as string) as integer
'
'	Purpose:	Counts all occurances of the string S in X
'	Errors:		None known
'	Author:		JRoark 16Jan2020 / ersmith 19Jan2021
'	Version:		2.0 of 16Jan2021
'	Requires:	Nothing
'	RefDoc:		N/A		
'	RelSpeed:	Fast	

	dim p as ubyte pointer				' setup p as a pointer to ubytes
  	dim i, j, m, z as integer			' i,j=loop cntr, m=len of input string, z=result
	dim slen as integer				' length of source string
	dim ch as integer				' temp variable

  	m = __builtin_strlen(x)				' get the length of the input string
  	
  	if (m = 0) then 				' trap for zero length input string
  		return 0				' return a count of zero
	end if

	slen = __builtin_strlen(s)
	if slen = 0 then				' trap for zero-length S argument
		return m				' return appropriate number of matches
	end if
	
	z = 0						' initialize count

	slen -= 1					' adjust for starting index of 0
	m -= 1						' ditto
	m -= slen					' only need to check first (m-slen) positions
	if m < 0 then
	    return 0
	end if
	
	' special case fast match for single character
	if slen = 0 then				' searching a single character
	    ch = s(0)					' fetch that character
	    for i = 0 to m				' iterate through input
	        if x(i) = ch then
		  z += 1
		end if
	    next i
	    return z
	end if

	' for longer strings
	for i = 0 to m					' iterate through the input string
	    ch = 1					' assume a match
	    for j = 0 to slen
		if (x(i+j) <> s(j)) then		' look char-by-char for a match in X with S
		    ch = 0				' no match possible
		    exit for   				' break out of the loop
		end if
	    next j
	    z += ch
	next i

	return z					' return count of how many S's were found in X
	
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION Delete$(target$ as string, offset as integer, count as integer) as string
'
'	Purpose:		Deletes everything in TARGET$ beginning at OFFSET and continuing for COUNT characters, and returns the resulting string.
'           	If OFFSET is beyond the end of TARGET$ nothing happens and TARGET$ is returned intact
'					If OFFSET is zero or negative, it is constrained to 1 and execution continues without error
'					If COUNT goes beyond the length of TARGET$ everything from OFFSET to the end of TARGET$ is deleted without error
'					If COUNT is zero or negative, nothing is done and TARGET$ is returned intact without error
'	Errors:		None known
'	Author:		JRoark 16Jan2020
'	Version:		2.0 of 16Jan2021
'	Requires:	LEFT$()
'	RefDoc:		N/A		
'	RelSpeed:	Fast	

  	dim p as ubyte pointer				' setup p as a pointer to ubytes
  	dim i,m,z as integer					' i=loop cntr, m=len of passed string, z=pointer offset

  	m = __builtin_strlen(target$)		' get length of passed string

	if (m < 1) then						' trap for zero-length string
		return 0								' return a null string
	end if
	
	if (offset > m) then 				' trap for offset beyond length of TARGET$
		return target$						' return the original string unmodified
	end if

	if (offset < 1) then 				' trap for offset zero or negative
		offset = 1							' force offset to 1 and continue
	end if

	if (count < 1) then 					' trap for negative or too small of a length
		return target$						' return the original string
	end if
	
	if ((offset + count) > m) then	' trap for where offset+count is larger than the length of TARGET$
		return left$(target$, offset - 1)	' return everything to the left of OFFSET 
	end if

   p = new ubyte(m-count)				' attempt to set pointer P to a new array of ubytes 

   if p then								' if P is non-zero then memory alloc was successful		
  		for i = 0 to offset-2			' step thru the first part of the string
			p(i) = target$(i)				' move char by char from TARGET$ to new string
		next i

		z = i									' save current value of I (we will need it below)

		for i = (offset+count-1) to m-1	' step thru second part of string
			p(z) = target$(i)				' move char by char from TARGET$ to new string
			z += 1							' incr our pointer offset
		next i
		p(z) = 0								' append end-of-string char
		return p								' return the new string
	end if
	
  	return 0									' if here, memory alloc was unsuccessful. return null string
 
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION Insert$(base$ as string, toInsert$ as string, offset as integer) as string
'
'	Purpose:		Inserts TOINSERT$ into BASE$ at the specified OFFSET and returns the resulting string.
'           	If OFFSET is beyond the end of BASE$, then TOINSERT$ is appended to the end of BASE$
'					If OFFSET is zero or negative, TOINSERT$ is pre-pended to the beginning of BASE$
'	Errors:		None known
'	Author:		JRoark 16Jan2021
'	Version:		2.0 of 16Jan2021
'	Requires:	Nothing
'	Notes:		None	
'	RefDoc:		N/A		
'	Speed:		Fast	

  	dim p as ubyte pointer				' setup p as a pointer to ubytes
  	dim b,i,j,k,n,t as integer

  	b = __builtin_strlen(base$)		' get length of BASE$
  	t = __builtin_strlen(toInsert$)	' get length of TOINSERT$

	if (offset < 2) then 				' trap for 1, zero or negative offset argument
		return toInsert$ + base$		' append base$ to toInsert$ and return it
	end if

	if (offset > b) then					' trap for a offset argument greater than length of Base$
		return base$ + toInsert$		' append toInsert$ to the end 
	end if

	if (b < 1) orelse (t < 1) then	' trap for empty BASE$ or TOINSERT$
		return base$
	end if
	
   p = new ubyte(b+t+1)					' attempt to set pointer P to a new array of ubytes 
   
   if p then								' if P is non-zero then memory alloc was successful
  		for i = 0 to offset-2			' step thru the first part of the string
			p(i) = base$(i)				' copy byte by byte from BASE$ to new string
		next i

		n = 0
		for j = i to i + t-1				' step thru our string to be inserted
			p(j) = toInsert$(n)			' copy byte by byte from TOINSERT$ to new string
			n += 1
		next j

		for k = j to (b+t)				' step thru remaining part of base string			
			p(k) = base$(i)				' copy byte by byte from remaining BASE$ to new string
			i += 1							' incr offset
		next k
		p(k) = 0								' mark end-of-string
  		return p								' return the new string
	end if
	
	return 0									' if here then the memory alloc failed, so return an empty string

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION Instr(Offset% as integer, Source$ as string, Target$ as string) as integer
'
'	Purpose:		Returns the position of the first occurance of Target$ in Source$. The search begins at the Offset% character.
'					If the Target$ isn't found in Source$, the function returns zero.
'	Errors:		None known
'	Author:		JRoark 11Sep2019
'	Requires:	MID$()
'	Notes:		None	
'	RefDoc:		N/A		
'	Speed:		Average	

   dim targetSize% as integer
   dim sourceSize% as integer
   dim idx% as integer

   targetSize% = len(Target$)			' get length of TARGET$
   sourceSize% = len(Source$)			' get length of SOURCE$

   if sourceSize% = 0 then				' Trap for zero-length SOURCE$ string.
      return 0								' Return a zero (no-match) if true
   end if

   if targetSize% = 0 then				' Trap for zero-length TARGET$ string. 
      return 0								' Return a zero (no-match) if true
   end if

   if Offset% > sourceSize% then		' Trap for an OFFSET% that exceeds the length of the Source. 
      return 0								' Return a zero (no-match) if true
   end if

   if Offset% < 1 then					' Trap & correction for a missing or negative Offset% 
      Offset% = 1							' Fix it quietly and continue without error
   end if

   for idx% = Offset% to sourceSize%
      if Mid$(Source$, idx%, targetSize%) = Target$ then 
			return idx%  
		end if
   next idx%

   return 0									' If here then no match was found. return zero	

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION InstrRev(Offset% as integer, Source$ as string, Target$ as string) as integer
'
'	Purpose:		Returns the position of the LAST occurance of Target$ in Source$. The search begins at the Offset% character.
'					If the Target$ isn't found in Source$, the function returns zero.
'	Errors:		None known
'	Author:		JRoark 11Sep2019
'	Requires:	MID$()
'	Notes:		None	
'	RefDoc:		N/A		
'	Speed:		Average	

   dim targetSize% as integer
   dim sourceSize% as integer
   dim idx% as integer

   targetSize% = __builtin_strlen(Target$)	' get length of TARGET$
   sourceSize% = __builtin_strlen(Source$)	' get length of SOURCE$

   if sourceSize% = 0 then				' Trap for zero-length SOURCE$ string.
      return 0								' Return a zero (no-match) if true
   end if

   if targetSize% = 0 then				' Trap for zero-length TARGET$ string. 
      return 0								' Return a zero (no-match) if true
   end if

   if Offset% < 1 then					' Trap & correction for a missing or negative Offset% 
      Offset% = 1							' Fix it quietly and continue without error
   end if

   if Offset% > sourceSize% then		' Trap for an OFFSET% that exceeds the length of the Source. 
      return 0								' Return a zero (no-match) if true
   end if

   for idx% =  sourceSize% to Offset% step -1
      if mid$(Source$, idx%, targetSize%) = Target$ then
         return idx%		'match found. return position of first char of match
		end if
   next idx%

   return 0									' If here then no match was found. return zero	

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION LCase$(x as string) as string
'
'	Synopsis:	Turns any upper case characters in X to lower case. 
'					Any non upper case characters are passed thru unaffected.
'	Issues:		None known
'	Author:		JRoark 09Jan2021
'	Requires:	Nothing
'	Notes:		None
'	RefDoc:		N/A
'	Speed:		Fast	

	dim p as ubyte pointer				' setup p as a pointer to ubytes
  	dim i, m as integer					' i=loop cntr, m=len of input string
	
  	m = __builtin_strlen(x)				' get the length of the input string
  	
  	if (m=0) then							' trap for zero length input string
  		return 0								' return null string					
	end if
	
  	p = new ubyte(m+1)					' attempt to set pointer P to a new array of ubytes 

	if p then  								' check if memory alloc was successful
 		for i = 0 to m-1					' iterate through the input string
 			if (x(i) > 64) andalso (x(i) < 91) then	' check if its an upper case char
				p(i) = x(i) + 32			' Yes. its upper case. change it to lower case
			else
				p(i) = x(i)					' No. its either already lower case or it isn't a letter. so just copy it
			end if
		next i
		p(m) = 0								' set end-of-string terminator char
		return p								' return the new string
	end if
	
	return 0									' if here then memory alloc failed. return null string
	
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION LPad$(x as string, width as ulong, pchar as string) as string
'
'	Synopsis:	Right-aligns X into a string of size WIDTH using PCHAR as the padding char on the LEFT.
'					If the length of ST is greater than WIDTH, the function returns only the rightmost WIDTH characters without 
'					any padding, ie, LPad$("Testing", 4, "-") would return "ting"' 
'	Errors:		None known
' 	Author:		JRoark 16Jan2020
' 	Version:		2.0 of 16Jan2021
' 	Requires:	STRING$(), LEFT$()
' 	RefDoc:		N/A		
' 	RelSpeed:	Fast	

  	dim p as ubyte pointer				' setup P as a pointer to ubytes
  	dim i, m, z as uinteger				' i=loop cntr, m=len of input string, z=offset counter
  	
  	m = __builtin_strlen(x)				' get the length of the input string

  	if (m=0) then							' trap for zero-length input string
  		return String$(width,pchar)	' return a string of the proper width completely populated by PCHARs
	end if
	
	if (m > width) then					' trap and fix if input string longer than specified WIDTH
		return Left$(x,width)			' return the leftmost (unpadded) WIDTH chars of X
	end if
	
	if (m=width) then						' speed-up for when input string is same size as WIDTH
		return x								' return X unchanged
	end if
	
  	p = new ubyte(width+1)				' attempt to set pointer P to a new array of ubytes 
  	
	if p then  								' check to see that our memory alloc worked (P<>0 means it worked)
		z = 0									' init Z
  		for i = 0 to width-1 			' step thru the input string char by char
  			if i < (width - m)  then	' 
  				p(i) = pchar(0)			' copy padchars into output string  char by char
  			else
  				p(i) = x(z)					' copy chars from input string into output string char by char
  				z = z + 1					' incr offset
  			end if
		next i
		
		p(width) = 0						' append string terminator char
		return p								' c'ya..
	end if
	
	return 0									' if here then memory alloc failed. Return null string		
  
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION LTrim$(x as string) as string
'
'	Purpose:		Removes leading spaces from X
'	Errors:		None known
' 	Author:   	JRoark 16Jan2020
' 	Version:		2.0 of 16Jan2021
' 	Requires: 	Nothing
' 	RefDoc:		N/A		
' 	RelSpeed:	Fast	

  	dim p as ubyte pointer				' setup p as a pointer to ubytes
  	dim i, m as integer					' i=loop cntr, m=len of input string
	
  	m = __builtin_strlen(x)				' get the length of the input string
  	if (m=0) return 0						' trap for zero-length input string
  	
  	for i = 0 to m-1 						' iterate through the input string
  		if (x(i) <> 32) then				' found a non-space, so copy everything to the right of it and quit
			p = new ubyte(m-i+1)			' attempt to set pointer P to a new array of ubytes 
			if p then						' check if memory alloc was successful
   			bytemove(p, @x(i), m-i)	' BYTEMOVE (DestAddress, SrcAddress, Count )
  				p(m-i) = 0					' terminate the string
				return p						' return the string
			else
				return 0						' if here then memory alloc failed. return null
			end if
		end if
	next i
	
	return x									' if here, the input string was all spaces
  
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION RemoveChar$(x as string, s as string) as string
'
'	Purpose:		Finds all occurances of the character S in X and removes (deletes) them
'					Note that S must be a single char. If multiple chars are provided, only
'					the first character is significant
'	Errors:		None known
'	Author:		JRoark 16Jan2020
'	Version:		2.0 of 16Jan2021
'	Requires:	COUNT()
'	RefDoc:		N/A		
'	RelSpeed:	Fast	

	dim p as ubyte pointer				' setup p as a pointer to ubytes
  	dim i, m, z as integer				' i=loop cntr, m=len of input string, z=offset counter						
	dim removeC as uinteger				' character to remove
	
  	m = __builtin_strlen(x)				' get the length of the input string
  	
  	if (m = 0) then 						' trap for zero length input string
  		return 0								' return a null string
	end if

	if __builtin_strlen(s) = 0 then	' trap for zero-length S argument
		return x								' return X unchanged
	end if

	z = Count(x, s)						' count occurances of target in the source string
												' so we know how long to make our output string
	
  	p = new ubyte(m-z+1)					' attempt to set pointer P to a new array of ubytes 

	removeC = s(0)
	z = 0  									' zero Z for reuse below
  	if p then								' check if memory alloc was successful
  		for i = 0 to m-1					' iterate through the input string
  			if (x(i) = removeC) then		' if the byte is the same as our target byte...
				'skip it						' do nothing (effectively deletes it)
			else								' otherwise...
				p(z) = x(i)					' copy byte to output string 
				z += 1						' incr output string offset
			end if
		next i								
		p(z) = 0								' append string terminator char
	  	return p								' return new string
	end if
	
	return 0									' if here, memory alloc failed. return a null string
	
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION ReplaceChar$(x as string, s as string, r as string) as string
'
' Purpose:	Finds all occurances of character S in X and replaces them with R
'				Both R and S must be a single character. If multiple chars are 
'				supplied, all chars but the first character will be ignored
' Errors:	None known
' Author:   JRoark 16Jan2020
' Version:	2.0 of 16Jan2021; modified slightly by ersmith
' Requires: Nothing
' RefDoc:	N/A		
' RelSpeed:	Fast	


	dim p as ubyte pointer				' setup p as a pointer to ubytes
  	dim i, m as integer				' i=loop cntr, m=len of input string						
	dim origC, newC as uinteger			' original and replacement char
  	m = __builtin_strlen(x)				' get the length of the input string
  	
  	if (m = 0) then 				' trap for zero length input string
  		return 0				' return a null string
	end if

	if __builtin_strlen(s) = 0 then	' trap for zero-length S argument
		return x				' return X unchanged
	end if

	if __builtin_strlen(r) = 0 then	' trap for zero-length R argument
		return x				' return X unchanged
	end if
					
  	p = new ubyte(m+1)				' attempt to set pointer P to a new array of ubytes 

	origC = s(0)
	replaceC = r(0)
	
  	if p then					' check if memory alloc was successful
  		for i = 0 to m-1			' iterate through the input string
  			if (x(i) = origC) then		' is this our target char?
				p(i) = replaceC		' yes. change to new char 
			else
				p(i) = x(i)		' no. dont change. just copy it to output string
			end if
		next i
		p(m) = 0				' append string terminator char
	  	return p				' return new string
	end if
	
	return 0					' if here, memory alloc failed. return a null string
	
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION Reverse$(x as string) as string
'
'	Purpose:		Reverses the order of the characters in X. (Swaps X end-for-end)
'	Issues:   	None known
'	Author:   	JRoark 16Jan2021
'	Requires: 	Nothing
'	Notes:		None
'	RefDoc:		N/A
'	Speed:		Fast	

	dim p as ubyte pointer				' setup p as a pointer to ubytes
	dim i, m, z as uinteger				' i=loop cntr, m=len of input string, z=offset value
  	
	m = __builtin_strlen(x)				' get length of input string
	if (m=0) return 0						' if zero lenght, return a null string

	p = new ubyte(m+1)					' attempt to set pointer P to a new array of ubytes 
	z = m										' copy M to Z (Z=pointer offset)
	
	if p then								' check if memory alloc was successful
		for i = 0 to m						' step through the input string, left to right, halfway, swapping ends as we go
			z -= 1							' decr pointer offset
			p(i) = x(z)						' copy byte by byte
		next i								' next victim...
		p(m) = 0								' append string terminator character
		return p								' return the result
	end if
	
	return 0									' if here then memory alloc failed. Return null string
	
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION RPad$(x as string, width as long, pchar as string) as string
'
'	Purpose:		Left-aligns x into a string of size WIDTH using PCHAR as the padding char on the RIGHT.
'  				If the length of ST is greater than WIDTH, the function returns only the leftmost WIDTH characters without 
'					any padding, ie, RPad$("Testing", 4, 45) would return "Test"
'	Issues:   	None known
'	Author:   	JRoark 16Jan2021
'	Requires: 	String$(), Left$()
'	Notes:		None
'	RefDoc:		N/A
'	Speed:		Fast	

  	dim p as ubyte pointer				' setup P as a pointer to ubytes
	dim i, m as integer					' I=loop cntr, M=len of input string
	
	m = __builtin_strlen(x)				' get the length of the input string

  	if (m=0) then							' trap for zero-length input string
  		return string$(width,pchar)	' return a string of the proper width completely populated by PCHARs
	end if

	if (m > width) then					' trap for input string longer than specified WIDTH
		return left$(x,width)			' return the leftmost (unpadded) WIDTH chars of X
	end if
	
	if (m=width) then						' speed-up for when input string is same size as WIDTH
		return x								' return X unchanged
	end if
	
	p = new ubyte(width+1)				' attempt to set pointer P to a new array of ubytes 
	
	if p then								' check if memory alloc was successful...
		for i = 0 to width-1				' iterate forward through the input string 
  			if i < m then		
  				p(i) = x(i)					' copy from input to output string char by char
			else
				p(i) = pchar(0)			' add-in padchars on right
			end if
		next i
		p(width) = 0						' add end-of-string terminator char
		return p								' return the new string
	end if	
		
	return 0									' if here then memory alloc failed. return null string
  
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION RTrim$(x as string) as string 
'
'	Purpose:		Removes trailing spaces from X
'	Errors:		None known
'	Author:   	JRoark 16Jan2020
'	Version:		2.0 of 16Jan2021
'	Requires: 	Nothing
'	RefDoc:		N/A		
'	RelSpeed:	Fast	

  	dim p as ubyte pointer				' setup P as a pointer to ubytes
  	dim i, m as integer					' I=loop cntr, M=len of input string
	
  	m = __builtin_strlen(x)				' get the length of the input string
  	
  	if (m=0) then							' trap for zero-length input string
  		return 0								' return a null string
	end if
	
  	for i = m-1 to 0 step -1			' iterate backwards through the input string
  		if (x(i) <> 32) then				' found a non-space, so copy everything to the right of it and quit
			p = new ubyte(i+1)			' attempt to set pointer P to a new array of ubytes 
			if p then						' check if the memory alloc was successful
   			bytemove(p, @x(0), i+1)	' BYTEMOVE (DestAddress, SrcAddress, Count )
  				p(i+1) = 0					' terminate the string
				return p						' return the new string
			else
				return 0						' memory alloc failed. return null string
			end if
		end if
	next i
	
	return 0									' if here, the input string was all spaces. return a null string
  
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION Space$(count% as integer) as string
'
'	Purpose:  	Returns a string containing COUNT space (" ", ASCII 32d) characters.
'	Issues:   	None known
'	Author:   	JRoark 07Jan2021
'	Requires: 	Nothing
'	Notes:		Trivial function included for compatability with many older BASIC implementations
'	RefDoc:		N/A
'	Speed:		Fast	

	return String$(count%, " ")
 
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION String$(cnt as integer, x = " " as string) as string
'
'	Purpose:  	Returns a string of CNT length of the character X.
'					X must be a single char. If multiple chars are supplied, the addl chars are ignored
'	Issues:   	None known
'	Author:   	JRoark 16Jan2021
'	Requires: 	Nothing
'	Notes:		Trivial function included for compatability with many older BASIC implementations
'	RefDoc:		N/A
'	Speed:		Fast	

  	dim p as ubyte pointer				' setup P as a pointer to ubytes
	dim m as integer						' M=len of input string
	
	m = __builtin_strlen(x)				' get length of input string
	
	if (m = 0) then						' trap for a zero-length input string
		return 0								' return null string
	end if

	if (cnt <= 0) then					' trap for zero or negative CNT
		return 0								' return null string
	end if
	
	p = new ubyte(cnt+1)					' attempt to set pointer P to a new array of ubytes 
	
	if p then								' check if memory alloc was successful 
   	bytefill(p, x(0), cnt)			' BYTEFILL (DestAddress, char, Count )
  		p(cnt+1) = 0						' terminate the string
		return p								' return the new string
	end if
	
	return 0									' if here, the memory alloc was unsuccessful. return null string

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION STRInt$(number as long) as string
'
' 	Purpose:		A faster (~8x) integer only analog to BASICs native STR$(). This function accepts only integers and thus
'					avoids the overhead of floating-point math. If a single/float is supplied, it will be truncated to an integer.
'					Note that the input is signed giving the return value a range of -2,147,483,648 to 2,147,483,647
'					So "FFFF_FFFFh" is interpreted as -1, not 4,294,967,295.
'	Issues:   	None known
'	Author:   	JRoark 16Jan2021
'	Requires: 	Nothing
'	Notes:		None
'	RefDoc:		N/A
'	Speed:		Fast	

	dim p as ubyte pointer				' setup p as a pointer to ubytes		
	dim divisor, i as long				' i=loop cntr							' 
	dim temp as ulong						' running residual

	p = new ubyte(12)						' set p to a new array of ubytes (the output string, max 12 chars)
	i = 0										' zero the byte pointer

	if (number < 0) then					' is number negative?
		p(i) = 45							' add-in a "-" as first character (ascii 45)
		i += 1								' increment the byte pointer for the next use
		if (number = &h80000000) then
			p(i) = asc("2")						' add-in a "2" as next char (ascii 50)
			i += 1							' increment the byte pointer for the next use
			number += 2_000_000_000		' add 2 million to number
		end if
		number = -number					' negate number
	else if (number = 0) then			' is number a zero?
		p(i) = asc("0")							' yes. force an ASCII zero character into byte string...
		i += 1								' increment the byte pointer for the next use 
		p(i) = 0								' add a zero to indicate end-of-string
		return p								' return the string to the caller
	end if

	divisor = 1_000_000_000				' force divisor to 1 million 
	while (divisor > number)			' while the divisor is bigger than number...
		divisor /= 10						' divide the divisor by 10
	end while

	while (divisor > 0)					' while divisor > 0
		temp = number / divisor			' divide number by divisor and save to temp
		p(i) = temp + asc("0")					' convert tmp to ASCII (add 48) and save to output byte string
		i += 1								' increment the byte pointer for the next use
		number -= (temp * divisor)		' subtract temp*divisor from number
		divisor /= 10						' divide divisor by 10
	end while

	p(i) = 0									' add a zero to indicate end-of-string
	
	return p									' return the string to the caller

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION TRIM$(x as string) as string
'
' 	Purpose:		Removes leading and trailing spaces from X
'	Issues:   	None known
'	Author:   	JRoark 16Jan2021
'	Requires: 	LTRIM$(), RTRIM$()
'	Notes:		None
'	RefDoc:		N/A
'	Speed:		Fast	

	return LTrim$(RTrim$(x))

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION UCase$(x as string) as string
'
'	Purpose:		Turns any lower case characters in X to upper case. 
'					Any non lower case characters are passed thru unaffected.
'	Issues:   	None known
'	Author:   	JRoark 16Jan2021
'	Requires: 	Nothing
'	Notes:		None
'	RefDoc:		N/A
'	Speed:		Fast

	dim p as ubyte pointer				' setup P as a pointer to ubytes
  	dim i, m as integer					' I=loop cntr, M=len of input string						
	
  	m = __builtin_strlen(x)				' get the length of the input string
  	
  	if (m = 0) then 						' trap for zero length input string
  		return 0								' return a null string
	end if
	
  	p = new ubyte(m+1)					' attempt to set pointer P to a new array of ubytes
  	
  	if p then								' check if memory alloc was successful
  		for i = 0 to m-1					' iterate through the input string
  			if (x(i) > 96) andalso (x(i) < 123) then		' test for a lower-case character
				p(i) = x(i) - 32			' Found one. change to upper case
			else
				p(i) = x(i)					' Not lower case, so copy it without changing case
			end if
		next i
		p(m) = 0								' append string terminator char
	  	return p								' return new string
	end if
	
	return 0									' if here, memory alloc failed. return a null string
	
END FUNCTION

'
'=================================================================================================================================
'=================================================================================================================================
'=================================================================================================================================
' 
' The original, unmodified string handling code by ERSmith follows.
' This code is a cut/copy from the original STRINGS.BAS that was
' included in FlexBASIC V5.0.7 dated Jan 14, 2021 
'
'=================================================================================================================================
'=================================================================================================================================
'=================================================================================================================================

' retrieve the leftmost n characters from a string
function left$(x as string, n as integer) as string
  dim p as ubyte pointer
  dim i, m as integer

  if (n <= 0) return ""
  m = __builtin_strlen(x)
  if (m <= n) return x
  p = new ubyte(n+1)
  if (p) then
    bytemove(p, x, n)
    p(n) = 0
  end if
  return p
end function

' retrieve the rightmost n characters from a string
function right$(x as string, n as integer) as string
  dim p as ubyte pointer
  dim i, m as integer

  if (n <= 0) return ""
  m = __builtin_strlen(x)
  if (m <= n) return x
  p = new ubyte(n+1)
  if (p) then
    i = m - n
    bytemove(p, @x(i), n+1)
  end if
  return p
end function

' retrieve the middle substring starting at i and j characters long
function mid$(x as string, i=1, j=9999999) as string
  dim p as ubyte pointer
  dim m, n
  if (j <= 0) return ""
  i = i-1 ' convert from 1 based to 0 based
  m = __builtin_strlen(x)
  if (m < i) return ""

  ' calculate number of chars we will copy
  n = (m-i)
  if (n > j) then
    n = j
  endif
  p = new ubyte(n+1)
  if p then
    bytemove(p, @x(i), n)
    p(n) = 0
  end if
  return p
end function

' convert an integer to a single character string
function chr$(x as integer) as string
  dim p as ubyte pointer
  p = new ubyte(2)
  if (p) then
    p(0) = x
    p(1) = 0
  end if
  return p
end function

class __strs_cl
  dim p as ubyte pointer
  dim i as integer
  function pfunc(c as integer) as integer
    if (i < 16) then
      p(i) = c
      i = i+1
      return 1
    else
      return -1
    end if
  end function
end class

' format a floating point number as a string
function str$(x as single) as string
  dim p as ubyte pointer
  dim i as integer
  dim g as __strs_cl pointer
  p = new ubyte(15)
  i = 0
  if p then
    g = __builtin_alloca(8)
    g(0).p = p
    g(0).i = i
    _fmtfloat(@g(0).pfunc, 0, x, ASC("g"))
  end if
  '' FIXME: should we check here that i is sensible?
  return p
end function

'
' convert a number val in base B to a string with n digits
' if n == 0 then we just use enough digits to fit
'
function number$(val as uinteger, n as uinteger, B as uinteger) as string
  dim s as ubyte ptr
  dim d as uinteger
  dim tmp as uinteger
  dim lasttmp as uinteger
  
  if n = 0 then
     ' figure out how many digits we need
     n = 1
     tmp = B
     lasttmp = 1
     ' we have to watch out for overflow in very large
     ' numbers; if tmp wraps around (so tmp < last tmp)
     ' then stop
     while tmp <= val and lasttmp < tmp
       lasttmp = tmp
       tmp = tmp * B
       n = n + 1
     end while
  end if
  ' for 32 bit numbers we'll never need more than 32 digits
  if n > 32 then
    n = 32
  endif
  s = new ubyte(n+1)
  s[n] = 0
  while n > 0
    n -= 1
    d = val mod B
    val = val / B
    if d < 10 then
      d = d + ASC("0")
    else
      d = (d - 10) + ASC("A")
    endif
    s[n] = d
  end while
  return s
end function

function hex$(v as uinteger, n=0 as uinteger)
  return number$(v, n, 16)
end function

function bin$(v as uinteger, n=0 as uinteger)
  return number$(v, n, 2)
end function

function decuns$(v as uinteger, n=0 as uinteger)
  return number$(v, n, 10)
end function

function oct$(v as uinteger, n=0 as uinteger)
  return number$(v, n, 8)
end function

'
'=================================================================================================================================
'
' M.I.T LICENSE 
'
' Copyright (c) 2015-2021 by ERSmith & JRoark
' 
' Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
' documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
' the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and 
' to permit persons to whom the Software is furnished to do so, subject to the following conditions:
'
'     1). The above copyright notice and this permission notice shall be included in all copies or 
'     substantial portions of the Software.
' 
' THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED 
' TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
' THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF 
' CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
' DEALINGS IN THE SOFTWARE.
'
'=================================================================================================================================
'
' (OPEN SOURCE INITIATIVE APPROVED)
'
'
