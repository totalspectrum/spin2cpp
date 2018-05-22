
PUB getErr(n)
    return @@errStringAddresses[n]

DAT
foo
	long 0
	word 1

errStringAddresses
	word @errString1
	word @errString2
	word @errString3
	word errString1
	word errString2
	word errString3
	word 0

errString1   byte "Error 1", 0
errString2   byte "Error 2", 0
errString3   byte "Error 3", 0

