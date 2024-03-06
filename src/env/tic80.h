#ifndef __TIC80_H
#define __TIC80_H

#include <stdbool.h>
#include <stdint.h>

#define WASM_EXPORT(name) __attribute__((export_name(name)))
#define WASM_IMPORT(name) __attribute((import_module("env"), import_name(name)))

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------
//      Screen
// ---------------------------

// How big each tile is in pixels.
#define TILE_SIZE 8

// How many pixels wide the screen is.
#define WIDTH 240

// How many pixels tall the screen is.
#define HEIGHT 136

// How many tiles wide the screen is.
#define WIDTH_TILES (WIDTH / TILE_SIZE)

// How many tiles tall the screen is.
#define HEIGHT_TILES (HEIGHT / TILE_SIZE)

// How many bits-per-pixel.
#define BPP 4

// ---------------------------
//      Structures
// ---------------------------

// Keyboard key codes.
enum KEYCODES {
    KEY_NULL = 0,
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_MINUS,
    KEY_EQUALS,
    KEY_LEFTBRACKET,
    KEY_RIGHTBRACKET,
    KEY_BACKSLASH,
    KEY_SEMICOLON,
    KEY_APOSTROPHE,
    KEY_GRAVE,
    KEY_COMMA,
    KEY_PERIOD,
    KEY_SLASH,
    KEY_SPACE,
    KEY_TAB,
    KEY_RETURN,
    KEY_BACKSPACE,
    KEY_DELETE,
    KEY_INSERT,
    KEY_PAGEUP,
    KEY_PAGEDOWN,
    KEY_HOME,
    KEY_END,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_CAPSLOCK,
    KEY_CTRL,
    KEY_SHIFT,
    KEY_ALT
};

// Gamepad button codes.
enum BUTTON_CODES {
    BUTTON_CODE_P1_UP = 0,
    BUTTON_CODE_P1_DOWN,
    BUTTON_CODE_P1_LEFT,
    BUTTON_CODE_P1_RIGHT,
    BUTTON_CODE_P1_A,
    BUTTON_CODE_P1_B,
    BUTTON_CODE_P1_X,
    BUTTON_CODE_P1_Y,
    BUTTON_CODE_P2_UP,
    BUTTON_CODE_P2_DOWN,
    BUTTON_CODE_P2_LEFT,
    BUTTON_CODE_P2_RIGHT,
    BUTTON_CODE_P2_A,
    BUTTON_CODE_P2_B,
    BUTTON_CODE_P2_X,
    BUTTON_CODE_P2_Y,
    BUTTON_CODE_P3_UP,
    BUTTON_CODE_P3_DOWN,
    BUTTON_CODE_P3_LEFT,
    BUTTON_CODE_P3_RIGHT,
    BUTTON_CODE_P3_A,
    BUTTON_CODE_P3_B,
    BUTTON_CODE_P3_X,
    BUTTON_CODE_P3_Y,
    BUTTON_CODE_P4_UP,
    BUTTON_CODE_P4_DOWN,
    BUTTON_CODE_P4_LEFT,
    BUTTON_CODE_P4_RIGHT,
    BUTTON_CODE_P4_A,
    BUTTON_CODE_P4_B,
    BUTTON_CODE_P4_X,
    BUTTON_CODE_P4_Y
};

// Video RAM.
typedef struct {
    uint8_t SCREEN[WIDTH * HEIGHT * BPP / 8];
    uint8_t PALETTE[48];                       // 16 colors.
    uint8_t PALETTE_MAP[8];                    // 16 indices.
    uint8_t BORDER_COLOR_AND_OVR_TRANSPARENCY; // Bank 0 is border color, bank 1
                                               // is OVR transparency.
    int8_t SCREEN_OFFSET_X;
    int8_t SCREEN_OFFSET_Y;
    int8_t MOUSE_CURSOR;
    uint8_t BLIT_SEGMENT;
    uint8_t RESERVED[3];
} VRAM;

typedef struct {
    uint8_t x : 8;
    uint8_t y : 8;
    int16_t left : 1;
    int16_t middle : 1;
    int16_t right : 1;
    int16_t h : 6;
    int16_t v : 6;
} MouseRAM;

// Mouse return data.
typedef struct {
    int16_t x;
    int16_t y;
    int8_t scrollx;
    int8_t scrolly;
    bool left;
    bool middle;
    bool right;
} MouseStatus;

// ---------------------------
//      Pointers
// ---------------------------

#define FRAMEBUFFER ((VRAM *)0)
#define TILES ((uint8_t *)0x04000)
#define SPRITES ((uint8_t *)0x06000)
#define MAP ((uint8_t *)0x08000)
#define GAMEPADS ((uint8_t *)0x0FF80)
#define MOUSE ((MouseRAM *)0x0FF84)
#define KEYBOARD ((uint8_t *)0x0FF88)
#define SFX_STATE ((uint8_t *)0x0FF8C)
#define SOUND_REGISTERS ((uint8_t *)0x0FF9C)
#define WAVEFORMS ((uint8_t *)0x0FFE4)
#define SFX ((uint8_t *)0x100E4)
#define MUSIC_PATTERNS ((uint8_t *)0x11164)
#define MUSIC_TRACKS ((uint8_t *)0x13E64)
#define SOUND_STATE ((uint8_t *)0x13FFC)
#define STEREO_VOLUME ((uint8_t *)0x14000)
#define PERSISTENT_MEMORY ((uint8_t *)0x14004)
#define SPRITE_FLAGS ((uint8_t *)0x14404)
#define SYSTEM_FONT ((uint8_t *)0x14604)
#define WASM_FREE_RAM ((uint8_t *)0x18000)

// ---------------------------
//      Constants
// ---------------------------

#define TILES_SIZE (0x2000)
#define SPRITES_SIZE (0x2000)
#define MAP_SIZE (32640)
#define GAMEPADS_SIZE (4)
#define MOUSE_SIZE (4)
#define KEYBOARD_SIZE (4)
#define SFX_STATE_SIZE (16)
#define SOUND_REGISTERS_SIZE (72)
#define WAVEFORMS_SIZE (256)
#define SFX_SIZE (4224)
#define MUSIC_PATTERNS_SIZE (11520)
#define MUSIC_TRACKS_SIZE (408)
#define SOUND_STATE_SIZE (4)
#define STEREO_VOLUME_SIZE (4)
#define PERSISTENT_MEMORY_SIZE (1024)
#define SPRITE_FLAGS_SIZE (512)
#define SYSTEM_FONT_SIZE (2048)
#define WASM_FREE_RAM_SIZE (163840)

// ---------------------------
//      WASM Special Constants
// ---------------------------

#define TIC80_PARAM_IGNORE ((int32_t) -1)

// ---------------------------
//      Drawing Functions
// ---------------------------

WASM_IMPORT("circ")
// Draw a filled circle.
void circ(int32_t x, int32_t y, int32_t radius, int8_t color);

WASM_IMPORT("circb")
// Draw a circle border.
void circb(int32_t x, int32_t y, int32_t radius, int8_t color);

WASM_IMPORT("elli")
// Draw a filled ellipse.
void elli(int32_t x, int32_t y, int32_t a, int32_t b, int8_t color);

WASM_IMPORT("ellib")
// Draw an ellipse border.
void ellib(int32_t x, int32_t y, int32_t a, int32_t b, int8_t color);

WASM_IMPORT("clip")
// Set the screen clipping region.
void clip(int32_t x, int32_t y, int32_t width, int32_t height);

WASM_IMPORT("cls")
// Clear the screen.
void cls(int8_t color);

WASM_IMPORT("font")
// Print a string using foreground sprite data as the font.
int8_t font(const char *text, int32_t x, int32_t y, uint8_t *trans_colors,
            int8_t trans_count, int8_t char_width, int8_t char_height,
            bool fixed, int8_t scale, bool alt);

WASM_IMPORT("line")
// Draw a straight line.
void line(float x0, float y0, float x1, float y1, int8_t color);

WASM_IMPORT("map")
// Draw a map region.
void map(int32_t x, int32_t y, int32_t w, int32_t h, int32_t sx, int32_t sy,
         uint8_t *trans_colors, int8_t colorCount, int8_t scale, int32_t remap);

WASM_IMPORT("pix")
// Get or set the color of a single pixel.
uint8_t pix(int32_t x, int32_t y, int8_t color);

WASM_IMPORT("print")
// Print a string using the system font.
int32_t print(const char *text, int32_t x, int32_t y, int8_t color,
              int8_t fixed, int32_t scale, int8_t alt);

WASM_IMPORT("rect")
// Draw a filled rectangle.
void rect(int32_t x, int32_t y, int32_t w, int32_t h, int8_t color);

WASM_IMPORT("rectb")
// Draw a rectangle border.
void rectb(int32_t x, int32_t y, int32_t w, int32_t h, int8_t color);

WASM_IMPORT("spr")
// Draw a sprite or composite sprite.
void spr(int32_t id, int32_t x, int32_t y, uint8_t *trans_colors,
         int8_t color_count, int32_t scale, int32_t flip, int32_t rotate,
         int32_t w, int32_t h);

WASM_IMPORT("tri")
// Draw a filled triangle.
void tri(float x1, float y1, float x2, float y2, float x3, float y3,
         int8_t color);

WASM_IMPORT("trib")
// Draw a triangle border.
void trib(float x1, float y1, float x2, float y2, float x3, float y3,
          int8_t color);

WASM_IMPORT("ttri")
// Draw a triangle filled with texture.
void ttri(float x1, float y1, float x2, float y2, float x3, float y3, float u1,
          float v1, float u2, float v2, float u3, float v3, int32_t texsrc,
          uint8_t *trans_colors, int8_t color_count, float z1, float z2,
          float z3, bool depth);

// ---------------------------
//      Input Functions
// ---------------------------

WASM_IMPORT("btn")
// Get gamepad button state in current frame.
int32_t btn(int32_t index);

WASM_IMPORT("btnp")
// Get gamepad button state according to previous frame.
int32_t btnp(int32_t index, int32_t hold, int32_t period);

WASM_IMPORT("key")
// Get keyboard button state in current frame.
int32_t key(int32_t x);

WASM_IMPORT("keyp")
// Get keyboard button state relative to previous frame.
int32_t keyp(int8_t x, int32_t hold, int32_t period);

WASM_IMPORT("mouse")
// Get XY and press state of mouse/touch.
void mouse(MouseStatus *mouse_ptr_addy);

// ---------------------------
//      Sound Functions
// ---------------------------

WASM_IMPORT("music")
// Play or stop playing music.
void music(int32_t track, int32_t frame, int32_t row, bool loop, bool sustain,
           int32_t tempo, int32_t speed);

WASM_IMPORT("sfx")
// Play or stop playing a given sound.
void sfx(int32_t sfx_id, int32_t note, int32_t octave, int32_t duration,
         int32_t channel, int32_t volume_left, int32_t volume_right,
         int32_t speed);

// ---------------------------
//      Memory Functions
// ---------------------------

WASM_IMPORT("pmem")
// Access or update the persistent memory. Param `address` must in range [0,
// 255]. Pass -1 to `value` will not update the value.
uint32_t pmem(int32_t address, int64_t value);

WASM_IMPORT("peek")
// Read a byte from an address in RAM.
int8_t peek(int32_t address, int8_t bits);

WASM_IMPORT("peek1")
// Read a single bit from an address in RAM.
int8_t peek1(int32_t address);

WASM_IMPORT("peek2")
// Read two bit value from an address in RAM.
int8_t peek2(int32_t address);

WASM_IMPORT("peek4")
// Read a nibble value from an address.
int8_t peek4(int32_t address);

WASM_IMPORT("poke")
// Write a byte value to an address in RAM.
void poke(int32_t address, int8_t value, int8_t bits);

WASM_IMPORT("poke1")
// Write a single bit to an address in RAM.
void poke1(int32_t address, int8_t value);

WASM_IMPORT("poke2")
// Write a two bit value to an address in RAM.
void poke2(int32_t address, int8_t value);

WASM_IMPORT("poke4")
// Write a nibble value to an address in RAM.
void poke4(int32_t address, int8_t value);

WASM_IMPORT("sync")
// Copy banks of RAM (sprites, map, etc) to and from the cartridge.
void sync(int32_t mask, int8_t bank, int8_t to_cart);

WASM_IMPORT("vbank")
// Switch the 16kb of banked video RAM.
int8_t vbank(int8_t bank);

// ---------------------------
//      Utility Functions
// ---------------------------

WASM_IMPORT("fget")
// Retrieve a sprite flag.
bool fget(int32_t sprite_index, int8_t flag);

WASM_IMPORT("fset")
// Update a sprite flag.
bool fset(int32_t sprite_index, int8_t flag, bool value);

WASM_IMPORT("mget")
// Retrieve a map tile at given coordinates.
int32_t mget(int32_t x, int32_t y);

WASM_IMPORT("mset")
// Update a map tile at given coordinates.
void mset(int32_t x, int32_t y, int32_t value);

// ---------------------------
//      System Functions
// ---------------------------

WASM_IMPORT("exit")
// Interrupt program and return to console.
__attribute__((noreturn)) void tic80_exit();

WASM_IMPORT("time")
// Returns how many milliseconds have passed since game started.
float time();

WASM_IMPORT("tstamp")
// Returns the current Unix timestamp in seconds.
uint32_t tstamp();

WASM_IMPORT("trace")
// Print a string to the Console. Pass -1 to `color` will use the default
// color(15).
void trace(const char *text, int8_t color);

#ifdef __cplusplus
}
#endif

#endif
