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
#include "cea708.h"
#include <float.h>
#include <memory.h>
#include <stdio.h>



int cea708_init(cea708_t* cea708, double timestamp)
{
    memset(cea708, 0, sizeof(cea708_t));
    cea708->country = country_united_states;
    cea708->provider = t35_provider_atsc;
    cea708->user_identifier = GA94;
    cea708->user_data_type_code = 3;
    cea708->directv_user_data_length = 0;
    cea708->user_data.process_em_data_flag = 0;
    cea708->user_data.process_cc_data_flag = 1;
    cea708->user_data.additional_data_flag = 0;
    cea708->user_data.em_data = 0xFF;
    cea708->user_data.cc_count = 0;
    cea708->timestamp = timestamp;
    return 1;
}


void cea708_parse_user_data_type_strcture(const uint8_t* data, size_t size, user_data_t* user_data)
{
    memset(user_data, 0, sizeof(user_data_t));
    user_data->process_em_data_flag = !!(data[0] & 0x80);
    user_data->process_cc_data_flag = !!(data[0] & 0x40);
    user_data->additional_data_flag = !!(data[0] & 0x20);
    user_data->cc_count = (data[0] & 0x1F);
    user_data->em_data = data[1];
    data += 2, size -= 2;

    for (int i = 0; 3 < size && i < (int)user_data->cc_count; ++i, data += 3, size -= 3) {
        user_data->cc_data[i].marker_bits = data[0] >> 3;
        user_data->cc_data[i].cc_valid = data[0] >> 2;
        user_data->cc_data[i].cc_type = data[0] >> 0;
        user_data->cc_data[i].cc_data = data[1] << 8 | data[2];
    }

}

// 00 00 00  06 C1  FF FC 34 B9 FF : onCaptionInfo.
libcaption_stauts_t cea708_parse_h264(const uint8_t* data, size_t size, cea708_t* cea708)
{

	if (3 > size) {
        goto error;
    }

    // I think the first few bytes need to be handled in mpeg
    cea708->country = (itu_t_t35_country_code_t)(data[0]);
    cea708->provider = (itu_t_t35_provider_code_t)((data[1] << 8) | data[2]);
    cea708->user_identifier = 0;
    cea708->user_data_type_code = 0;
    data += 3, size -= 3;

    if (t35_provider_atsc == cea708->provider) {
        if (4 > size) {
            goto error;
        }

        cea708->user_identifier = ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
        data += 4, size -= 4;
    }

    // where country and provider are zero
    // Im not sure what this extra byte is. It sonly seesm to come up in onCaptionInfo
    // h264 spec seems to describe this
    if (0 == cea708->provider && 0 == cea708->country) {
        if (1 > size) {
            goto error;
        }

        data += 1, size -= 1;
    } else if (t35_provider_atsc == cea708->provider || t35_provider_direct_tv == cea708->provider) {
        if (1 > size) {
            goto error;
        }

        cea708->user_data_type_code = data[0];
        data += 1, size -= 1;
    }

    if (t35_provider_direct_tv == cea708->provider) {
        if (1 > size) {
            goto error;
        }

        cea708->directv_user_data_length = data[0];
        data += 1, size -= 1;
    }

    if (3 == cea708->user_data_type_code && 2 <= size) {
        cea708_parse_user_data_type_strcture(data, size, &cea708->user_data);
    } else if (4 == cea708->user_data_type_code) {
        // additional_CEA_608_data
    } else if (5 == cea708->user_data_type_code) {
        // luma_PAM_data
    } else {
        // ATSC_reserved_user_data
    }

    return LIBCAPTION_OK;
error:
    return LIBCAPTION_ERROR;
}



libcaption_stauts_t cea708_to_caption_frame(caption_frame_t* frame, cea708_t* cea708)
{
    int i, count = cea708->user_data.cc_count;
    libcaption_stauts_t status = LIBCAPTION_OK;

    //if (GA94 == cea708->user_identifier) {
        for (i = 0; i < count; ++i) {

        	int valid             = cea708->user_data.cc_data[i].cc_valid;
            cea708_cc_type_t type = cea708->user_data.cc_data[i].cc_type;
            uint16_t cc_data      = cea708->user_data.cc_data[i].cc_data;


            if (valid && cc_type_ntsc_cc_field_1 == type) {
            	//printf("cea708_to_caption_frame() count:%d\n",i);
                status = libcaption_status_update(status, caption_frame_decode(frame, cc_data, cea708->timestamp));
            }
        }
    //}

    return status;
}
