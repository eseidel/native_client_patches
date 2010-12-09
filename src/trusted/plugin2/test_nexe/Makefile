.PHONY: all clean

OBJECTS = hello_ppapi.o \
          ppapi_runtime.o \

LDFLAGS = -lgoogle_nacl_imc \
          -lgoogle_nacl_npruntime \
          -lpthread \
          -lsrpc \
          -lnosys \

PROJECT_ROOT = .
PROJECT_NAME = hello_ppapi

all: $(PROJECT_NAME).nexe

include common.mk

$(PROJECT_NAME).nexe: $(OBJECTS) $(TEST_OBJECTS)
	$(CPP) $^ $(LDFLAGS) -m32 -o $@

run: $(PROJECT_NAME).nexe
	$(LDR) -- $(PROJECT_NAME).nexe

clean:
	$(RM) $(OBJECTS) $(TEST_OBJECTS) $(PROJECT_NAME).nexe
