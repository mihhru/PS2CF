IOP_BIN = ../pcdev9.irx
IOP_OBJS = ps2dev9.o pcdiag_cis.o pcdiag_cf.o pcdiag_ata.o pcdiag_fat.o imports.o exports.o
IOP_CFLAGS += -I../pcclog
IOP_CFLAGS += -DDEV9_PSX_SUPPORT=1 -DDEV9_GAMESTAR_WORKAROUND=1
all: $(IOP_BIN)
clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)
include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
