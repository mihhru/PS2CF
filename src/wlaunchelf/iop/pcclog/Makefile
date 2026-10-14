IOP_BIN = ../pcclog.irx
IOP_OBJS = pcclog.o imports.o exports.o
IOP_CFLAGS += -I../../include
all: $(IOP_BIN)
clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)
include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
