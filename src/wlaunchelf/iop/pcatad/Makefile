IOP_BIN = ../pcatad.irx
IOP_OBJS = ps2atad.o imports.o exports.o
IOP_CFLAGS += -I../pcclog
IOP_CFLAGS += -DATA_GAMESTAR_WORKAROUND=1
all: $(IOP_BIN)
clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)
include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
