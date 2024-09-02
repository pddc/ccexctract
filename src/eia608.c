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
#include "eia608.h"
#include <stdio.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
int eia608_row_map[] = { 10, -1, 0, 1, 2, 3, 11, 12, 13, 14, 4, 5, 6, 7, 8, 9 };
int eia608_reverse_row_map[] = { 2, 3, 4, 5, 10, 11, 12, 13, 14, 15, 0, 6, 7, 8, 9, 1 };

const char* eia608_style_map[] = {
    "white",
    "green",
    "blue",
    "cyan",
    "red",
    "yellow",
    "magenta",
    "italics",
};

static inline uint16_t eia608_row_pramble(int row, int chan, int x, int underline)
{
    row = eia608_reverse_row_map[row & 0x0F];
    return eia608_parity(0x1040 | (chan ? 0x0800 : 0x0000) | ((row << 7) & 0x0700) | ((row << 5) & 0x0020)) | ((x << 1) & 0x001E) | (underline ? 0x0001 : 0x0000);
}

uint16_t eia608_row_column_pramble(int row, int col, int chan, int underline) { return eia608_row_pramble(row, chan, 0x10 | (col / 4), underline); }
uint16_t eia608_row_style_pramble(int row, int chan, eia608_style_t style, int underline) { return eia608_row_pramble(row, chan, style, underline); }
uint16_t eia608_midrow_change(int chan, eia608_style_t style, int underline) { return eia608_parity(0x1120 | ((chan << 11) & 0x0800) | ((style << 1) & 0x000E) | (underline & 0x0001)); }

int eia608_parse_preamble(uint16_t cc_data, int* row, int* col, eia608_style_t* style, int* chan, int* underline)
{
    (*row) = eia608_row_map[((0x0700 & cc_data) >> 7) | ((0x0020 & cc_data) >> 5)];
    (*chan) = !!(0x0800 & cc_data);
    (*underline) = 0x0001 & cc_data;

    if (0x0010 & cc_data) {
        (*style) = eia608_style_white;
        (*col) = 4 * ((0x000E & cc_data) >> 1);
    } else {
        (*style) = (0x000E & cc_data) >> 1;
        (*col) = 0;
    }

    return 1;
}

int eia608_parse_midrowchange(uint16_t cc_data, int* chan, eia608_style_t* style, int* underline)
{
    (*chan) = !!(0x0800 & cc_data);

    if (0x1120 == (0x7770 & cc_data)) {
        (*style) = (0x000E & cc_data) >> 1;
        (*underline) = 0x0001 & cc_data;
    }

    return 1;
}
////////////////////////////////////////////////////////////////////////////////
// control command
eia608_control_t eia608_parse_control(uint16_t cc_data, int* cc)
{
    if (0x0200 & cc_data) {
        (*cc) = (cc_data & 0x0800 ? 0x01 : 0x00);
        return (eia608_control_t)(0x177F & cc_data);
    } else {
        (*cc) = (cc_data & 0x0800 ? 0x01 : 0x00) | (cc_data & 0x0100 ? 0x02 : 0x00);
        return (eia608_control_t)(0x167F & cc_data);
    }
}

uint16_t eia608_control_command(eia608_control_t cmd, int cc)
{
    uint16_t c = (cc & 0x01) ? 0x0800 : 0x0000;
    uint16_t f = (cc & 0x02) ? 0x0100 : 0x0000;

    if (eia608_tab_offset_0 == (eia608_control_t)(cmd & 0xFFC0)) {
        return (eia608_control_t)eia608_parity(cmd | c);
    } else {
        return (eia608_control_t)eia608_parity(cmd | c | f);
    }
}
////////////////////////////////////////////////////////////////////////////////
// text

static int eia608_to_index(uint16_t cc_data, int* chan, int* c1, int* c2)
{
    (*c1) = (*c2) = -1;
    (*chan) = 0;
    cc_data &= 0x7F7F; // strip off parity bits

    // Handle Basic NA BEFORE we strip the channel bit
    if (eia608_is_basicna(cc_data)) {
        // we got first char, yes. But what about second char?
        (*c1) = (cc_data >> 8) - 0x20;
        cc_data &= 0x00FF;

        if (0x0020 <= cc_data && 0x0080 > cc_data) {
            (*c2) = cc_data - 0x20;
            return 2;
        }

        return 1;
    }

    // Check then strip second channel toggle
    (*chan) = cc_data & 0x0800;
    cc_data = cc_data & 0xF7FF;

    if (eia608_is_specialna(cc_data)) {
        // Special North American character
        (*c1) = cc_data - 0x1130 + 0x60;
        return 1;
    }

    if (0x1220 <= cc_data && 0x1240 > cc_data) {
        // Extended Western European character set, Spanish/Miscellaneous/French
        (*c1) = cc_data - 0x1220 + 0x70;
        return 1;
    }

    if (0x1320 <= cc_data && 0x1340 > cc_data) {
        // Extended Western European character set, Portuguese/German/Danish
        (*c1) = cc_data - 0x1320 + 0x90;
        return 1;
    }

    return 0;
}

static const char* utf8_from_index(int idx) { return (0 <= idx && EIA608_CHAR_COUNT > idx) ? eia608_char_map[idx] : ""; }
int eia608_to_utf8(uint16_t c, int* chan, char* str1, char* str2)
{
    int c1, c2;
    int size = (int)eia608_to_index(c, chan, &c1, &c2);
    utf8_char_copy(str1, utf8_from_index(c1));
    utf8_char_copy(str2, utf8_from_index(c2));
    return size;
}

