#include <xprintf/xprintf.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <tic80.h>

typedef struct {
    char *buf;
    char *limit;
} BufferStream;

int __strip_rn_trace(char *str) {
	size_t len = strlen(str);
	if (len > 1 && str[len - 1] == '\n') {
		str[len - 1] = '\0';
		len --;
	}
	if (len > 1 && str[len - 1] == '\r') {
		str[len - 1] = '\0';
		len --;
	}
	trace(str, -1);
	return len;
}

static void __putbuff(void* p, void const* src, int len ) {
    BufferStream *stm = (BufferStream *) p;
    size_t limit = stm->limit - stm->buf;
	len = (len >= 0) ? len : 0;
    len = ((size_t) len < limit) ? len : limit - 1;
    if (len <= 0) return;
    memcpy(stm->buf, src, len);
    stm->buf += len;
}

int puts(const char *str) {
	trace(str, -1);
	return strlen(str) + 1;
}

int printf(char const* fmt, ... ) {
	char tmp[81];
	BufferStream stm = {
	    .buf = tmp,
	    .limit = tmp + 81
	};
	struct ostrm __stdout = { .p = &stm, .func = __putbuff };
	va_list va;
	va_start( va, fmt );
	int const rslt = xvprintf( &__stdout, fmt, va );
	va_end( va );
	int tail = (rslt < 80) ? rslt : 80;
	tmp[tail] = '\0';
	__strip_rn_trace(tmp);
	return rslt;
}

int sprintf(char* buff, char const* fmt, ... ) {
    char* tmp = buff;
    struct ostrm const ostrm = { .p = &tmp, .func = __putbuff };
	va_list va;
	va_start( va, fmt );
	int const rslt = xvprintf( &ostrm, fmt, va );
	va_end( va );
    buff[ rslt ] = '\0';
	return rslt;
}
