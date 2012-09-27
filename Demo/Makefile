CC = propeller-elf-gcc
CFLAGS = -O -mfcache
SPIN2CPP = ../spin2cpp

default: hello.elf fibo.elf count.elf

hello.elf: Hello.spin
	$(SPIN2CPP) --elf -o hello.elf $(CFLAGS) Hello.spin

fibo.elf: Fibo.spin FullDuplexSerial.spin
	$(SPIN2CPP) --elf $(CFLAGS) -o fibo.elf Fibo.spin

count.elf: Count.spin FullDuplexSerial.spin
	$(SPIN2CPP) --elf $(CFLAGS) -o count.elf Count.spin


clean:
	rm -f *.cpp *.c *.h *.elf *.binary

