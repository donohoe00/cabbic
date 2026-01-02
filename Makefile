# Override 'APP' to build a different application, e.g.:
#
#  make APP=d8741_read_rom
#
APP ?= dummy

DEPS = inc/cabbic/api.h Makefile
CFLAGS = -Wall -Ilib -Iinc
LD=cc

VPATH=apps/$(APP)

OBJS = main.o apps/$(APP)/$(APP).o

include apps/$(APP)/app.mk

$(APP): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ -lusb-1.0

apps/$(APP)/%.o: apps/$(APP)/%.c $(DEPS)
	cc -c -I. $(CFLAGS) -o $@ $<

apps/$(APP)/%.o: apps/$(APP)/%.cpp $(DEPS)
	cc -c -I. $(CFLAGS) -o $@ $<

lib/%.o: lib/%.cpp $(DEPS)
	c++ -c -I. $(CFLAGS) -o $@ $<

main.o: main.c $(DEPS)
	cc -c -I. -I/usr/include/libusb-1.0 $(CFLAGS) -o $@ $<

clean:
	rm -f apps/$(APP)/*.o $(OBJS)
