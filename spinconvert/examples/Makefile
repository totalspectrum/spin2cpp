SPIN2CPP = ../../build/spin2cpp

default: hello.binary math.binary led.binary

%.pasm: %.spin
	$(SPIN2CPP) --asm $<

%.binary: %.pasm
	$(SPIN2CPP) --dat --binary $<

%.elf: %.spin
	$(SPIN2CPP) --elf -Os $<

clean:
	rm -f *.elf *.pasm *.binary *.cpp *.c *.h
