#
INCLUDE = -I/usr/local/include
CFLAGS = -wiringPi -lwiringPiDev
LDFLAGS = -L/usr/local/lib
LDLIBS = -lwiringPi -lwiringPiDev

cucheck:cucheck.c
	cc cucheck.c -o $@ $(LDFLAGS) $(LDLIBS) 
