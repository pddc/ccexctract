/**********************************************************************************************/
/* The MIT License                                                                            */
/*                                                                                            */
/* Copyright 2016-2017 Twitch Interactive, Inc. or its affiliates. All Rights Reserved.       */
/*                                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a copy               */
/* of this software and associated documentation files (the "Software"), to deal              */
/* in the Software without restriction, including without limitation the rights               */
/* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell                  */
/* copies of the Software, and to permit persons to whom the Software is                      */
/* furnished to do so, subject to the following conditions:                                   */
/*                                                                                            */
/* The above copyright notice and this permission notice shall be included in                 */
/* all copies or substantial portions of the Software.                                        */
/*                                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR                 */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,                   */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE                */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER                     */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,              */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN                  */
/* THE SOFTWARE.                                                                              */
/**********************************************************************************************/

#include "utf8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// returnes the length of the char in bytes
size_t utf8_char_length(const utf8_char_t* c)
{
    // count null term as zero size
    if (!c || 0x00 == c[0]) {
        return 0;
    }

    static const size_t _utf8_char_length[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0
    };

    return _utf8_char_length[(c[0] >> 3) & 0x1F];
}

int utf8_char_whitespace(const utf8_char_t* c)
{
    // 0x7F is DEL
    if (!c || (c[0] >= 0 && c[0] <= ' ') || c[0] == 0x7F) {
        return 1;
    }

    // EIA608_CHAR_NO_BREAK_SPACE TODO other utf8 spaces
    if (0xC2 == (unsigned char)c[0] && 0xA0 == (unsigned char)c[1]) {
        return 1;
    }

    return 0;
}



size_t utf8_char_copy(utf8_char_t* dst, const utf8_char_t* src)
{
    size_t bytes = utf8_char_length(src);

    if (bytes && dst) {
        memcpy(dst, src, bytes);
        dst[bytes] = '\0';
    }

    return bytes;
}


