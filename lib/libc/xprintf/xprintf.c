
/*
<https://github.com/rafagafe/xprintf>

  Licensed under the MIT License <http://opensource.org/licenses/MIT>.
  SPDX-License-Identifier: MIT
  Copyright (c) 2016-2018 Rafa Garcia <rafagarcia77@gmail.com>.
  Permission is hereby  granted, free of charge, to any  person obtaining a copy
  of this software and associated  documentation files (the "Software"), to deal
  in the Software  without restriction, including without  limitation the rights
  to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
  copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
  IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
  FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
  AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
  LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/

#include "xprintf.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <stdint.h>

#ifdef USE_XPRNTF_CFG
#include "xprintf-cfg.h"
#else
enum {
    diswidth = 0,
    disprec  = 0,
    disu     = 0,
    disd     = 0,
    disx     = 0,
    disz     = 0,
    disl     = 0,
    disll    = 0,
    disn     = 0
};
#endif

/** Get flag from format string.
  * @param flag Destination '-', '+', ' ', '#', '0' or 'n' for none
  * @param fmt Format string.
  * @return num bytes read from format string. 0 ir 1. */
static int getflag( int* flag, char const* fmt  ) {
    static char const flags[] = { '-', '+', ' ', '#', '0' };
    for( size_t i = 0; i < sizeof flags; ++i ) {
        if ( flags[i] == *fmt ) {
            *flag = flags[i];
            return 1;
        }
    }
    *flag = 'n';
    return 0;
}

/** Get and number from format string.
  * @param dest Destination.
  * @param fmt Format string.
  * @return num bytes read from format string. */
static int getnum( int* dest, char const* fmt ) {
    int const max = 6;
    *dest = 0;
    for( int len = 0; max > len; ++len ) {
        if ( !isdigit( (unsigned char)fmt[len] ) )
            return len;
        *dest *= 10;
        *dest += fmt[ len ] - '0';
    }
    return max;
}

/** Get and number from format string or from va_list i an * is found.
  * @param dest Destination.
  * @param va
  * @param fmt Format string.
  * @return num bytes read from format string. */
#define getval( dest, va, src ) ({          \
    int rslt = 0;                           \
    if ( isdigit( (unsigned char)*src ) )   \
        rslt = getnum( dest, src );         \
    else if ( '*' == *src ) {               \
        *dest = va_arg( va, int );          \
        rslt = 1;                           \
    }                                       \
    else                                    \
        rslt = 0;                           \
    rslt;                                   \
})

/** Reverse a string.
  * @param str String to be reversed.
  * @param len Length of the string. */
static void reverse( char str[], int len )  {
    int start = 0;
    int end   = len -1;
    while( start < end )  {
        char const tmp = str[ start ];
        str[ start ] = str[ end ];
        str[ end ] = tmp;
        ++start;
        --end;
    }
}

/** Print an integer in hexadecimal base in a buffer.
  * @param dest  Destination buffer.
  * @param val   Value to be printed.
  * @param upper Print hexa digit in upper or lower case.
  * @return Numbytes printed. */
#define x2a( dest, val, upper ) ({              \
    char const* const ptr = upper ?             \
        "0123456789ABCDEF":                     \
        "0123456789abcdef";                     \
    int len = 0;                                \
    while ( 0 != val ) {                        \
        int const nibble = val % 16;            \
        dest[ len++ ] = ptr[ nibble ];          \
        val /= 16;                              \
    }                                           \
    if ( 0 == len ) {                           \
        *dest = '0';                            \
        len = 1;                                \
    }                                           \
    dest[ len ] = '\0';                         \
    reverse( dest, len );                       \
    len;                                        \
})

/** Print an unsigned integer in decimal base in a buffer.
  * @param dest Destination buffer.
  * @param val  Value to be printed.
  * @return Numbytes printed. */
#define u2a( dest, val ) ({                 \
    int len = 0;                            \
    while ( 0 != val ) {                    \
        unsigned int const rem = val % 10;  \
        dest[ len++ ] = rem + '0';          \
        val /= 10;                          \
    }                                       \
    if ( 0 == len ) {                       \
        *dest = '0';                        \
        len = 1;                            \
    }                                       \
    dest[ len ] = '\0';                     \
    reverse( dest, len );                   \
    len;                                    \
})

/** Print a signed integer in decimal base in a buffer.
  * @param dest Destination buffer.
  * @param val  Value to be printed.
  * @param sign Force print '+'
  * @return Numbytes printed. */
#define i2a( dest, val, sign ) ({           \
    int const neg = 0 > val;                \
    if ( neg ) {                            \
        *dest = '-';                        \
        val = -val;                         \
    }                                       \
    else if ( sign )                        \
        *dest = '+';                        \
    char* ptr = dest + ( neg || sign );     \
    ( neg || sign ) + u2a( ptr, val );      \
})

/** Send to an output stream a memory block.
  * @param obj Destination output stream.
  * @param src Source memory block.
  * @param len Length in bytes of the memory block. */
static void ostrm( struct ostrm const* obj, void const* src, int len ) {
    obj->func( obj->p, src, len );
}

/** Send to an output stream a character.
  * @param obj Destination output stream.
  * @param ch  character value. */
static void ostrmch( struct ostrm const* obj, unsigned char ch ) {
    ostrm( obj, &ch, sizeof ch );
}

/** Send to an output stream a character several times.
  * @param obj Destination output stream.
  * @param ch  Character value.
  * @param qty Number of times. */
static void ostrmchq( struct ostrm const* obj, unsigned char ch, int qty ) {
    for( int i = 0; i < qty; ++i )
        ostrm( obj, &ch, sizeof ch );
}

#ifndef max
#define max( a, b ) ( (a) > (b) ? (a) : (b) )
#endif

/** Send to an output stream a string with a number.
  * @param obj   Destination output stream.
  * @param buff  source string with a number.
  * @param len   Length in bytes of the string.
  * @param width Width got in the format string.
  * @param flag  Flag got in the format string.
  * @param prec  Precision got in the format string. */
static int sendnum( struct ostrm const* obj, char const* buff, int len, int width, int flag, int prec, char base ) {

    int rslt = 0;

    int const hassign       = '-' == *buff || '+' == *buff;
    int const hasbase       = '\0' != base && '0' != *buff;
    int const numdigits     = len - hassign;
    int const hasprec       = INT_MAX != prec;
    int const zerosprec     = hasprec * ( max( numdigits, prec ) - numdigits );
    int const totallen      = numdigits + zerosprec + hassign + 2*hasbase;
    int const padding       = width > totallen ? width - totallen : 0;
    int const leftjust      = '-' == flag;
    int const zerospadding  = ( !hasprec && '0' == flag && !leftjust ) * padding;
    int const leftpadding   = ( !zerospadding && !leftjust ) * padding;
    int const rigthpadding  = leftjust * padding;
    int const numzeros      = max( zerospadding, zerosprec );

    ostrmchq( obj, ' ', leftpadding );
    rslt += leftpadding;

    if ( hasbase ) {
        ostrmch( obj, '0' );
        ostrmch( obj, base );
        rslt += 2;
    }

    if ( hassign && 0 != numzeros ) {
        ostrmch( obj, *buff );
        ++buff, --len, ++rslt;
    }

    ostrmchq( obj, '0', numzeros );
    rslt += numzeros;

    ostrm( obj, buff, len );
    rslt += len;

    ostrmchq( obj, ' ', rigthpadding );
    rslt += rigthpadding;

    return rslt;
}

/* Header compare */
static size_t hdrcmp( char const* header, char const* str ) {
    size_t len;
    for( len = 0; '\0' != *header; ++len, ++str, ++header )
        if ( *header != *str )
            return 0;
    return len;
}

enum argtype { none, l, ll, z };

enum argtype gettype( char const** fmt ) {
    static struct { char const* token; enum argtype type; } const lut [] = {
        { "ll", ll },
        { "l",  l  },
        { "z",  z  }
    };
    for( size_t i = 0; i < sizeof lut / sizeof *lut; ++i ) {
        size_t len = hdrcmp( lut[i].token, *fmt );
        if ( len ) {
            *fmt += len;
            return lut[i].type;
        }
    }
    return none;
}


/* Print formatted data to a user defined stream. */
int xvprintf( struct ostrm const* o, char const* fmt, va_list va ) {

    char buff[ 22 ];

    int rslt = 0;

    while( '\0' != *fmt ) {

        if( '%' != *fmt ) {
            int len = 0;
            while( '\0' != fmt[len] && '%' != fmt[len] )
                ++len;
            ostrm( o, fmt, len );
            rslt += len;
            fmt += len;
            continue;
        }
        ++fmt;

        int flag;
        fmt += getflag( &flag, fmt );

        int width = 0;
        fmt += getval( &width, va, fmt );
        if ( 0 > width ) {
            flag = '-';
            width = -width;
        }
        if ( diswidth )
            width = 0;

        int precision = INT_MAX;
        if ( '.' == *fmt ) {
            ++fmt;
            fmt += getval( &precision, va, fmt );
        }
        if ( disprec )
            precision = INT_MAX;

        enum argtype const type = gettype( &fmt );
        int const specifier = *fmt++;
        switch( specifier ) {
            case '\0':
                return rslt;
            case '%':
                ostrmch( o, '%' );
                ++rslt;
                break;
            case 'u': {
                if ( disu )
                    return rslt;
                int len;
                switch( type ) {
                    case none: {
                        unsigned int val = va_arg( va, unsigned int );
                        len = u2a( buff, val );
                        break;
                    }
                    case z: {
                        if ( disz )
                            return rslt;
                        size_t val = va_arg( va, size_t );
                        len = u2a( buff, val );
                        break;
                    }
                    case l: {
                        if ( disl )
                            return rslt;
                        unsigned long int val = va_arg( va, unsigned long int );
                        len = u2a( buff, val );
                        break;
                    }
                    case ll: {
                        if ( disll )
                            return rslt;
                        unsigned long long int val = va_arg( va, unsigned long long int );
                        len = u2a( buff, val );
                        break;
                    }
                    default:
                        return rslt;
                }
                rslt += sendnum( o, buff, len, width, flag, precision, '\0' );
                break;
            }
            case 'i':
            case 'd': {
                if ( disd )
                    return rslt;
                int len;
                switch( type ) {
                    case none: {
                        int val = va_arg( va, int );
                        len = i2a( buff, val, '+' == flag );
                        break;
                    }
                    case z: {
                        if ( disz )
                            return rslt;
                        size_t val = va_arg( va, size_t );
                        len = i2a( buff, val, '+' == flag );
                        break;
                    }
                    case l: {
                        if ( disl )
                            return rslt;
                        long int val = va_arg( va, long int );
                        len = i2a( buff, val, '+' == flag );
                        break;
                    }
                    case ll: {
                        if ( disll )
                            return rslt;
                        long long int val = va_arg( va, long long int );
                        len = i2a( buff, val, '+' == flag );
                        break;
                    }
                    default:
                        return rslt;
                }
                rslt += sendnum( o, buff, len, width, flag, precision, '\0' );
                break;
            }
            case 'x':
            case 'X': {
                if ( disx )
                    return rslt;
                int const upper = 'X' == specifier;
                int len;
                switch( type ) {
                    case none: {
                        unsigned int val = va_arg( va, unsigned int );
                        len = x2a( buff, val, upper );
                        break;
                    }
                    case z: {
                        if ( disz )
                            return rslt;
                        size_t val = va_arg( va, size_t );
                        len = x2a( buff, val, upper );
                        break;
                    }
                    case l: {
                        if ( disl )
                            return rslt;
                        unsigned long int val = va_arg( va, unsigned long int );
                        len = x2a( buff, val, upper );
                        break;
                    }
                    case ll: {
                        if ( disll )
                            return rslt;
                        unsigned long long int val = va_arg( va, unsigned long long int );
                        len = x2a( buff, val, upper );
                        break;
                    }
                    default:
                        return rslt;
                }
                int const base  = '#' == flag ? specifier : '\0';
                rslt += sendnum( o, buff, len, width, flag, precision, base );
                break;
            }
            case 'p': {
                if ( disx )
                    return rslt;
                uintptr_t val = (uintptr_t)va_arg( va, void* );
                char const* ptr;
                int len;
                int base;
                if ( 0 != val ) {
                    ptr = buff;
                    len = x2a( buff, val, 0 );
                    base = 'x';
                }
                else {
                    static char const nullval[] = "(nil)";
                    ptr = nullval;
                    len = sizeof nullval - 1;
                    base = '\0';
                    if ( '0' == flag )
                        flag = 'n';
                }
                rslt += sendnum( o, ptr, len, width, flag, precision, base );
                break;
            }
            case 'n': {
                if ( disn )
                    return rslt;
                int* val = va_arg( va, int* );
                if ( NULL != val )
                    *val = rslt;
                break;
            }
            case 'c':
            case 's': {
                char tmp;
                char const* val;
                int vallen;
                if ( 's' == specifier ) {
                    val = va_arg( va, char* );
                    if ( NULL == val )
                        val = "(null)";
                    vallen = strlen( val );
                }
                else {
                    tmp = va_arg( va, int );
                    val = &tmp;
                    vallen = 1;
                }
                int const len = precision < vallen ? precision : vallen;
                if ( '-' != flag && width > len ) {
                    int const diff = width - len;
                    for( int i = 0; i < diff; ++i )
                        ostrmch( o, ' ' );
                    rslt += diff;
                }
                ostrm( o, val, len );
                rslt += len;
                if ( '-' == flag && width > len ) {
                    int const diff = width - len;
                    for( int i = 0; i < diff; ++i )
                        ostrmch( o, ' ' );
                    rslt += diff;
                }
                break;
            }
            default:
                return rslt;
        }
    }
    return rslt;
}

/* Print formatted data to a user defined stream. */
int xprintf( struct ostrm const* o, char const* fmt, ... ) {
	va_list va;
	va_start( va, fmt );
	int const rslt = xvprintf( o, fmt, va );
	va_end( va );
	return rslt;
}

static void putbuff( void* p, void const* src, int len ) {
    char** buff = p;
    memcpy( *buff, src, len );
    *buff += len;
}

/* Print formatted data to a buffer. */
int xsprintf( char* buff, char const* fmt, ... ) {
    char* tmp = buff;
    struct ostrm const ostrm = { .p = &tmp, .func = putbuff };
	va_list va;
	va_start( va, fmt );
	int const rslt = xvprintf( &ostrm, fmt, va );
	va_end( va );
    buff[ rslt ] = '\0';
	return rslt;
}
