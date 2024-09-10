all:
	g++ fonts.cpp handlers.cpp main.cpp -I /usr/include/freetype2 -lX11 -lXcursor -lfontconfig -lXft  -o pwm
	cp -f pwm /usr/bin/
