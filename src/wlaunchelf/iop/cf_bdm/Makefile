IOP_BIN = ../cf_bdm.irx
IOP_OBJS = main.o bdm.o part_driver.o part_driver_mbr.o part_driver_gpt.o imports.o exports.o
IOP_LIBS = -L$(PS2SDK)/iop/lib -lbdm -lgcc
IOP_CFLAGS += -I.
all: $(IOP_BIN)
clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)
include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal
$(IOP_OBJS): $(wildcard *.h) Makefile
