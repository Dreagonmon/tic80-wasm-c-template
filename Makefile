# required packages: clang, lld
# optional packages: binaryen

BUILD = build
TARGET_NAME = demo
TARGET_WAT = $(BUILD)/$(TARGET_NAME).wat
TARGET_WASM = $(BUILD)/$(TARGET_NAME).wasm
TARGET_CART = $(BUILD)/$(TARGET_NAME).tic
CONFIG_CART = config.tic

CC = clang
LD = wasm-ld
WASM2WAT = wasm2wat
SIZE = llvm-size
ECHO = echo
RM_F = rm -f
TIC80 = tic80

# SRC += $(wildcard *.c littlefs/*.c)
SRC += $(wildcard src/*.c)
SRC += $(wildcard src/env/*.c)
SRC += $(wildcard src/libc/*.c)
SRC += $(wildcard src/libc/math/*.c)
SRC += $(wildcard src/libc/ctype/*.c)
SRC += $(wildcard src/libc/string/*.c)
SRC += $(wildcard src/libc/stdlib/*.c)
SRC += $(wildcard src/libc/umm_malloc/*.c)
SRC += $(wildcard src/libc/xprintf/*.c)
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
else
CFLAGS += -Os
endif
# include header
CFLAGS += -Isrc
CFLAGS += -Isrc/env
CFLAGS += -Isrc/libc

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

$(TARGET_WASM): $(OBJ)
	@$(ECHO) Linking...
	@$(LD) $^ $(LFLAGS) -o $@
	@$(SIZE) $(TARGET_WASM)
	@$(ECHO) done.

$(TARGET_WAT): $(TARGET_WASM)
	@$(WASM2WAT) -o $(TARGET_WAT) $(TARGET_WASM)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) -c -MMD $(CFLAGS) $< -o $@

clean:
	@$(ECHO) Cleaning...
	@$(RM_F) $(TARGET_WASM)
	@$(RM_F) $(TARGET_WAT)
	@$(RM_F) $(TARGET_CART)
	@$(RM_F) $(OBJ)
	@$(ECHO) done.

wasm: $(TARGET_WASM)

wat: $(TARGET_WAT)

run: $(TARGET_WASM)
	@$(RM_F) $(TARGET_CART)
	@$(TIC80) --skip --soft --fs . --cmd="new wasm & load $(CONFIG_CART) & import binary $(TARGET_WASM) & save $(TARGET_CART) & run & exit"

cart: $(TARGET_WASM)
	@$(RM_F) $(TARGET_CART)
	@$(TIC80) --skip --cli --fs . --cmd="new wasm & load $(CONFIG_CART) & import binary $(TARGET_WASM) & save $(TARGET_CART) & exit"

config:
	@$(TIC80) --skip --soft --fs . --cmd="new wasm & load $(CONFIG_CART) & edit"
