/*
 * Copyright (C) 2026 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "hci_dump_android_logcat.c"

#include "hci_dump_android_logcat.h"

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "hci.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include <stdio.h>
#include <string.h>

#ifndef ENABLE_PRINTF_HEXDUMP
#error "HCI Dump on logcat requires ENABLE_PRINTF_HEXDUMP. Add it to btstack_config.h"
#endif

#define ANDROID_LOGCAT_TAG "BTstack"

static char log_message_buffer[HCI_DUMP_MAX_MESSAGE_LEN];

static void android_logcat_print(int log_level, const char * message){
#ifdef __ANDROID__
    int priority = ANDROID_LOG_INFO;
    switch (log_level) {
        case HCI_DUMP_LOG_LEVEL_DEBUG:
            priority = ANDROID_LOG_DEBUG;
            break;
        case HCI_DUMP_LOG_LEVEL_ERROR:
            priority = ANDROID_LOG_ERROR;
            break;
        default:
            priority = ANDROID_LOG_INFO;
            break;
    }
    __android_log_write(priority, ANDROID_LOGCAT_TAG, message);
#else
    UNUSED(log_level);
    puts(message);
#endif
}

static void hci_dump_android_logcat_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len){
    const char * prefix;
    switch (packet_type) {
        case HCI_COMMAND_DATA_PACKET:
            prefix = "CMD =>";
            break;
        case HCI_EVENT_PACKET:
            prefix = "EVT <=";
            break;
        case HCI_ACL_DATA_PACKET:
            prefix = in ? "ACL <=" : "ACL =>";
            break;
        case HCI_SCO_DATA_PACKET:
            prefix = in ? "SCO <=" : "SCO =>";
            break;
        case HCI_ISO_DATA_PACKET:
            prefix = in ? "ISO <=" : "ISO =>";
            break;
        case LOG_MESSAGE_PACKET:
            android_logcat_print(HCI_DUMP_LOG_LEVEL_INFO, (const char *) packet);
            return;
        default:
            return;
    }

    char line[128];
    snprintf(line, sizeof(line), "%s len %u", prefix, len);
    android_logcat_print(HCI_DUMP_LOG_LEVEL_INFO, line);
    printf_hexdump(packet, len);
}

static void hci_dump_android_logcat_log_message(int log_level, const char * format, va_list argptr){
    int full_string_len = vsnprintf(log_message_buffer, sizeof(log_message_buffer), format, argptr);
    int len = (int) btstack_min(sizeof(log_message_buffer) - 1, (uint32_t) full_string_len);
    if (len < 0) {
        return;
    }
    log_message_buffer[len] = 0;
    android_logcat_print(log_level, log_message_buffer);
}

const hci_dump_t * hci_dump_android_logcat_get_instance(void){
    static const hci_dump_t hci_dump_instance = {
        NULL,
        &hci_dump_android_logcat_packet,
        &hci_dump_android_logcat_log_message,
    };
    return &hci_dump_instance;
}
