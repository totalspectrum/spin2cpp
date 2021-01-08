'
' ==============================================================================================================================
' StringLIBP2.bas - An include file that adds-in additional string handling functions.  For FlexBASIC on Propeller V2 
' ==============================================================================================================================
'
' ==============================================================================================================================
' NAME:				StringLibP2.bas
' PURPOSE:			Adds many "missing" FlexBASIC string handling functions
' AUTHOR:  			JRoark (JMH)
' REVISION:			1.0x (beta) 15Nov2019
' COPYRIGHT:  		2018-2020 by JRoark 
' RESTRICTIONS: MIT Licensed
'
' LANGUAGE:			FlexBASIC
' REQUIRES:			Nothing. 	
' TARGET:			Parallax Propeller & P2-Rev B silicon (see www.parallax.com)
'						FLEXGui Version 4.0.3 or later (see http://www.github.com/totalspectrum)
' ==============================================================================================================================
'
' DESCRIPTION OF FUNCTIONS/SUBS
'
'	Delete$(t$,o%,l%)		Deletes everything in T$ beginning at offset O% and continuing for length L% characters
'	Insert$(x$, y$, p%)	Inserts Y$ into X$ at position P and returns the resulting string
'	Instr(off, x$, y$)			Searches for the first occurance of Y$ in X$ and returns the offset where the match is found. 
'	InstrRev(off, x$, y$)		Like INSTR() but searches backward instead of forward
'	LCase$(x$)				Converts X$ to lower case.
'	LPad$(x$, w, ch$)		LEFT-pads X$ to the width specified by W by adding CH$ as padding on the RIGHT
'	LTrim$(x$)				Removes leading spaces from X$
'	Reverse$(x$)			Swaps the characters in X$ end-to-end.
'	RPad$(x$, w, ch$)		RIGHT-pads X$ to the width specified by W by adding CH$ as padding on the LEFT
'	RTrim$(x$)				Removes trailing spaces from X$
'	Space$(i%)				Returns a string composed of I% spaces (ASCII char 32 decimal)
'	String$(i%, x$)		Returns a string composed of I% iterations of X$
'	Trim$(x$)				Removes leading and trailing spaces from X$
'	UCase$(x$)				Converts X$ to upper case
'
'===========================================================================================
'
' But we still are missing the following common BASIC functions:
'
' --- String Stuff ---
'
' CVD()		Converts a 64-bit integer or 8-byte string to a double-precision value (NOT SUPPORTED YET)
' CVI()		Converts a floating-point number or string to an integer variable using a binary copy
' CVL()		Converts a single-precision floating-point number or four-byte string to an integer (long) variable
' CVS()		Converts a 32-bit integer or 4-byte string to a single-precision variable
' FORMAT$()	Formats a number in a specified format
' LSET()	 	Left-justifies a string (LPAD implemented as a subroutine)
' MKD$()	 	Does a binary copy from a double variable to a string, setting its length to 8 bytes (NOT POSSIBLE ON PROP1)
' MKI$()	 	Does a binary copy from an integer variable to a string of the same length as the size of the input variable
' MKL$()	 	Does a binary copy from a long variable to a string, setting its length to 4 bytes	
' MKS$()	 	Does a binary copy from a single variable to a string, setting its length to 4 bytes
' RSET()	 	Right-justifies a string (RPAD implemented as a subroutine)
'
'
'=================================================================================================================================
'
FUNCTION Delete$(target$ as string, offset as integer, count as integer) as string

' Purpose:  Deletes everything in TARGET$ beginning at OFFSET and continuing for COUNT character, and returns the resulting string.
'           If OFFSET is beyond the end of TARGET$, then nothing happens and TARGET$ is returned intact
'				If OFFSET is zero or negative, it is constrained to 1
'				If COUNT goes beyond the length of TARGET$, then everything from OFFSET to the end of TARGET$ is deleted
'				If COUNT is zero or negative, nothing happens and TARGET$ is returned intact
' Errors:	None known
' Example:  Print Delete$("Happy 25th Birthday John!", 7, 4) would output "Happy Birthday John!"
' Author:   JRoark 04Jan2020
' Requires: None.
' MemberOf:	StringLibP2.bas
' RefDoc:	N/A		
' Speed:		Average	

	dim leftChunk$ as string
	dim rightChunk$ as string

	if (offset > len(target$)) then 
		' trap for offset beyond length of target$
		' just return target$
		return target$
	end if

	if (offset < 1) then 
		' trap for offset zero or negative
		' force offset to 1
		offset = 1
	end if

	if (count < 1) then 
		' trap for too small of a length
		' just return the original string
		return target$
	end if
	
	if (offset + count > len(target$)) then
		' trap for where offset+count is larger than the length of target$
		' return everything to the left of offset 
		return left$(target$, offset -1)
	end if

	leftChunk$ = left$(target$, offset - 1)
	rightChunk$ = mid$(target$, offset + count, 999999)

	return (leftChunk$ + rightChunk$)
 
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION Insert$(base$ as string, toInsert$ as string, offset as integer) as string

' Purpose:  Inserts TOINSERT$ into BASE$ at the specified OFFSET and returns the resulting string.
'           If OFFSET is beyond the end of BASE$, then TOINSERT$ is appended to the end of BASE$
'				If OFFSET is zero or negative, TOINSERT$ is pre-pended to the beginning of BASE$
' Errors:	None known
' Example:  Print Insert$("Happy Birthday John!", " 25th", 6) would output "Happy 25th Birthday John!"
' Author:   JRoark 01Oct2019
' Requires: None.
' MemberOf:	StringLib.bas
' Notes:		None	
' RefDoc:	N/A		
' Speed:		Average	

	dim leftChunk$ as string
	dim rightChunk$ as string

	if (offset < 2) then 
		' trap for 1, zero or negative offset argument
		' append base$ to toInsert$ and return it
		return toInsert$ + base$
	end if

	if (offset > len(base$)) then
		' trap for a offset argument greater than length of Base$
		' append toInsert$ to the end 
		return base$ + toInsert$
	end if

	leftChunk$ = left$(base$, offset - 1)
	rightChunk$ = mid$(base$, offset, len(base$) - (offset-1))

	return (leftChunk$ + toInsert$ + rightChunk$)
 
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION _Instr(Offset% as integer, Source$ as string, Target$ as string) as integer

' Purpose:  Returns the position of the first occurance of Target$ in Source$. The search begins at the Offset% character.
'           If the Target$ isn't found in Source$, the function returns zero.
' Errors:	None known
' Example:  Print Instr(1, "I LIKE BEER", "BEE") would output "8" (ie, the match occured starting at the 8th character in Source$)
' Author:   JRoark 11Sep2019
' Requires: None.
' MemberOf:	StringLibP2.bas
' Notes:		None	
' RefDoc:	N/A		
' Speed:		Average	


   dim targetSize% as integer
   dim sourceSize% as integer
   dim idx% as integer

   targetSize% = len(Target$)
   sourceSize% = len(Source$)

' Check for zero-length Source$ string. Return a zero (no-match) if true
   if sourceSize% = 0 then
      return 0
   end if

' Check for zero-length Target$ string. Return a zero (no-match) if true
   if targetSize% = 0 then
      return 0
   end if

' Check for an Offset% that exceeds the length of the Source. 
   if Offset% > sourceSize% then
      return 0
   end if

' Check for a missing or negative Offset% and correct it
   if Offset% < 1 then
      Offset% = 1
   end if

   for idx% = Offset% to sourceSize%
      if Mid$(Source$, idx%, targetSize%) = Target$ then 
			return idx%  
		end if
   next idx%

   return 0	'if here then no match

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION _InstrRev(Offset% as integer, Source$ as string, Target$ as string) as integer

' Purpose:  Returns the position of the LAST occurance of Target$ in Source$. The search begins at the Offset% character.
'				If the Target$ isn't found in Source$, the function returns zero.
' Errors:	None known
' Example:  Print Instr(1, "I LIKE BEER BEER BEER", "BEE") would output "18" (ie, the last match occured starting at the 18th character in Source$)
' Author:   JRoark 11Sep2019
' Requires: None.
' MemberOf:	StringLibP2.bas
' Notes:		None	
' RefDoc:	N/A		
' Speed:		Average	


   dim targetSize% as integer
   dim sourceSize% as integer
   dim idx% as integer

   targetSize% = __builtin_strlen(Target$)
   sourceSize% = __builtin_strlen(Source$)

' Check for zero-length Source$ string. Return a zero (no-match) if true
   if sourceSize% = 0 then
      return 0
  	end if

' Check for zero-length Target$ string. Return a zero (no-match) if true
   if targetSize% = 0 then
      return 0
   end if

' Check for a missing or negative Offset% and correct it
   if Offset% < 1 then
      Offset% = 1
   end if

' Check for an Offset% that exceeds the length of the Source. 
   if Offset% > sourceSize% then
      return 0
   end if

   for idx% =  sourceSize% to Offset% step -1
      if mid$(Source$, idx%, targetSize%) = Target$ then
         return idx%		'match found. return position of first char of match
		end if
   next idx%

   return 0		'no match found

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION LCase$(theString$ as string) as string

' Purpose:  Converts all of the characters in theString$ to LOWER case
' Errors:	None known
' Example:  Print LCase$("aBcDeF") would output "abcdef"
' Author:   JRoark 11Sep2019
' Requires: None.
' MemberOf:	StringLibP2.bas
' Notes:		None	
' RefDoc:	N/A		
' Speed:		Average	

   var idx% = 0
   var ch% = 0
	var strSize% = 0
   var out$ = ""

	strSize% = len(theString$)

' Check for a zero-length string. If zero-length, abort
   if strSize% = 0 then
      return ""
   end if

' Step thru each character in the string. If its upper case, convert it to lower case
   for idx% = 1 to strSize%
      ch% = asc(mid$(theString$, idx%, 1))
      if (ch% > 64) and (ch% < 91) then
         'it is a upper case character. convert it to lower case by adding 32
         ch% = ch% + 32
      end if
      out$ = out$ + chr$(ch%)
   next idx%

   return out$

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION LPad$(theString$ as string, width% as integer, alignChar$ as string)

' Purpose:  Right-aligns theString$ into a string whose width is equal to width% using alignChar$ as padding on the left.
'           alignChar$ can be a single character or a multi-char string. If theString$ supplied is longer than width%, the function 
'           returns only the rightmost width% characters without any padding, ie, LPad$("Testing", 4, "-") would return "ting"
' Errors:	None known
' Example:  Print LPad$("Test", 8, "-") would print "----Test"
' Author:   JRoark 11Sep2019
' Requires: StringLibP2: String$()
' MemberOf:	StringLibP2.bas
' Notes:		None	
' RefDoc:	N/A		
' Speed:		Average	

	var out$ = ""
	var strSize% = 0

	strSize% = len(theString$)

' Check theString$ for a zero-length string. Abort if zero length
   if strSize% = 0 then
      return ""
   end if

' Check alignChar$ for a zero-length string. Abort if zero length
   if len(alignChar$) = 0 then
      return ""
   end if

' Check that width% is greater than zero. Abort if less than 1
   if width% < 1 then
      return ""
   end if

 'Check if theString$ is longer than width%. If so, just return the rightmost width% chars
   if (strSize% >= width%) then
      return right$(theString$, width%)
   end if 

'	format the output string but appending padding to the left side of theString$ 
' 	and chopping the resulting rightmost chars of the string to a length of width%.

 	out$ = String$(width%, alignChar$) + theString$

 	return right$(out$, width%)

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION LTrim$(theString$ as string) as string

' Purpose:  Removes leading spaces from theString$
' Errors:	None known
' Example:  Print LTrim$("   TEST   ") would output "TEST   " (spaces on left side of text are removed)
' Author:   JRoark 11Sep2019
' Requires: None
' MemberOf:	StringLibP2.bas
' Notes:		None	
' RefDoc:	N/A		
' Speed:		Average	

   var idx% = 0
	var strSize% = 0

	strSize% = len(theString$)

' Check for a zero-length input string
   if strSize% = 0 then
      return ""
   end if

' Step thru each character in the string. If its a space, toss it until we come to a non-space char

   for idx% = 1 to strSize%
      if asc(mid$(theString$, idx%, 1)) <> 32 then
         'Not a space. Append the entirety of the remaining input string to the output string
         return mid$(theString$, idx%, 999)
      end if
   next idx%

   return theString$		'if here, no spaces were found

END FUNCTION
'=================================================================================================================================
'
FUNCTION Reverse$(theString$ as string) as string

' Purpose:  Reverses the order of all characters in theString$ and returns it to the caller
' Errors:   None known
' Example:  Print Reverse$("ABCDEF") would output "FEDCBA"
' Author:   JRoark 11Sep2019
' Requires: None
' MemberOf:	StringLib.bas
' Notes:		None
' RefDoc:	N/A
' Speed:		Average.	

   var idx% = 0
	var strSize% = 0
   var out$ = ""

	strSize% = len(theString$)

' Check for a zero-length string. If found, return empty string
   if strSize% = 0 then
      return ""
   end if

' Step thru each character in the string starting at the end and moving to the beginning. 
' Copy each char in the reverse order into the output string

   for idx% = strSize% to 1 step -1
      out$ = out$ + mid$(theString$, idx%, 1)
   next idx%

   return out$

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION RPad$(theString$ as string, width% as integer, alignChar$ as string)

' Purpose:  LEFT-aligns THESTRING$ into a string whose width is equal to WIDTH% using alignChar$ 
'				as padding on the right. alignChar$ can be a single character or a string of any length. 
'				If theString$ is longer than WIDTH%, function returns only the leftmost WIDTH% characters without padding
' Errors:   None known
' Example: 	Print RPad$("Test", 8, "-") would print "TEST----"
' Author:   JRoark 11Sep2019
' Requires: StringLibP2: String$()
' MemberOf:	StringLibP2.bas
' Notes:		None
' RefDoc:	N/A
' Speed:		Average.	

	var out$ =""
	var strSize% = 0

	strSize% = len(theString$)

'	Check theString$ for a zero-length string. Abort if zero length
   if strSize% = 0 then
      return ""
   end if

'	Check alignChar$ for a zero-length string. Abort if zero length
   if len(alignChar$) = 0 then
      return ""
   end if

'	Check that width% is greater than zero. Abort if less than 1
   if width% < 1 then
      return ""
   end if

'	Check if theString$ is longer than width%. If so, just return the leftmost width% chars
   if strSize% >= width% then
   	out$ = left$(theString$, width%)
   	return out$
   end if 

'	Format the output string by appending padding to the right side of theString$ 
' 	and chopping the resulting string to a length of width%.

	out$ = theString$ + string$(width%, alignChar$)
	out$ = left$(out$, width%)

 	return out$

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION RTrim$(theString$ as string) as string

' Purpose:  Removes trailing spaces from theString$
' Errors:   None known
' Example:  Print RTrim$("   TEST   ") would output "   TEST" (spaces on right side of text are removed)
' Author:   JRoark 11Sep2019
' Requires: None
' MemberOf:	StringLibP2.bas
' Notes:		None
' RefDoc:	N/A
' Speed:		Average.	

   var idx% = 0
	var strSize% = 0

	strSize% = len(theString$)

' Check for a zero-length string
   if strSize% = 0 then
      return ""
   end if

' Step thru each character in the string in the reverse directon. 
' If its a space, toss it until we come to a non-space character

   for idx% = strSize% to 1 step -1
      if asc(mid$(theString$, idx%, 1)) <> 32 then
         'not a space. append the entirety of the remaining input string to the output string
         return left$(theString$, idx%)
      end if
   next idx%

   return theString$

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION Space$(count% as integer) as string

' Purpose:  Returns a string containing COUNT space characters.
' Errors:   None known
' Example:  Print Space$(8) would return "        " (ie, eight spaces)
' Author:   JRoark 11Sep2019
' Requires: StringLibP2: String$()
' MemberOf:	StringLibP2.bas
' Notes:		Trivial function included for compatability with "standard" BASIC implementations
' RefDoc:	N/A
' Speed:		Average.	

	return String$(count%, " ")
 
END FUNCTION
'
'=================================================================================================================================
'
FUNCTION String$(count% as integer, char$ as string) as string
'
' Purpose:  Returns a string containing COUNT iterations of the string CHAR.
' Errors:   None known
' Example:  Print String$(8, "X") would output "XXXXXXXX"
' Author:   JRoark 117Nov2019
' Requires: Nothing
' MemberOf:	StringLibP2.bas
' Notes:		None
' RefDoc:	N/A
' Speed:		Weapons grade.	

	dim xptr as ubyte pointer

' Check for a zero-length CHAR string or COUNT=0. If found, abort
   if (len(char$) = 0) or (count% < 1) then
      return ""
   end if

	xptr = new ubyte(count% + 1)

	if (xptr) then
		bytefill(xptr, asc(char$), count%)
		xptr(count%) = 0
	end if

	return xptr

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION StrXL$(theNum as long) as string
'
' Purpose:  Mnemonic: "String, eXtended, for LONG variable types".
' 				This is a replacement for FlexBASIC's STR$ function that is designed to be used with signed LONGs. 
'  			It differs from the standard STR$ function in that it does NOT convert very large numbers into 
' 				an exponent format.  Instead, it prints the entire number as a string of digits.  This can be useful
'  			when you are trying to sort-out a precision problem or presenting large numbers to a user.
'				See the companion function STRXUL$.
'  			Note: 
' Errors:   None known
' Example:  Print StrXL$(&H1234_5678) would output "12345678"
' Author:   JRoark 117Nov2019
' Requires: StringLibP2: StrXUL$()
' MemberOf:	StringLibP2.bas
' Notes:		This is just a wrapper that handles the sign and then uses STRUXL$ to do the heavy lifting.
' RefDoc:	N/A
' Speed:		Average.	

	if theNum < 0 then
		'its a negative number
		theNum = ABS(theNum)									'get the abs value
		return "-" + StrXUL$(cast(ULONG, theNum))		'feed this to StrUL$() and manually add back the minus sign
	else
		'its a positive number.
		return StrXUL$(cast(ULONG, theNum))				'just pass it along to STRXUL$
	end if

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION StrXUL$(theNum as ulong) as string
'
' Purpose:  Mnemonic: "String, eXtended, for LONG variable types".
' 				This is a replacement for FlexBASIC's STR$ function that is designed to be used with signed LONGs. 
'  			It differs from the standard STR$ function in that it does NOT convert very large numbers into 
' 				an exponent format.  Instead, it prints the entire number as a string of digits.  This can be useful
'  			when you are trying to sort-out a precision problem or presenting large numbers to a user.
'				See the companion function STRXL$.
'  			Note: 
' Errors:   None known
' Example:  Print StrXUL$(&H1234_5678) would output "12345678"
' Author:   JRoark 17Nov2019
' Requires: MathLibP2: POW()  <- POW() may be obsolete as of FlexBASIC 4.1.0Beta2. Need to check!
' MemberOf:	StringLibP2.bas
' Notes:		This is just a wrapper that handles the sign and then uses STRUXL$ to do the heavy lifting.
' RefDoc:	N/A
' Speed:		Average.	

	dim idx as long					' loop counter, index to digit position		
	dim tmpNum as ulong				' accumulates decreasing total residual with each pass thru the loop
	dim theDigit as long				' holds a single digit (0-9) that our counting loop produces
	dim out$ as string				' output string gets built here
	dim divisor as ulong				' holds our rolling divisor
	dim nzflag as long				' set to 1 after we get our first non-zero digit (suppresses leading zeros)

	out$ = ""							' clear OUT$
	tmpNum = theNum					' create a local working copy of input param THENUM
	nzflag = 0							' reset our non-zero flag

	for idx = 9 to 1 step -1
		divisor = POW(10, idx)
		if (divisor <= tmpNum) then
			nzflag = 1
			theDigit = (tmpNum / divisor)
			out$ = out$ + str$(theDigit)
			tmpNum = tmpNum - (divisor * theDigit)
		else
			if nzflag = 1 then		' once we have found any non-zero char, zeros become significant ! (they are not leading zeros)
				out$ = out$ + "0"		' ... so we have to manually add zeros when the divisor <= tmpnum
			end if
		end if
	next idx

	out$ = out$ + str$(tmpNum) 	' catch the last digit

 	return out$							' return the text representation of the number as a string

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION Trim$(theString$ as string) as string

' Purpose:  removes leading and trailing spaces from theString$
' Errors:	None known
' Example:  Print Trim$("   TEST   ") would output "TEST" (no spaces on either side)
' Author:   JRoark 11Sep2019
' Requires: None
' MemberOf:	StringLibP2.bas
' Notes:		Trivial function included for compatability with "standard" BASIC implementations
' RefDoc:	N/A
' Speed:		Average.	

' Check for zero-length string
   if len(theString$) = 0 then
      return ""
   end if

   return ltrim$(rtrim$(theString$))

END FUNCTION
'
'=================================================================================================================================
'
FUNCTION UCase$(theString$ as string) as string

' Purpose:  Converts all of the characters in theString$ to UPPER case
' Errors:	None known
' Example:  Print uCase$("aBcDeF") would output "ABCDEF"
' Author:   JRoark 11Sep2019
' Requires: None
' MemberOf:	StringLibP2.bas
' Notes:		None
' RefDoc:	N/A
' Speed:		Average.	

   var idx% = 0
   var ch% = 0
	var strSize% = 0
   var out$ = ""

	strSize% = len(theString$)

' Check for a zero-length string. If found, abort
   if strSize% = 0 then
      return ""
   end if

' Step thru each character in the string. If its lower case, convert it to upper case
   for idx% = 1 to strSize%
      ch% = asc(mid$(theString$, idx%, 1))
      if (ch% > 96) and (ch% < 123) then
         'it is a lower case character. convert it to upper case by subtracting 32
         ch% = ch% - 32
      end if
      out$ = out$ + chr$(ch%)
   next idx%

   return out$

END FUNCTION


