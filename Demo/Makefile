CC = propeller-elf-gcc
CFLAGS = -O -mfcache

default: hello.elf fibo.elf count.elf

hello.elf: Hello.spin
	../spin2cpp --elf -o hello.elf $(CFLAGS) Hello.spin

fibo.elf: Fibo.spin FullDuplexSerial.spin
	../spin2cpp --elf $(CFLAGS) -o fibo.elf Fibo.spin

count.elf: Count.spin FullDuplexSerial.spin
	../spin2cpp --elf $(CFLAGS) -o count.elf Count.spin


clean:
	rm -f *.cpp *.h *.elf 
