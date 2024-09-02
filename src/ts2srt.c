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
#include "ts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    //const char* path = argv[1];
   
    char mydata[2048];

    ts_t ts;
    mpeg_bitstream_t mpegbs;
    caption_frame_t frame;
    uint8_t pkt[TS_PACKET_SIZE];
    ts_init(&ts);
    caption_frame_init(&frame);
    mpeg_bitstream_init(&mpegbs);

    //srt = vtt_new();
    FILE* file = fopen("./cc_minimum.ts", "rb");
    if(!file) {
        printf("Failed to open input\n");
        return EXIT_FAILURE;
    }

    setvbuf(file, 0, _IOFBF, 8192 * TS_PACKET_SIZE);
    while (TS_PACKET_SIZE == fread(&pkt[0], 1, TS_PACKET_SIZE, file)) {

        if (LIBCAPTION_READY == ts_parse_packet(&ts, &pkt[0])) {
            double dts = ts_dts_seconds(&ts);
            double cts = ts_cts_seconds(&ts);
            while (ts.size) {

                size_t bytes_read = mpeg_bitstream_parse(&pkt[0], &mpegbs, &frame, ts.data, ts.size, STREAM_TYPE_H264, dts, cts);
                ts.data += bytes_read, ts.size -= bytes_read;

                switch (mpegbs.status) {
                default:
                    return EXIT_FAILURE;
                    break;

                case LIBCAPTION_OK:
                    break;

                case LIBCAPTION_READY: {
                	printf("-------------------------------\n");
                    caption_frame_to_text(&frame, mydata);
                    printf("data:\n%s\n",mydata);

                } break;
                } //switch
            } // while
        } // if
    } // while

    printf("------------------------------------------------------------\n");



    return EXIT_SUCCESS;
}
