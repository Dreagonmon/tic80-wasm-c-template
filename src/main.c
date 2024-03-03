#include <stdint.h>
#include <stdio.h>
#include <tic80.h>
#include <_malloc.h>

#define max(a, b) (a > b) ? a : b
#define min(a, b) (a < b) ? a : b

static int x, y, t;
static int r = 0;
static uint8_t transcolors = { 14 };

void init_heap(void) {
    size_t mems = _init_memory(4);
    printf("heap: %lu bytes\n", mems);
}

WASM_EXPORT("BOOT")
void BOOT() {
    init_heap(); // if you don't need malloc functions, you can skip init_heap()
    t = 0;
    x = 96;
    y = 24;
    r = 0;
    printf("RAM Size: %lu\n", sizeof(MouseRAM));
}

WASM_EXPORT("TIC")
void TIC() {
    if (btn(0)) {
        y --;
    }
    if (btn(1)) {
        y ++;
    }
    if (btn(2)) {
        x --;
    }
    if (btn(3)) {
        x ++;
    }
    cls(13);
    spr(1 + t%60 / 30 * 2, x, y, &transcolors, 1, 3, 0, 0, 2, 2);
    t ++;

    // Mouse example, using tic80 api.
    MouseStatus md;
    mouse(&md);
    if (md.left) {
        r ++;
    } else {
        r --;
    }
    r = min(32, r);
    r = max(0, r);
    line(md.x, 0, md.x, 136, 11);
    line(0, md.y, 240, md.y, 11);
    circ(md.x, md.y, r, 11);

    const int BUFSIZ = 10;
    char buf[BUFSIZ];
    sprintf(buf, "(%03d,%03d) %03d", md.x, md.y, r);
    // puts(buf);
    print(buf, 3, 3, 15, 0, 1, 1);

    // Mouse example, direct memory access.
    const int BUFSIZ2 = 48;
    char buf2[BUFSIZ2];
    sprintf(
        buf2,
        "x: %03d, y: %03d, l: %01u, m: %01u, r: %01u, h: %02d, v: %02d",
        ((int32_t) MOUSE->x) - 8,
        ((int32_t) MOUSE->y) - 4,
        (bool) MOUSE->left,
        (bool) MOUSE->middle,
        (bool) MOUSE->right,
        MOUSE->h,
        MOUSE->v
    );
    print(buf2, 3, 3 + 8, 15, 0, 1, 1);
}
