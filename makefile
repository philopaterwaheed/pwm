# Compiler and Flags
CXX := g++
CXXFLAGS := -I /usr/include/freetype2
LDFLAGS := -lX11 -lXcursor -lfontconfig -lXinerama -lXft
SRCS := arrange.cpp args.cpp fonts.cpp handlers.cpp main.cpp
OBJS := $(SRCS:.cpp=.o)
TARGET := pwm

PREFIX := /usr/bin

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) *.d

install: $(TARGET)
	install -Dm755 $(TARGET) $(PREFIX)/$(TARGET)

%.d: %.cpp
	@$(CXX) $(CXXFLAGS) -MM $< > $@

-include $(SRCS:.cpp=.d)
