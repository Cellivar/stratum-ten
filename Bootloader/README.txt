
Specially modified bootloader for Arduino Mega2560; required to run
the Mega with a 10MHz system clock. The Arduino IDE's boards.txt file
must also be modified since the bootloader runs at 28,800 baud instead
of the normal 115,200.

Use WinAvr to build this with this make command:

	make mega2560_10MHz


A pre-built hex file is also included: stk500boot_v2_mega2560.hex

See PDF report "StratumTen.pdf" for more information.

