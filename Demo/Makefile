default: hello.elf fibo.elf

hello.elf: Hello.spin FullDuplexSerial.spin
	../spin2cpp -main Hello.spin
	propeller-elf-gcc -o hello.elf -O Hello.cpp FullDuplexSerial.cpp -s

fibo.elf: Fibo.spin FullDuplexSerial.spin
	../spin2cpp -main Fibo.spin
	propeller-elf-gcc -o fibo.elf -O Fibo.cpp FullDuplexSerial.cpp -s


clean:
	rm -f *.cpp *.h *.elf 
