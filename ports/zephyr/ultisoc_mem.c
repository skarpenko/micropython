/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 The UltiSoC Project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stddef.h>


/*
 * Ultiparc core doesn't support load/store instructions for unaligned memory
 * access (lwr, lwl, swl, swr). However these instructions are used in memory
 * functions provided with MIPS-I toolchain. Functions below override standard
 * implementations with simpler versions which do not use unsupported
 * instructions.
 */


void *memcpy(void *dst, const void *src, size_t n) {
    char *d = dst;
    const char *s = src;
    size_t i;

    for(i = 0; i < n; ++i)
        d[i] = s[i];

    return dst;
}


void *memset(void *s, int c, size_t n) {
    char *xs = (char *)s;

    for( ; n > 0; --n)
        *xs++ = (char)c;

    return s;
}
