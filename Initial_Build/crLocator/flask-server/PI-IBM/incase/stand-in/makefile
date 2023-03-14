ODIR=obj
LDIR=lib
BIN=bin
SRC=src

include src/linux/linux_makefile.mk

JVMTI_INLCUDE = $(JAVA_HOME)/include

AGENT_FLAGS=-I$(JVMTI_INLCUDE)

# -- COMMON PARTS --

COMMON_DIR=common

_COMMON_DEPS = types.h kernel.h events.h fileLogging.h
COMMON_DEPS = $(patsubst %,$(SRC)/$(COMMON_DIR)/%,$(_COMMON_DEPS))

_COMMON_OBJ = utils.o kernel.o fileLogging.o
COMMON_OBJ = $(patsubst %,$(ODIR)/$(COMMON_DIR)/%,$(_COMMON_OBJ))

# -- AGENT PARTS --

AGENT_DIR=agent

_AGENT_DEPS = agent.h
AGENT_DEPS = $(patsubst %,$(SRC)/$(AGENT_DIR)/%,$(_AGENT_DEPS))

_AGENT_OBJ = agent.o
AGENT_OBJ = $(patsubst %,$(ODIR)/$(AGENT_DIR)/%,$(_AGENT_OBJ))

# -- A2N PARTS --

A2N_DIR=a2n

_A2N_DEPS = a2n.h
A2N_DEPS = $(patsubst %,$(SRC)/$(A2N_DIR)/%,$(_A2N_DEPS))

_A2N_OBJ = a2n.o
A2N_OBJ = $(patsubst %,$(ODIR)/$(A2N_DIR)/%,$(_A2N_OBJ))

# -- SCS PARTS --

SCS_DIR=scs

_SCS_DEPS = scs.h
SCS_DEPS = $(patsubst %,$(SRC)/$(SCS_DIR)/%,$(_SCS_DEPS))

_SCS_OBJ = scs.o
SCS_OBJ = $(patsubst %,$(ODIR)/$(SCS_DIR)/%,$(_SCS_OBJ))

# -- TPROF PARTS --

TPROF_DIR=tprof

_TPROF_DEPS = tprof.h
TPROF_DEPS = $(patsubst %,$(SRC)/$(TPROF_DIR)/%,$(_TPROF_DEPS))

_TPROF_OBJ = tprof.o
TPROF_OBJ = $(patsubst %,$(ODIR)/$(TPROF_DIR)/%,$(_TPROF_OBJ))

# -- TARGETS --

.PHONY: clean all libpicommon tprof a2n scs arcflow directories

all : directories libcommon tprof a2n scs

libcommon : $(LDIR)/libcommon.a

tprof : $(BIN)/tprof

a2n : $(BIN)/liba2n.so

scs : $(BIN)/libscs.so

directories : $(ODIR) $(LDIR) $(BIN)

clean :
	$(RM) $(ODIR)
	$(RM) $(BIN)/tprof
	$(RM) $(BIN)/liba2n.so
	$(RM) $(BIN)/libscs.so
	$(RM) $(LDIR)/libcommon.a

$(ODIR) :
	$(MKDIR) $(ODIR)
	$(MKDIR) $(ODIR)/$(KERNEL_DIR)
	$(MKDIR) $(ODIR)/$(TPROF_DIR)
	$(MKDIR) $(ODIR)/$(AGENT_DIR)
	$(MKDIR) $(ODIR)/$(A2N_DIR)
	$(MKDIR) $(ODIR)/$(SCS_DIR)
	$(MKDIR) $(ODIR)/$(COMMON_DIR)

$(BIN) :
	$(MKDIR) $(BIN)

$(LDIR) :
	$(MKDIR) $(LDIR)

# -- OBJECT FILES --
$(ODIR)/$(COMMON_DIR)/%.o: $(SRC)/$(COMMON_DIR)/%.cpp $(COMMON_DEPS) | directories
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/$(TPROF_DIR)/%.o: $(SRC)/$(TPROF_DIR)/%.cpp $(COMMON_DEPS) $(KERNEL_DEPS) $(TPROF_DEPS) | directories
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/$(KERNEL_DIR)/%.o: $(SRC)/$(KERNEL_DIR)/%.c $(COMMON_DEPS) $(KERNEL_DEPS) | directories
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/$(AGENT_DIR)/%.o: $(SRC)/$(AGENT_DIR)/%.cpp $(COMMON_DEPS) $(AGENT_DEPS) | directories
	$(CC) -c -o $@ $< $(CFLAGS) $(AGENT_FLAGS)

$(ODIR)/$(A2N_DIR)/%.o: $(SRC)/$(A2N_DIR)/%.cpp $(COMMON_DEPS) $(AGENT_DEPS) $(A2N_DEPS) | directories
	$(CC) -c -o $@ $< $(CFLAGS) $(AGENT_FLAGS)

$(ODIR)/$(SCS_DIR)/%.o: $(SRC)/$(SCS_DIR)/%.cpp $(COMMON_DEPS) $(AGENT_DEPS) $(SCS_DEPS) | directories
	$(CC) -c -o $@ $< $(CFLAGS) $(AGENT_FLAGS)

# -- LIBS AND BINS --
$(LDIR)/libcommon.a : $(KERNEL_OBJ) $(COMMON_OBJ) $(COMMON_DEPS) $(KERNEL_DEPS)
	$(AR) rcs $@ $^

$(BIN)/liba2n.so : $(AGENT_OBJ) $(A2N_OBJ) $(A2N_DEPS) $(AGENT_DEPS) $(LDIR)/libcommon.a
	$(LNK) -o $@ $^ $(CFLAGS) $(LFLAGS) $(AGENT_FLAGS)

$(BIN)/libscs.so : $(AGENT_OBJ) $(SCS_OBJ) $(SCS_DEPS) $(AGENT_DEPS) $(LDIR)/libcommon.a
	$(LNK) -o $@ $^ $(CFLAGS) $(LFLAGS) $(AGENT_FLAGS)

$(BIN)/tprof : $(TPROF_OBJ) $(TPROF_DEPS) $(LDIR)/libcommon.a
	$(LNK) -o $@ $^ $(CFLAGS)

