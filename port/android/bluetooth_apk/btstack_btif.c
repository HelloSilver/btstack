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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 */

#define BTSTACK_FILE__ "btstack_btif.c"

#include "btstack_btif.h"

#include "ad_parser.h"
#include "bluetooth.h"
#include "btstack_android.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"
#include "hci.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <android/log.h>
#define APK_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "btstack_apk", __VA_ARGS__)
#define APK_LOGW(...) __android_log_print(ANDROID_LOG_WARN, "btstack_apk", __VA_ARGS__)
#else
#define APK_LOGI(...) do { printf("btstack_apk: " __VA_ARGS__); printf("\n"); } while (0)
#define APK_LOGW(...) do { printf("btstack_apk: " __VA_ARGS__); printf("\n"); } while (0)
#endif

#ifndef BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME
#define BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME 0x09
#endif
#ifndef BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME
#define BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME 0x08
#endif

static bt_callbacks_t *btif_callbacks;
static bt_os_callouts_t *btif_callouts;
static int btif_inited;
static int btif_enabled;
static int btif_discovering;
static pthread_t btif_thread;
static int btif_thread_running;
static char btif_name[249] = "BTstack";
static uint32_t btif_discover_timeout_sec = 12;
static bt_scan_mode_t btif_scan_mode = BT_SCAN_MODE_CONNECTABLE;
static btstack_packet_callback_registration_t btif_hci_callback;

static void btif_emit_state(bt_state_t state){
    if ((btif_callbacks != NULL) && (btif_callbacks->adapter_state_changed_cb != NULL)) {
        btif_callbacks->adapter_state_changed_cb(state);
    }
}

static void btif_emit_discovery(bt_discovery_state_t state){
    if ((btif_callbacks != NULL) && (btif_callbacks->discovery_state_changed_cb != NULL)) {
        btif_callbacks->discovery_state_changed_cb(state);
    }
}

static void btif_fill_name(bt_bdname_t *out){
    memset(out, 0, sizeof(*out));
    strncpy((char *) out->name, btif_name, sizeof(out->name) - 1);
}

static void btif_emit_one_property(bt_property_type_t type, void *val, int len){
    bt_property_t prop;
    prop.type = type;
    prop.val = val;
    prop.len = len;
    if ((btif_callbacks != NULL) && (btif_callbacks->adapter_properties_cb != NULL)) {
        btif_callbacks->adapter_properties_cb(BT_STATUS_SUCCESS, 1, &prop);
    }
}

static void btif_emit_all_adapter_properties(void){
    RawAddress addr;
    bt_bdname_t name;
    uint32_t cod = 0x000000;
    uint32_t dtype = BT_DEVICE_DEVTYPE_DUAL;
    uint32_t timeout = btif_discover_timeout_sec;
    int32_t scan_mode = (int32_t) btif_scan_mode;
    bt_property_t props[7];
    int n = 0;

    gap_local_bd_addr(addr.address);
    btif_fill_name(&name);

    props[n].type = BT_PROPERTY_BDADDR;
    props[n].val = &addr;
    props[n].len = (int) sizeof(addr);
    n++;
    props[n].type = BT_PROPERTY_BDNAME;
    props[n].val = &name;
    props[n].len = (int) (strlen((char *) name.name) + 1);
    n++;
    props[n].type = BT_PROPERTY_CLASS_OF_DEVICE;
    props[n].val = &cod;
    props[n].len = (int) sizeof(cod);
    n++;
    props[n].type = BT_PROPERTY_TYPE_OF_DEVICE;
    props[n].val = &dtype;
    props[n].len = (int) sizeof(dtype);
    n++;
    props[n].type = BT_PROPERTY_ADAPTER_SCAN_MODE;
    props[n].val = &scan_mode;
    props[n].len = (int) sizeof(scan_mode);
    n++;
    props[n].type = BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT;
    props[n].val = &timeout;
    props[n].len = (int) sizeof(timeout);
    n++;
    props[n].type = BT_PROPERTY_ADAPTER_BONDED_DEVICES;
    props[n].val = NULL;
    props[n].len = 0;
    n++;

    if ((btif_callbacks != NULL) && (btif_callbacks->adapter_properties_cb != NULL)) {
        btif_callbacks->adapter_properties_cb(BT_STATUS_SUCCESS, n, props);
    }
}

static void btif_emit_device_found(const uint8_t *bd_addr, const char *name, int32_t rssi,
                                   uint32_t cod, uint32_t dtype){
    RawAddress addr;
    bt_bdname_t bdname;
    bt_property_t props[5];
    int n = 0;

    memcpy(addr.address, bd_addr, 6);
    memset(&bdname, 0, sizeof(bdname));
    if (name != NULL) {
        strncpy((char *) bdname.name, name, sizeof(bdname.name) - 1);
    }

    props[n].type = BT_PROPERTY_BDADDR;
    props[n].val = &addr;
    props[n].len = (int) sizeof(addr);
    n++;
    if (name != NULL) {
        props[n].type = BT_PROPERTY_BDNAME;
        props[n].val = &bdname;
        props[n].len = (int) (strlen((char *) bdname.name) + 1);
        n++;
    }
    props[n].type = BT_PROPERTY_REMOTE_RSSI;
    props[n].val = &rssi;
    props[n].len = (int) sizeof(rssi);
    n++;
    props[n].type = BT_PROPERTY_CLASS_OF_DEVICE;
    props[n].val = &cod;
    props[n].len = (int) sizeof(cod);
    n++;
    props[n].type = BT_PROPERTY_TYPE_OF_DEVICE;
    props[n].val = &dtype;
    props[n].len = (int) sizeof(dtype);
    n++;

    if ((btif_callbacks != NULL) && (btif_callbacks->device_found_cb != NULL)) {
        btif_callbacks->device_found_cb(n, props);
    }
}

static const char *btif_adv_name(const uint8_t *data, uint8_t len, char *out, size_t out_size){
    ad_context_t context;
    const char *found = NULL;
    ad_iterator_init(&context, len, data);
    while (ad_iterator_has_more(&context)) {
        uint8_t type = ad_iterator_get_data_type(&context);
        uint8_t dlen = ad_iterator_get_data_len(&context);
        const uint8_t *d = ad_iterator_get_data(&context);
        if ((type == BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME) ||
            (type == BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME)) {
            size_t copy = dlen;
            if (copy >= out_size) {
                copy = out_size - 1;
            }
            memcpy(out, d, copy);
            out[copy] = 0;
            found = out;
            if (type == BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME) {
                break;
            }
        }
        ad_iterator_next(&context);
    }
    return found;
}

static void btif_hci_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t addr;
    char name_buf[249];
    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }
    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            switch (btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
                    btif_enabled = 1;
                    APK_LOGI("HCI_STATE_WORKING");
                    btif_emit_state(BT_STATE_ON);
                    btif_emit_all_adapter_properties();
                    break;
                case HCI_STATE_OFF:
                    btif_enabled = 0;
                    btif_discovering = 0;
                    APK_LOGI("HCI_STATE_OFF");
                    btif_emit_state(BT_STATE_OFF);
                    break;
                default:
                    break;
            }
            break;
        case GAP_EVENT_INQUIRY_RESULT:
            gap_event_inquiry_result_get_bd_addr(packet, addr);
            {
                const char *nm = NULL;
                if (gap_event_inquiry_result_get_name_available(packet)) {
                    uint8_t nlen = gap_event_inquiry_result_get_name_len(packet);
                    if (nlen >= sizeof(name_buf)) {
                        nlen = (uint8_t) (sizeof(name_buf) - 1);
                    }
                    memcpy(name_buf, gap_event_inquiry_result_get_name(packet), nlen);
                    name_buf[nlen] = 0;
                    nm = name_buf;
                }
                int32_t rssi = gap_event_inquiry_result_get_rssi_available(packet)
                                   ? gap_event_inquiry_result_get_rssi(packet)
                                   : 0;
                btif_emit_device_found(addr, nm, rssi,
                                       gap_event_inquiry_result_get_class_of_device(packet),
                                       BT_DEVICE_DEVTYPE_BREDR);
            }
            break;
        case GAP_EVENT_INQUIRY_COMPLETE:
            gap_stop_scan();
            btif_discovering = 0;
            btif_emit_discovery(BT_DISCOVERY_STOPPED);
            break;
        case GAP_EVENT_ADVERTISING_REPORT:
            gap_event_advertising_report_get_address(packet, addr);
            {
                char nbuf[249];
                const char *nm = btif_adv_name(gap_event_advertising_report_get_data(packet),
                                               gap_event_advertising_report_get_data_length(packet),
                                               nbuf, sizeof(nbuf));
                btif_emit_device_found(addr, nm, gap_event_advertising_report_get_rssi(packet),
                                       0, BT_DEVICE_DEVTYPE_BLE);
            }
            break;
        default:
            break;
    }
}

static void btif_do_power_on(void *context){
    UNUSED(context);
    hci_power_control(HCI_POWER_ON);
}

static void btif_do_power_off(void *context){
    UNUSED(context);
    hci_power_control(HCI_POWER_OFF);
}

static void btif_do_get_all_props(void *context){
    UNUSED(context);
    btif_emit_all_adapter_properties();
}

static void btif_do_start_discovery(void *context){
    UNUSED(context);
    uint8_t duration = (uint8_t) ((btif_discover_timeout_sec + 1) / 2);
    if (duration < 1) {
        duration = 8;
    }
    gap_inquiry_start(duration);
    gap_start_scan();
    btif_discovering = 1;
    btif_emit_discovery(BT_DISCOVERY_STARTED);
}

static void btif_do_cancel_discovery(void *context){
    UNUSED(context);
    gap_inquiry_stop();
    gap_stop_scan();
    btif_discovering = 0;
    btif_emit_discovery(BT_DISCOVERY_STOPPED);
}

static void btif_post(void (*fn)(void *), btstack_context_callback_registration_t *reg){
    reg->callback = fn;
    reg->context = NULL;
    btstack_run_loop_execute_on_main_thread(reg);
}

static btstack_context_callback_registration_t reg_on;
static btstack_context_callback_registration_t reg_off;
static btstack_context_callback_registration_t reg_props;
static btstack_context_callback_registration_t reg_disc;
static btstack_context_callback_registration_t reg_cancel;

static void *btif_thread_fn(void *arg){
    UNUSED(arg);
    if ((btif_callbacks != NULL) && (btif_callbacks->thread_evt_cb != NULL)) {
        btif_callbacks->thread_evt_cb(ASSOCIATE_JVM);
    }
    APK_LOGI("BTstack run loop starting");
    btstack_android_run();
    if ((btif_callbacks != NULL) && (btif_callbacks->thread_evt_cb != NULL)) {
        btif_callbacks->thread_evt_cb(DISASSOCIATE_JVM);
    }
    btif_thread_running = 0;
    return NULL;
}

int btstack_btif_init(bt_callbacks_t *callbacks, const char *user_data_directory){
    const char *transport;
    const char *instance;
    const char *argv_storage[8];
    int argc = 0;

    if (btif_inited) {
        btif_callbacks = callbacks;
        return BT_STATUS_DONE;
    }

    btif_callbacks = callbacks;
    UNUSED(user_data_directory);

    transport = getenv("BTSTACK_TRANSPORT");
    if ((transport == NULL) || (transport[0] == 0)) {
        transport = "aidl";
    }
    instance = getenv("BTSTACK_AIDL_INSTANCE");
    if ((instance == NULL) || (instance[0] == 0)) {
        instance = "default";
    }

    argv_storage[argc++] = "btstack";
    argv_storage[argc++] = "-t";
    argv_storage[argc++] = transport;
    if (strcmp(transport, "hci") == 0) {
        const char *dev = getenv("BTSTACK_HCI_DEV");
        argv_storage[argc++] = "-d";
        argv_storage[argc++] = (dev != NULL) ? dev : "0";
    } else if (strcmp(transport, "h4") == 0) {
        const char *tty = getenv("BTSTACK_H4_TTY");
        argv_storage[argc++] = "-u";
        argv_storage[argc++] = (tty != NULL) ? tty : "/dev/ttyUSB0";
    } else {
        argv_storage[argc++] = "-i";
        argv_storage[argc++] = instance;
    }
    argv_storage[argc++] = "-c";

    APK_LOGI("btstack_android_init transport=%s", transport);
    if (btstack_android_init(argc, argv_storage) != 0) {
        APK_LOGW("btstack_android_init failed");
        return BT_STATUS_FAIL;
    }

    gap_set_local_name(btif_name);
    btif_hci_callback.callback = &btif_hci_packet_handler;
    hci_add_event_handler(&btif_hci_callback);
    btif_inited = 1;
    return BT_STATUS_SUCCESS;
}

int btstack_btif_enable(void){
    if (!btif_inited) {
        return BT_STATUS_NOT_READY;
    }
    if (btif_enabled) {
        return BT_STATUS_DONE;
    }
    if (!btif_thread_running) {
        btif_thread_running = 1;
        if (pthread_create(&btif_thread, NULL, &btif_thread_fn, NULL) != 0) {
            btif_thread_running = 0;
            APK_LOGW("pthread_create failed");
            return BT_STATUS_FAIL;
        }
        pthread_detach(btif_thread);
    }
    btif_post(&btif_do_power_on, &reg_on);
    return BT_STATUS_SUCCESS;
}

int btstack_btif_disable(void){
    if (!btif_inited) {
        return BT_STATUS_NOT_READY;
    }
    if (btif_discovering) {
        btif_post(&btif_do_cancel_discovery, &reg_cancel);
    }
    btif_post(&btif_do_power_off, &reg_off);
    return BT_STATUS_SUCCESS;
}

void btstack_btif_cleanup(void){
    if (!btif_inited) {
        return;
    }
    (void) btstack_btif_disable();
    btif_callbacks = NULL;
}

int btstack_btif_get_adapter_properties(void){
    if (!btif_inited) {
        return BT_STATUS_NOT_READY;
    }
    btif_post(&btif_do_get_all_props, &reg_props);
    return BT_STATUS_SUCCESS;
}

int btstack_btif_get_adapter_property(bt_property_type_t type){
    RawAddress addr;
    bt_bdname_t name;
    uint32_t u32;
    int32_t i32;
    if (!btif_inited) {
        return BT_STATUS_NOT_READY;
    }
    switch (type) {
        case BT_PROPERTY_BDADDR:
            gap_local_bd_addr(addr.address);
            btif_emit_one_property(type, &addr, (int) sizeof(addr));
            break;
        case BT_PROPERTY_BDNAME:
            btif_fill_name(&name);
            btif_emit_one_property(type, &name, (int) (strlen((char *) name.name) + 1));
            break;
        case BT_PROPERTY_ADAPTER_SCAN_MODE:
            i32 = (int32_t) btif_scan_mode;
            btif_emit_one_property(type, &i32, (int) sizeof(i32));
            break;
        case BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT:
            u32 = btif_discover_timeout_sec;
            btif_emit_one_property(type, &u32, (int) sizeof(u32));
            break;
        case BT_PROPERTY_TYPE_OF_DEVICE:
            u32 = BT_DEVICE_DEVTYPE_DUAL;
            btif_emit_one_property(type, &u32, (int) sizeof(u32));
            break;
        case BT_PROPERTY_ADAPTER_BONDED_DEVICES:
            btif_emit_one_property(type, NULL, 0);
            break;
        default:
            APK_LOGW("get_adapter_property stub type=%d", (int) type);
            return BT_STATUS_UNSUPPORTED;
    }
    return BT_STATUS_SUCCESS;
}

int btstack_btif_set_adapter_property(const bt_property_t *property){
    if ((property == NULL) || (property->val == NULL)) {
        return BT_STATUS_PARM_INVALID;
    }
    switch (property->type) {
        case BT_PROPERTY_BDNAME:
            memset(btif_name, 0, sizeof(btif_name));
            strncpy(btif_name, (const char *) property->val, sizeof(btif_name) - 1);
            gap_set_local_name(btif_name);
            btif_emit_one_property(BT_PROPERTY_BDNAME, property->val, property->len);
            return BT_STATUS_SUCCESS;
        case BT_PROPERTY_ADAPTER_SCAN_MODE:
            if (property->len >= (int) sizeof(int32_t)) {
                btif_scan_mode = (bt_scan_mode_t) (*(const int32_t *) property->val);
                gap_connectable_control(btif_scan_mode != BT_SCAN_MODE_NONE);
                gap_discoverable_control(btif_scan_mode >= BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
            }
            btif_emit_one_property(property->type, property->val, property->len);
            return BT_STATUS_SUCCESS;
        case BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT:
            if (property->len >= (int) sizeof(uint32_t)) {
                btif_discover_timeout_sec = *(const uint32_t *) property->val;
            }
            btif_emit_one_property(property->type, property->val, property->len);
            return BT_STATUS_SUCCESS;
        default:
            APK_LOGW("set_adapter_property stub type=%d", (int) property->type);
            return BT_STATUS_UNSUPPORTED;
    }
}

int btstack_btif_start_discovery(void){
    if (!btif_enabled) {
        return BT_STATUS_NOT_READY;
    }
    btif_post(&btif_do_start_discovery, &reg_disc);
    return BT_STATUS_SUCCESS;
}

int btstack_btif_cancel_discovery(void){
    if (!btif_inited) {
        return BT_STATUS_NOT_READY;
    }
    btif_post(&btif_do_cancel_discovery, &reg_cancel);
    return BT_STATUS_SUCCESS;
}

int btstack_btif_set_os_callouts(bt_os_callouts_t *callouts){
    btif_callouts = callouts;
    return BT_STATUS_SUCCESS;
}

const void *btstack_btif_get_profile_interface(const char *profile_id){
    APK_LOGW("get_profile_interface(\"%s\") stub — not a Fluoride profile clone",
             profile_id ? profile_id : "(null)");
    return NULL;
}
