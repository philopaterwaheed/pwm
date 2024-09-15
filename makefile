all:
	g++ args.cpp fonts.cpp handlers.cpp main.cpp -I /usr/include/freetype2 -lX11 -lXcursor -lfontconfig -lXinerama -lXft  -DDEBUG -g -o pwm
	cp -f pwm /usr/bin/
