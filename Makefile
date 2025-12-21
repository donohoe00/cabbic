# Override 'APP' to build a different application, e.g.:
#
#  make APP=d8741_read_rom
#
APP ?= dummy

DEPS = api.h Makefile
CFLAGS = -Wall -Ilib
LD=cc

OBJS = main.o apps/$(APP).o

ifeq ($(APP), si5351_gen_clock)
OBJS += lib/si5351.o
LD=c++
endif

$(APP): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ -lusb-1.0

apps/%.o: apps/%.c $(DEPS)
	cc -c -I. $(CFLAGS) -o $@ $<

apps/%.o: apps/%.cpp $(DEPS)
	cc -c -I. $(CFLAGS) -o $@ $<

lib/%.o: lib/%.cpp $(DEPS)
	c++ -c -I. $(CFLAGS) -o $@ $<

main.o: main.c $(DEPS)
	cc -c -I. -I/usr/include/libusb-1.0 $(CFLAGS) -o $@ $<

clean:
	rm -f apps/*.o $(OBJS)
