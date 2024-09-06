all:
	g++ main.cpp -lX11 -lXcursor -o pwm
	cp pwm /usr/bin/
