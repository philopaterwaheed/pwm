all:
	g++ test.cpp -lX11 -lXcursor -o pwm
	cp pwm /usr/bin/
