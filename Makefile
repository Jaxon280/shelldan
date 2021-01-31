COMPILER = /usr/bin/gcc
OPTION = -Wall

SRCS = $(wildcard *.c)

TARGET = shellman

$(TARGET): $(SRCS)
	$(COMPILER) $(OPTION) -g $(SRCS) -o $(TARGET)
