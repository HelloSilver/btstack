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

#define BTSTACK_FILE__ "hci_transport_android_hci.c"

#include "hci_transport_android_hci.h"

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_run_loop.h"
#include "hci.h"
#include "hci_transport.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

// Linux/Android HCI User Channel. Constants are duplicated so the NDK build
// does not depend on <bluetooth/hci.h>, which is not part of the NDK sysroot.
#ifndef AF_BLUETOOTH
#define AF_BLUETOOTH 31
#endif
#ifndef PF_BLUETOOTH
#define PF_BLUETOOTH AF_BLUETOOTH
#endif
#ifndef BTPROTO_HCI
#define BTPROTO_HCI 1
#endif
#ifndef HCI_CHANNEL_USER
#define HCI_CHANNEL_USER 1
#endif
#ifndef HCIDEVDOWN
#define HCIDEVDOWN _IOW('H', 202, int)
#endif

struct sockaddr_hci {
    sa_family_t    hci_family;
    unsigned short hci_dev;
    unsigned short hci_channel;
};

#if !defined(HCI_OUTGOING_PRE_BUFFER_SIZE) || (HCI_OUTGOING_PRE_BUFFER_SIZE == 0)
#error HCI_OUTGOING_PRE_BUFFER_SIZE not defined. Please update hci.h
#endif

static void (*hci_transport_android_hci_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

static btstack_data_source_t hci_transport_android_hci_data_source;

static int hci_transport_android_hci_fd = -1;
static int hci_transport_android_hci_device_id;

typedef enum {
    TX_OFF,
    TX_IDLE,
    TX_W4_PACKET_SENT,
} TX_STATE;

static TX_STATE hci_transport_android_hci_tx_state;

static const uint8_t * hci_transport_android_hci_write_data;
static int             hci_transport_android_hci_write_len;

static uint8_t hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + HCI_INCOMING_PACKET_BUFFER_SIZE + 1];
static uint8_t * hci_packet = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];

static int hci_transport_android_hci_try_dev_down(int device_id){
    int ctl = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    if (ctl < 0) {
        log_info("HCI control socket failed: %s", strerror(errno));
        return -1;
    }
    int res = ioctl(ctl, HCIDEVDOWN, device_id);
    if (res < 0) {
        log_info("HCIDEVDOWN hci%d: %s (ok if the device was already down or unused)", device_id, strerror(errno));
    }
    close(ctl);
    return res;
}

static void hci_transport_android_hci_process_write(btstack_data_source_t *ds){
    if (hci_transport_android_hci_write_len == 0) {
        return;
    }

    int res = (int) send(ds->source.fd, hci_transport_android_hci_write_data, (size_t) hci_transport_android_hci_write_len, 0);
    if (res < 0) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);
            return;
        }
        log_error("HCI User Channel send failed: %s", strerror(errno));
        return;
    }

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);
    hci_transport_android_hci_tx_state = TX_IDLE;
    hci_transport_android_hci_write_data = NULL;
    hci_transport_android_hci_write_len = 0;

    static const uint8_t packet_sent_event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0 };
    hci_transport_android_hci_packet_handler(HCI_EVENT_PACKET, (uint8_t *) &packet_sent_event[0], sizeof(packet_sent_event));
}

static void hci_transport_android_hci_process_read(btstack_data_source_t *ds){
    int bytes_read = (int) recv(ds->source.fd, hci_packet, HCI_INCOMING_PACKET_BUFFER_SIZE + 1, 0);
    if (bytes_read <= 0) {
        if ((bytes_read < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
            return;
        }
        log_error("HCI User Channel recv failed: %s", strerror(errno));
        return;
    }

    hci_transport_android_hci_packet_handler(hci_packet[0], &hci_packet[1], (uint16_t) (bytes_read - 1));
}

static void hci_transport_android_hci_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    if (ds->source.fd < 0) {
        return;
    }
    switch (callback_type) {
        case DATA_SOURCE_CALLBACK_READ:
            hci_transport_android_hci_process_read(ds);
            break;
        case DATA_SOURCE_CALLBACK_WRITE:
            hci_transport_android_hci_process_write(ds);
            break;
        default:
            break;
    }
}

static void hci_transport_android_hci_init(const void *transport_config){
    log_info("init");
    hci_transport_android_hci_device_id = 0;
    if (transport_config != NULL) {
        const hci_transport_config_android_hci_t * config = (const hci_transport_config_android_hci_t *) transport_config;
        hci_transport_android_hci_device_id = config->device_id;
    }
    hci_transport_android_hci_tx_state = TX_OFF;
    hci_transport_android_hci_fd = -1;
}

static int hci_transport_android_hci_open(void){
    log_info("open hci%d (HCI_CHANNEL_USER)", hci_transport_android_hci_device_id);

    // Best-effort: the kernel rejects HCI_CHANNEL_USER while the device is UP.
    hci_transport_android_hci_try_dev_down(hci_transport_android_hci_device_id);

    int fd = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    if (fd < 0) {
        log_error("socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI) failed: %s", strerror(errno));
        printf("Android HCI socket unavailable (%s). Kernel needs CONFIG_BT, and this process needs CAP_NET_ADMIN.\n", strerror(errno));
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    struct sockaddr_hci addr;
    memset(&addr, 0, sizeof(addr));
    addr.hci_family  = AF_BLUETOOTH;
    addr.hci_dev     = (unsigned short) hci_transport_android_hci_device_id;
    addr.hci_channel = HCI_CHANNEL_USER;

    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        log_error("bind HCI_CHANNEL_USER hci%d failed: %s", hci_transport_android_hci_device_id, strerror(errno));
        printf("bind(HCI_CHANNEL_USER, hci%d) failed: %s\n", hci_transport_android_hci_device_id, strerror(errno));
        printf("Stop Android Bluetooth first (e.g. 'svc bluetooth disable') and run as root.\n");
        close(fd);
        return -1;
    }

    hci_transport_android_hci_fd = fd;
    btstack_run_loop_set_data_source_fd(&hci_transport_android_hci_data_source, fd);
    btstack_run_loop_set_data_source_handler(&hci_transport_android_hci_data_source, &hci_transport_android_hci_process);
    btstack_run_loop_add_data_source(&hci_transport_android_hci_data_source);
    btstack_run_loop_enable_data_source_callbacks(&hci_transport_android_hci_data_source, DATA_SOURCE_CALLBACK_READ);

    hci_transport_android_hci_tx_state = TX_IDLE;
    return 0;
}

static int hci_transport_android_hci_close(void){
    if (hci_transport_android_hci_fd >= 0) {
        btstack_run_loop_remove_data_source(&hci_transport_android_hci_data_source);
        close(hci_transport_android_hci_fd);
        hci_transport_android_hci_fd = -1;
    }
    hci_transport_android_hci_tx_state = TX_OFF;
    return 0;
}

static void hci_transport_android_hci_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    hci_transport_android_hci_packet_handler = handler;
}

static int hci_transport_android_hci_can_send_now(uint8_t packet_type){
    UNUSED(packet_type);
    return hci_transport_android_hci_tx_state == TX_IDLE;
}

static int hci_transport_android_hci_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    btstack_assert(hci_transport_android_hci_write_len == 0);

    uint8_t * buffer = &packet[-1];
    buffer[0] = packet_type;
    hci_transport_android_hci_write_data = buffer;
    hci_transport_android_hci_write_len  = size + 1;
    hci_transport_android_hci_tx_state = TX_W4_PACKET_SENT;

    btstack_run_loop_enable_data_source_callbacks(&hci_transport_android_hci_data_source, DATA_SOURCE_CALLBACK_WRITE);
    return 0;
}

static const hci_transport_t hci_transport_android_hci = {
    /* const char * name; */ "Android-HCI",
    /* void   (*init) (const void *transport_config); */ &hci_transport_android_hci_init,
    /* int    (*open)(void); */ &hci_transport_android_hci_open,
    /* int    (*close)(void); */ &hci_transport_android_hci_close,
    /* void   (*register_packet_handler)(void (*handler)(...)); */ &hci_transport_android_hci_register_packet_handler,
    /* int    (*can_send_packet_now)(uint8_t packet_type); */ &hci_transport_android_hci_can_send_now,
    /* int    (*send_packet)(uint8_t packet_type, uint8_t *packet, int size); */ &hci_transport_android_hci_send_packet,
    /* int    (*set_baudrate)(uint32_t baudrate); */ NULL,
    /* void   (*reset_link)(void); */ NULL,
    /* void   (*set_sco_config)(uint16_t voice_setting, int num_connections); */ NULL,
};

const hci_transport_t * hci_transport_android_hci_instance(void){
    return &hci_transport_android_hci;
}
