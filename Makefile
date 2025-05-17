# required packages: clang, lld
# optional packages: binaryen

BUILD = build
LIB_BUILD = build/lib
LIBC_A = $(LIB_BUILD)/libc.a
TARGET_NAME = demo
TARGET_WAT = $(BUILD)/$(TARGET_NAME).wat
TARGET_WASM = $(BUILD)/$(TARGET_NAME).wasm
TARGET_CART = $(BUILD)/$(TARGET_NAME).tic
CONFIG_CART = config.tic

CC = clang
LD = wasm-ld
AR = llvm-ar
WASM2WAT = wasm2wat
SIZE = llvm-size
ECHO = echo
RM_F = rm -f
TIC80 = tic80

# source
LIB_SRC += $(wildcard lib/env/*.c)
LIB_SRC += $(wildcard lib/libc/*.c)
LIB_SRC += $(wildcard lib/libc/math/*.c)
LIB_SRC += $(wildcard lib/libc/ctype/*.c)
LIB_SRC += $(wildcard lib/libc/string/*.c)
LIB_SRC += $(wildcard lib/libc/stdlib/*.c)
LIB_SRC += $(wildcard lib/libc/umm_malloc/*.c)
LIB_SRC += $(wildcard lib/libc/xprintf/*.c)
LIB_OBJ := $(LIB_SRC:%.c=$(LIB_BUILD)/%.o)
SRC += $(wildcard src/*.c)
OBJ := $(SRC:%.c=$(BUILD)/%.o)

# target
CFLAGS += --target=wasm32
CFLAGS += -std=gnu17 -Wall -Wextra
CFLAGS += -nostdlib
CFLAGS += -fvisibility=hidden
CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections
CFLAGS += -flto
CFLAGS += -foptimize-sibling-calls
# wasm3 support features
# CFLAGS += -mmutable-globals
# CFLAGS += -mnontrapping-fptoint
# CFLAGS += -msign-ext
# CFLAGS += -mmultivalue
CFLAGS += -mbulk-memory
# opt
ifdef DEBUG
CFLAGS += -O0 -g3
CFLAGS += -DDEBUG
else
CFLAGS += -Os
endif
# include header
CFLAGS += -Isrc
CFLAGS += -Ilib/env
CFLAGS += -Ilib/libc

# link
LFLAGS += --no-entry
LFLAGS += --strip-all
LFLAGS += --gc-sections
LFLAGS += --lto-O3
# stack and memory size
LFLAGS += --global-base=98304
LFLAGS += -z stack-size=4096
LFLAGS += --import-memory
LFLAGS += --initial-memory=262144
LFLAGS += --max-memory=262144


all: cart

$(TARGET_WASM): $(OBJ) $(LIBC_A)
	@$(ECHO) Linking...
	@$(LD) $^ $(LFLAGS) -o $@
	@$(SIZE) $(TARGET_WASM)
	@$(ECHO) done.

$(TARGET_WAT): $(TARGET_WASM)
	@$(WASM2WAT) -o $(TARGET_WAT) $(TARGET_WASM)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) -c -MMD $(CFLAGS) $< -o $@

$(LIBC_A): $(LIB_OBJ)
	@$(AR) -rc $(LIBC_A) $(LIB_OBJ)

$(LIB_BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) -c -MMD $(CFLAGS) $< -o $@

clean:
	@$(ECHO) Cleaning...
	@$(RM_F) $(TARGET_WASM)
	@$(RM_F) $(TARGET_WAT)
	@$(RM_F) $(TARGET_CART)
	@$(RM_F) $(OBJ)
	@$(ECHO) done.

cleanlib:
	@$(ECHO) Cleaning library...
	@$(RM_F) $(LIB_OBJ)
	@$(RM_F) $(LIBC_A)
	@$(ECHO) done.

wasm: $(TARGET_WASM)

wat: $(TARGET_WAT)

lib: $(LIBC_A)

run: $(TARGET_WASM)
	@$(RM_F) $(TARGET_CART)
	@$(TIC80) --skip --soft --fs . --cmd="new wasm & load $(CONFIG_CART) & import binary $(TARGET_WASM) & save $(TARGET_CART) & run & exit"

cart: $(TARGET_WASM)
	@$(RM_F) $(TARGET_CART)
	@$(TIC80) --skip --cli --fs . --cmd="new wasm & load $(CONFIG_CART) & import binary $(TARGET_WASM) & save $(TARGET_CART) & exit"

config:
	@$(TIC80) --skip --soft --fs . --cmd="new wasm & load $(CONFIG_CART) & edit"
