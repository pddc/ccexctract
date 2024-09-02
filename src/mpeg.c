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

#include "mpeg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
////////////////////////////////////////////////////////////////////////////////
// AVC RBSP Methods
//  TODO move the to a avcutils file
static size_t _find_emulation_prevention_byte(const uint8_t* data, size_t size)
{
    size_t offset = 2;

    while (offset < size) {
        if (0 == data[offset]) {
            // 0 0 X 3 //; we know X is zero
            offset += 1;
        } else if (3 != data[offset]) {
            // 0 0 X 0 0 3; we know X is not 0 and not 3
            offset += 3;
        } else if (0 != data[offset - 1]) {
            // 0 X 0 0 3
            offset += 2;
        } else if (0 != data[offset - 2]) {
            // X 0 0 3
            offset += 1;
        } else {
            // 0 0 3
            return offset;
        }
    }

    return size;
}

static size_t _copy_to_rbsp(uint8_t* destData, size_t destSize, const uint8_t* sorcData, size_t sorcSize)
{
    size_t toCopy, totlSize = 0;

    for (;;) {
        if (destSize >= sorcSize) {
            return 0;
        }

        // The following line IS correct! We want to look in sorcData up to destSize bytes
        // We know destSize is smaller than sorcSize because of the previous line
        toCopy = _find_emulation_prevention_byte(sorcData, destSize);
        memcpy(destData, sorcData, toCopy);
        totlSize += toCopy;
        destData += toCopy;
        destSize -= toCopy;

        if (0 == destSize) {
            return totlSize;
        }

        // skip the emulation prevention byte
        totlSize += 1;
        sorcData += toCopy + 1;
        sorcSize -= toCopy + 1;
    }

    return 0;
}




void sei_init(sei_t* sei, double timestamp)
{
    sei->head = 0;
    sei->tail = 0;
    sei->timestamp = timestamp;
}

void sei_free(sei_t* sei)
{
    sei_message_t* tail;

    while (sei->head) {
        tail = sei->head->next;
        free(sei->head);
        sei->head = tail;
    }

    sei_init(sei, 0);
}




////////////////////////////////////////////////////////////////////////////////

//00 00 01 06 data -->>> 04 44 B5 00 2F 03 3F D4 FF
libcaption_stauts_t sei_parse(sei_t* sei, const uint8_t* data, size_t size, double timestamp)
{
    sei_init(sei, timestamp);
    int ret = 0;

    // SEI may contain more than one payload
    while (1 < size) {
        size_t payloadType = 0;
        size_t payloadSize = 0;

        while (0 < size && 255 == (*data)) {
            payloadType += 255;
            ++data, --size;
        }

        if (0 == size) {
            return LIBCAPTION_ERROR;
        }

        payloadType += (*data);
        ++data, --size;

        while (0 < size && 255 == (*data)) {
            payloadSize += 255;
            ++data, --size;
        }

        if (0 == size) {
            return LIBCAPTION_ERROR;
        }

        payloadSize += (*data);
        ++data, --size;

        if (payloadSize) {
            //sei_message_t* msg = sei_message_new((sei_msgtype_t)payloadType, 0, payloadSize);

            struct _sei_message_t* msg = (struct _sei_message_t*)malloc(sizeof(struct _sei_message_t) + size);
            msg->next = 0;
            msg->type = payloadType;
            msg->size = payloadSize;
            msg->payload = ((uint8_t*)msg) + sizeof(struct _sei_message_t);
            memset(msg->payload, 0, size);
            size_t bytes = _copy_to_rbsp(msg->payload, payloadSize, data, size);


            if (sei->head == 0) {
                sei->head = msg;
                sei->tail = msg;
            } else {
                sei->tail->next = msg;
                sei->tail = msg;
            }


            if (bytes < payloadSize) {
                return LIBCAPTION_ERROR;
            }

            data += bytes;
            size -= bytes;
            ++ret;
        }
    }

    // There should be one trailing byte, 0x80. But really, we can just ignore that fact.
    return LIBCAPTION_OK;
}

////////////////////////////////////////////////////////////////////////////////
#define DEFAULT_CHANNEL 0

////////////////////////////////////////////////////////////////////////////////
// bitstream
void mpeg_bitstream_init(mpeg_bitstream_t* packet)
{
    packet->dts = 0;
    packet->cts = 0;
    packet->size = 0;
    packet->front = 0;
    packet->latent = 0;
    packet->status = LIBCAPTION_OK;
}


static size_t find_start_code(const uint8_t* data, size_t size)
{
    uint32_t start_code = 0xffffffff;
    for (size_t i = 1; i < size; ++i) {
        start_code = (start_code << 8) | data[i];
        if (0x00000100 == (start_code & 0xffffff00)) {
            return i - 3;
        }
    }
    return 0;
}

// WILL wrap around if larger than MAX_REFRENCE_FRAMES for memory saftey
cea708_t* _mpeg_bitstream_cea708_at(mpeg_bitstream_t* packet, size_t pos) { return &packet->cea708[(packet->front + pos) % MAX_REFRENCE_FRAMES]; }

cea708_t* _mpeg_bitstream_cea708_emplace_back(mpeg_bitstream_t* packet, double timestamp)
{
    ++packet->latent;
    cea708_t* cea708 = _mpeg_bitstream_cea708_at(packet, packet->latent - 1);
    cea708_init(cea708, timestamp);
    return cea708;
}

void _mpeg_bitstream_cea708_sort(mpeg_bitstream_t* packet)
{
    // TODO better sort? (for small nearly sorted lists bubble is difficult to beat)
    // This must be stable, decending sort
again:
    for (size_t i = 1; i < packet->latent; ++i) {
        cea708_t c;
        cea708_t* a = _mpeg_bitstream_cea708_at(packet, i - 1);
        cea708_t* b = _mpeg_bitstream_cea708_at(packet, i);
        if (a->timestamp > b->timestamp) {
            memcpy(&c, a, sizeof(cea708_t));
            memcpy(a, b, sizeof(cea708_t));
            memcpy(b, &c, sizeof(cea708_t));
            goto again;
        }
    }
}




size_t mpeg_bitstream_parse(const uint8_t* tsPacket, mpeg_bitstream_t* packet, caption_frame_t* frame, const uint8_t* data, size_t size, unsigned stream_type, double dts, double cts)
{
    if (MAX_NALU_SIZE <= packet->size) {
        packet->status = LIBCAPTION_ERROR;
        // fprintf(stderr, "LIBCAPTION_ERROR\n");
        return 0;
    }

    // consume upto MAX_NALU_SIZE bytes
    if (MAX_NALU_SIZE <= packet->size + size) {
        size = MAX_NALU_SIZE - packet->size;
    }

    sei_t seiMsgHolder;
    libcaption_stauts_t new_paket_status;

    size_t header_size, scpos;
    packet->status = LIBCAPTION_OK;
    memcpy(&packet->data[packet->size], data, size);
    packet->size += size;

    header_size = 4;

    while (packet->status == LIBCAPTION_OK) {

    	if ((scpos = find_start_code(&packet->data[0], packet->size)) <= header_size){
    		break;
    	}
    	if ((packet->size > 4) && ((packet->data[3] & 0x1F) == H264_SEI_PACKET)){

    		new_paket_status = sei_parse(&seiMsgHolder, &packet->data[header_size], scpos - header_size, dts + cts);
			packet->status = libcaption_status_update(packet->status, new_paket_status);


			int count = 0;
			int count2 = 0;

    		//for (sei_message_t* msg = seiMsgHolder.head; msg; msg = msg->next) {

    			sei_message_t* msg = seiMsgHolder.head;

    			if (msg->type == sei_type_user_data_registered_itu_t_t35) {

    				printf("count=%d\n",count++);
    				++packet->latent;
     			    cea708_t* cea708 = _mpeg_bitstream_cea708_at(packet, packet->latent - 1);
    			    cea708_init(cea708, dts + cts);
    			    new_paket_status = cea708_parse_h264(msg->payload, msg->size, cea708);
    			    packet->status = libcaption_status_update(packet->status, new_paket_status);




    			    _mpeg_bitstream_cea708_sort(packet);

    			    // Loop will terminate on LIBCAPTION_READY
    			    //while (packet->latent && packet->status == LIBCAPTION_OK && (cea708 =_mpeg_bitstream_cea708_at(packet, 0))->timestamp < dts) {
   			    	while (1) {

    			    	if (packet->latent == 0){
    			    		printf("Exit packet->latent == 0\n");
    			    		break;
    			    	}
    			    	if (packet->status != LIBCAPTION_OK){
    			    		printf("Exit status != LIBCAPTION_OK\n");
    			    		break;
    			    	}
    			    	if ((cea708 =_mpeg_bitstream_cea708_at(packet, 0))->timestamp >= dts){
    			    		printf("Exit timestamp >= dts\n");
    			    		break;
    			    	}

    			    	printf("count2=%d\n",count2++);
    			    	new_paket_status = cea708_to_caption_frame(frame, cea708);
    			    	packet->status   = libcaption_status_update(LIBCAPTION_OK, new_paket_status);
    			    	packet->front = (packet->front + 1) % MAX_REFRENCE_FRAMES;
    			    	--packet->latent;

    			    }
    			}
    		//}
    		sei_free(&seiMsgHolder);

    	}

        packet->size -= scpos;
        memmove(&packet->data[0], &packet->data[scpos], packet->size);
    }

    return size;
}

