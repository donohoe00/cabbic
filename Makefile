# Overrride 'APP' to build a different application, e.g.:
#
#  make APP=d8741_read_rom
#
APP ?= dummy

DEPS = api.h Makefile
CFLAGS = -Wall

OBJS = main.o apps/$(APP).o

$(APP): $(OBJS)
	cc $(LDFLAGS) -o $@ $^ -lusb-1.0

apps/%.o: apps/%.c $(DEPS)
	cc -c -I. $(CFLAGS) -o $@ $<

main.o: main.c $(DEPS)
	cc -c -I. -I/usr/include/libusb-1.0 $(CFLAGS) -o $@ $<

clean:
	rm -f apps/*.o $(OBJS)
