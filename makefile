all:
	g++ main.cpp -lX11 -lXcursor -o pwm
	cp -f pwm /usr/bin/
