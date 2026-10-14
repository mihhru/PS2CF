IOP_BIN = ../cf_bdmfs.irx
IOP_OBJS = main.o fs_driver.o diskio.o ff.o ffunicode.o ffsystem.o imports.o
IOP_LIBS = -lgcc
IOP_CFLAGS += -I.
all: $(IOP_BIN)
clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)
include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal
$(IOP_OBJS): $(wildcard *.h) Makefile
