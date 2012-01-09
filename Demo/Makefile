CC = propeller-elf-gcc
CFLAGS = -O -mfcache

default: hello.elf fibo.elf

hello.elf: Hello.spin FullDuplexSerial.spin
	../spin2cpp -main Hello.spin
	$(CC) $(CFLAGS) -o hello.elf Hello.cpp FullDuplexSerial.cpp -s

fibo.elf: Fibo.spin FullDuplexSerial.spin
	../spin2cpp -main Fibo.spin
	$(CC) $(CFLAGS) -o fibo.elf Fibo.cpp FullDuplexSerial.cpp -s


clean:
	rm -f *.cpp *.h *.elf 
