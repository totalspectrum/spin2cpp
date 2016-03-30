SPIN2CPP = ../../build/spin2cpp

default: hello.binary math.binary led.binary fibo.binary abort.binary

%.pasm: %.spin
	$(SPIN2CPP) -L../../Lib -g --asm $<

%.binary: %.spin
	$(SPIN2CPP) -L../../Lib -g --asm --binary $<

%.elf: %.spin
	$(SPIN2CPP) --elf -Os $<

clean:
	rm -f *.elf *.pasm *.binary *.cpp *.c *.h
