CC=gcc#i686-linux-gnu-gcc#gcc
SHARED_FLAGS = -std=c99 -ffreestanding -fno-stack-protector -g \
               -I. -mno-red-zone -fno-pie # -mcmodel=large -fno-pie
CFLAGS = $(SHARED_FLAGS)
ASFLAGS = $(SHARED_FLAGS) -Wa,--divide

OBJS := boot.o
#OBJS += trapz.o
#OBJS += trap.o
OBJS += kernel.o
#OBJS += memory.o
#OBJS += print.o
#OBJS += debug.o
#OBJS += lib.o
#OBJS += trapz.o



DFILES = $(patsubst %.o,%.d,$(OBJS))

all: kernel

kernel: $(OBJS) link.lds Makefile
	$(CC) $(CFLAGS) -nostdlib -no-pie -Wl,--build-id=none -T link.lds -o $@ $(OBJS)
#-z max-page-size=0x1000
clean:
	find -name "*~" -delete
	rm -rf $(OBJS) $(DFILES) kernel

$(OBJS): Makefile
-include $(DFILES)
