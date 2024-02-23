all:
	g++ main.cpp -lX11 -o pwm
	cp pwm /bin/
