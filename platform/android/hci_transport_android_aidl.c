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

#define BTSTACK_FILE__ "hci_transport_android_aidl.c"

#include "hci_transport_android_aidl.h"

#include "bluetooth.h"
#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_run_loop.h"
#include "btstack_util.h"
#include "hci.h"
#include "hci_transport.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ANDROID__) && defined(HAVE_ANDROID_BINDER_NDK)

#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>

#include <dlfcn.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

// Vendored snapshots: platform/android/aidl/android/hardware/bluetooth/*.aidl
// from AOSP android14-release hardware/interfaces/bluetooth/aidl.
// Transaction codes follow AIDL source order (FIRST_CALL_TRANSACTION + index),
// matching the generated Java Stub used by AOSP extras/android/RemoteHCI.
#define AIDL_HCI_DESCRIPTOR            "android.hardware.bluetooth.IBluetoothHci"
#define AIDL_HCI_CALLBACKS_DESCRIPTOR  "android.hardware.bluetooth.IBluetoothHciCallbacks"
#define AIDL_HCI_SERVICE_PREFIX        "android.hardware.bluetooth.IBluetoothHci/"

#define AIDL_TXN_CLOSE                 ((transaction_code_t) (FIRST_CALL_TRANSACTION + 0))
#define AIDL_TXN_INITIALIZE            ((transaction_code_t) (FIRST_CALL_TRANSACTION + 1))
#define AIDL_TXN_SEND_ACL_DATA         ((transaction_code_t) (FIRST_CALL_TRANSACTION + 2))
#define AIDL_TXN_SEND_HCI_COMMAND      ((transaction_code_t) (FIRST_CALL_TRANSACTION + 3))
#define AIDL_TXN_SEND_ISO_DATA         ((transaction_code_t) (FIRST_CALL_TRANSACTION + 4))
#define AIDL_TXN_SEND_SCO_DATA         ((transaction_code_t) (FIRST_CALL_TRANSACTION + 5))

#define AIDL_CB_ACL_DATA_RECEIVED      ((transaction_code_t) (FIRST_CALL_TRANSACTION + 0))
#define AIDL_CB_HCI_EVENT_RECEIVED     ((transaction_code_t) (FIRST_CALL_TRANSACTION + 1))
#define AIDL_CB_INITIALIZATION_COMPLETE ((transaction_code_t) (FIRST_CALL_TRANSACTION + 2))
#define AIDL_CB_ISO_DATA_RECEIVED      ((transaction_code_t) (FIRST_CALL_TRANSACTION + 3))
#define AIDL_CB_SCO_DATA_RECEIVED      ((transaction_code_t) (FIRST_CALL_TRANSACTION + 4))

// android.hardware.bluetooth.Status (Status.aidl, @Backing(type="int"))
#define AIDL_STATUS_SUCCESS                    0
#define AIDL_STATUS_ALREADY_INITIALIZED        1
#define AIDL_STATUS_UNABLE_TO_OPEN_INTERFACE   2
#define AIDL_STATUS_HARDWARE_INITIALIZATION_ERROR 3
#define AIDL_STATUS_UNKNOWN                    4

#define AIDL_INIT_TIMEOUT_SEC  5
#define AIDL_SERVICE_POLL_MS   100
#define AIDL_SERVICE_POLLS     50

// Public NDK libbinder_ndk stubs (API 34) export AIBinder_*/AParcel_* but not
// AServiceManager_* / ABinderProcess_*. Resolve those from the device .so.
typedef AIBinder *(*aidl_wait_for_service_t)(const char *instance);
typedef AIBinder *(*aidl_check_service_t)(const char *instance);
typedef void (*aidl_start_thread_pool_t)(void);
typedef bool (*aidl_set_thread_pool_max_t)(uint32_t num_threads);

typedef struct {
    uint8_t *data;
    int32_t len;
} aidl_byte_array_t;

typedef struct aidl_rx_item {
    struct aidl_rx_item *next;
    uint8_t packet_type;
    uint16_t len;
    uint8_t data[1];
} aidl_rx_item_t;

static void (*hci_transport_android_aidl_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

static char aidl_instance_arg[256];
static char aidl_service_name[320];
static int aidl_opened;

static AIBinder_Class *aidl_hci_class;
static AIBinder_Class *aidl_callbacks_class;
static AIBinder *aidl_hci_binder;
static AIBinder *aidl_callbacks_binder;
static AIBinder_DeathRecipient *aidl_death_recipient;

static pthread_mutex_t aidl_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  aidl_init_cond = PTHREAD_COND_INITIALIZER;
static int aidl_init_done;
static int32_t aidl_init_status;

static aidl_rx_item_t *aidl_rx_head;
static aidl_rx_item_t *aidl_rx_tail;
static int aidl_drain_scheduled;

static btstack_context_callback_registration_t aidl_drain_registration;

static void hci_transport_android_aidl_drain(void *context);

static aidl_wait_for_service_t aidl_wait_for_service;
static aidl_check_service_t aidl_check_service;
static aidl_start_thread_pool_t aidl_start_thread_pool;
static aidl_set_thread_pool_max_t aidl_set_thread_pool_max;
static int aidl_binder_syms_resolved;

static const char *aidl_status_name(int32_t status){
    switch (status) {
        case AIDL_STATUS_SUCCESS: return "SUCCESS";
        case AIDL_STATUS_ALREADY_INITIALIZED: return "ALREADY_INITIALIZED";
        case AIDL_STATUS_UNABLE_TO_OPEN_INTERFACE: return "UNABLE_TO_OPEN_INTERFACE";
        case AIDL_STATUS_HARDWARE_INITIALIZATION_ERROR: return "HARDWARE_INITIALIZATION_ERROR";
        case AIDL_STATUS_UNKNOWN: return "UNKNOWN";
        default: return "invalid";
    }
}

static int aidl_resolve_binder_manager(void){
    if (aidl_binder_syms_resolved) {
        return (aidl_wait_for_service != NULL) || (aidl_check_service != NULL);
    }
    aidl_binder_syms_resolved = 1;

    void *handle = dlopen("libbinder_ndk.so", RTLD_NOW | RTLD_GLOBAL);
    if (handle == NULL) {
        log_error("dlopen(libbinder_ndk.so) failed: %s", dlerror());
        handle = RTLD_DEFAULT;
    }

    aidl_wait_for_service = (aidl_wait_for_service_t) dlsym(handle, "AServiceManager_waitForService");
    aidl_check_service = (aidl_check_service_t) dlsym(handle, "AServiceManager_checkService");
    aidl_start_thread_pool = (aidl_start_thread_pool_t) dlsym(handle, "ABinderProcess_startThreadPool");
    aidl_set_thread_pool_max = (aidl_set_thread_pool_max_t) dlsym(handle, "ABinderProcess_setThreadPoolMaxThreadCount");

    if ((aidl_wait_for_service == NULL) && (aidl_check_service == NULL)) {
        log_error("libbinder_ndk has no AServiceManager_* (NDK stub only). Need a device image.");
        return 0;
    }
    return 1;
}

static void aidl_build_service_name(void){
    const char *instance = aidl_instance_arg;
    if ((instance == NULL) || (instance[0] == 0)) {
        instance = "default";
    }
    if (strchr(instance, '/') != NULL) {
        btstack_strcpy(aidl_service_name, sizeof(aidl_service_name), instance);
    } else {
        btstack_strcpy(aidl_service_name, sizeof(aidl_service_name), AIDL_HCI_SERVICE_PREFIX);
        btstack_strcat(aidl_service_name, sizeof(aidl_service_name), instance);
    }
}

static bool aidl_byte_array_allocator(void *arrayData, int32_t length, int8_t **outBuffer){
    aidl_byte_array_t *arr = (aidl_byte_array_t *) arrayData;
    arr->data = NULL;
    arr->len = 0;
    if (length < 0) {
        return true;
    }
    if (length > HCI_INCOMING_PACKET_BUFFER_SIZE) {
        log_error("IBluetoothHci callback packet too large: %d", (int) length);
        return false;
    }
    if (length == 0) {
        *outBuffer = NULL;
        return true;
    }
    arr->data = (uint8_t *) malloc((size_t) length);
    if (arr->data == NULL) {
        return false;
    }
    arr->len = length;
    *outBuffer = (int8_t *) arr->data;
    return true;
}

static binder_status_t aidl_write_ok(AParcel *out){
    AStatus *ok = AStatus_newOk();
    if (ok == NULL) {
        return STATUS_NO_MEMORY;
    }
    binder_status_t status = AParcel_writeStatusHeader(out, ok);
    AStatus_delete(ok);
    return status;
}

static binder_status_t aidl_transact_void(AIBinder *binder, transaction_code_t code){
    AParcel *in = NULL;
    AParcel *out = NULL;
    binder_status_t status = AIBinder_prepareTransaction(binder, &in);
    if (status != STATUS_OK) {
        return status;
    }
    status = AIBinder_transact(binder, code, &in, &out, 0);
    if ((status == STATUS_OK) && (out != NULL)) {
        AStatus *reply = NULL;
        if (AParcel_readStatusHeader(out, &reply) == STATUS_OK) {
            if ((reply != NULL) && !AStatus_isOk(reply)) {
                log_error("IBluetoothHci txn %u failed: %s", (unsigned) code,
                          AStatus_getMessage(reply));
                status = STATUS_FAILED_TRANSACTION;
            }
            AStatus_delete(reply);
        }
    }
    if (out != NULL) {
        AParcel_delete(out);
    }
    return status;
}

static binder_status_t aidl_transact_bytes(AIBinder *binder, transaction_code_t code,
                                           const uint8_t *data, int size){
    AParcel *in = NULL;
    AParcel *out = NULL;
    binder_status_t status = AIBinder_prepareTransaction(binder, &in);
    if (status != STATUS_OK) {
        return status;
    }
    status = AParcel_writeByteArray(in, (const int8_t *) data, size);
    if (status != STATUS_OK) {
        AParcel_delete(in);
        return status;
    }
    status = AIBinder_transact(binder, code, &in, &out, 0);
    if ((status == STATUS_OK) && (out != NULL)) {
        AStatus *reply = NULL;
        if (AParcel_readStatusHeader(out, &reply) == STATUS_OK) {
            if ((reply != NULL) && !AStatus_isOk(reply)) {
                log_error("IBluetoothHci send txn %u failed: %s", (unsigned) code,
                          AStatus_getMessage(reply));
                status = STATUS_FAILED_TRANSACTION;
            }
            AStatus_delete(reply);
        }
    }
    if (out != NULL) {
        AParcel_delete(out);
    }
    return status;
}

static binder_status_t aidl_transact_initialize(AIBinder *hci, AIBinder *callbacks){
    AParcel *in = NULL;
    AParcel *out = NULL;
    binder_status_t status = AIBinder_prepareTransaction(hci, &in);
    if (status != STATUS_OK) {
        return status;
    }
    status = AParcel_writeStrongBinder(in, callbacks);
    if (status != STATUS_OK) {
        AParcel_delete(in);
        return status;
    }
    status = AIBinder_transact(hci, AIDL_TXN_INITIALIZE, &in, &out, 0);
    if ((status == STATUS_OK) && (out != NULL)) {
        AStatus *reply = NULL;
        if (AParcel_readStatusHeader(out, &reply) == STATUS_OK) {
            if ((reply != NULL) && !AStatus_isOk(reply)) {
                log_error("IBluetoothHci.initialize failed: %s", AStatus_getMessage(reply));
                status = STATUS_FAILED_TRANSACTION;
            }
            AStatus_delete(reply);
        }
    }
    if (out != NULL) {
        AParcel_delete(out);
    }
    return status;
}

static void aidl_enqueue_packet(uint8_t packet_type, uint8_t *data, int32_t len){
    if ((data == NULL) || (len <= 0)) {
        free(data);
        return;
    }

    size_t item_size = sizeof(aidl_rx_item_t) + (size_t) len - 1u;
    aidl_rx_item_t *item = (aidl_rx_item_t *) malloc(item_size);
    if (item == NULL) {
        log_error("AIDL RX malloc failed");
        free(data);
        return;
    }
    item->next = NULL;
    item->packet_type = packet_type;
    item->len = (uint16_t) len;
    memcpy(item->data, data, (size_t) len);
    free(data);

    int schedule = 0;
    pthread_mutex_lock(&aidl_mutex);
    if (!aidl_opened) {
        pthread_mutex_unlock(&aidl_mutex);
        free(item);
        return;
    }
    if (aidl_rx_tail != NULL) {
        aidl_rx_tail->next = item;
    } else {
        aidl_rx_head = item;
    }
    aidl_rx_tail = item;
    if (!aidl_drain_scheduled) {
        aidl_drain_scheduled = 1;
        schedule = 1;
    }
    pthread_mutex_unlock(&aidl_mutex);

    if (schedule) {
        aidl_drain_registration.callback = &hci_transport_android_aidl_drain;
        aidl_drain_registration.context = NULL;
        btstack_run_loop_execute_on_main_thread(&aidl_drain_registration);
    }
}

static void hci_transport_android_aidl_drain(void *context){
    UNUSED(context);
    while (1) {
        pthread_mutex_lock(&aidl_mutex);
        aidl_rx_item_t *item = aidl_rx_head;
        if (item == NULL) {
            aidl_drain_scheduled = 0;
            pthread_mutex_unlock(&aidl_mutex);
            return;
        }
        aidl_rx_head = item->next;
        if (aidl_rx_head == NULL) {
            aidl_rx_tail = NULL;
        }
        pthread_mutex_unlock(&aidl_mutex);

        if (hci_transport_android_aidl_packet_handler != NULL) {
            hci_transport_android_aidl_packet_handler(item->packet_type, item->data, item->len);
        }
        free(item);
    }
}

static void aidl_free_rx_queue(void){
    aidl_rx_item_t *item = aidl_rx_head;
    aidl_rx_head = NULL;
    aidl_rx_tail = NULL;
    aidl_drain_scheduled = 0;
    while (item != NULL) {
        aidl_rx_item_t *next = item->next;
        free(item);
        item = next;
    }
}

static void aidl_on_binder_died(void *cookie){
    UNUSED(cookie);
    log_error("IBluetoothHci HAL died");
    printf("IBluetoothHci HAL died (android.hardware.bluetooth).\n");
}

static void *aidl_class_on_create(void *args){
    return args;
}

static void aidl_class_on_destroy(void *userData){
    UNUSED(userData);
}

static binder_status_t aidl_hci_on_transact(AIBinder *binder, transaction_code_t code,
                                            const AParcel *in, AParcel *out){
    UNUSED(binder);
    UNUSED(code);
    UNUSED(in);
    UNUSED(out);
    return STATUS_UNKNOWN_TRANSACTION;
}

static binder_status_t aidl_callbacks_on_transact(AIBinder *binder, transaction_code_t code,
                                                  const AParcel *in, AParcel *out){
    UNUSED(binder);

    if (code == AIDL_CB_INITIALIZATION_COMPLETE) {
        int32_t status_value = AIDL_STATUS_UNKNOWN;
        binder_status_t status = AParcel_readInt32(in, &status_value);
        if (status != STATUS_OK) {
            return status;
        }
        log_info("IBluetoothHciCallbacks.initializationComplete(%s)", aidl_status_name(status_value));
        pthread_mutex_lock(&aidl_mutex);
        aidl_init_status = status_value;
        aidl_init_done = 1;
        pthread_cond_signal(&aidl_init_cond);
        pthread_mutex_unlock(&aidl_mutex);
        return aidl_write_ok(out);
    }

    uint8_t packet_type;
    switch (code) {
        case AIDL_CB_ACL_DATA_RECEIVED:
            packet_type = HCI_ACL_DATA_PACKET;
            break;
        case AIDL_CB_HCI_EVENT_RECEIVED:
            packet_type = HCI_EVENT_PACKET;
            break;
        case AIDL_CB_ISO_DATA_RECEIVED:
            packet_type = HCI_ISO_DATA_PACKET;
            break;
        case AIDL_CB_SCO_DATA_RECEIVED:
            packet_type = HCI_SCO_DATA_PACKET;
            break;
        default:
            return STATUS_UNKNOWN_TRANSACTION;
    }

    // HCI payloads are Core Spec Vol 2 Part 5 (no H4 packet-indicator byte).
    aidl_byte_array_t bytes;
    memset(&bytes, 0, sizeof(bytes));
    binder_status_t status = AParcel_readByteArray(in, &bytes, &aidl_byte_array_allocator);
    if (status != STATUS_OK) {
        free(bytes.data);
        return status;
    }
    aidl_enqueue_packet(packet_type, bytes.data, bytes.len);
    return aidl_write_ok(out);
}

static int aidl_ensure_classes(void){
    if (aidl_hci_class == NULL) {
        aidl_hci_class = AIBinder_Class_define(AIDL_HCI_DESCRIPTOR, &aidl_class_on_create,
                                               &aidl_class_on_destroy, &aidl_hci_on_transact);
        if (aidl_hci_class == NULL) {
            log_error("AIBinder_Class_define(%s) failed", AIDL_HCI_DESCRIPTOR);
            return -1;
        }
    }
    if (aidl_callbacks_class == NULL) {
        aidl_callbacks_class = AIBinder_Class_define(AIDL_HCI_CALLBACKS_DESCRIPTOR,
                                                     &aidl_class_on_create, &aidl_class_on_destroy,
                                                     &aidl_callbacks_on_transact);
        if (aidl_callbacks_class == NULL) {
            log_error("AIBinder_Class_define(%s) failed", AIDL_HCI_CALLBACKS_DESCRIPTOR);
            return -1;
        }
    }
    return 0;
}

static AIBinder *aidl_lookup_service(void){
    AIBinder *binder = NULL;
    unsigned i;
    for (i = 0; (i < AIDL_SERVICE_POLLS) && (binder == NULL); i++) {
        if (aidl_check_service != NULL) {
            binder = aidl_check_service(aidl_service_name);
        }
        if (binder != NULL) {
            break;
        }
        if ((i == 0) && (aidl_wait_for_service != NULL)) {
            printf("Waiting for %s ...\n", aidl_service_name);
            binder = aidl_wait_for_service(aidl_service_name);
            break;
        }
        usleep(AIDL_SERVICE_POLL_MS * 1000);
    }
    return binder;
}

static int aidl_wait_init_complete(void){
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        ts.tv_sec = time(NULL);
        ts.tv_nsec = 0;
    }
    ts.tv_sec += AIDL_INIT_TIMEOUT_SEC;

    pthread_mutex_lock(&aidl_mutex);
    while (!aidl_init_done) {
        int rc = pthread_cond_timedwait(&aidl_init_cond, &aidl_mutex, &ts);
        if (rc != 0) {
            pthread_mutex_unlock(&aidl_mutex);
            return -1;
        }
    }
    int32_t status = aidl_init_status;
    pthread_mutex_unlock(&aidl_mutex);
    return status;
}

static void hci_transport_android_aidl_init(const void *transport_config){
    log_info("init");
    btstack_strcpy(aidl_instance_arg, sizeof(aidl_instance_arg), "default");
    if (transport_config != NULL) {
        const hci_transport_config_android_aidl_t *config =
            (const hci_transport_config_android_aidl_t *) transport_config;
        if ((config->instance != NULL) && (config->instance[0] != 0)) {
            btstack_strcpy(aidl_instance_arg, sizeof(aidl_instance_arg), config->instance);
        }
    }
    aidl_build_service_name();
}

static void hci_transport_android_aidl_release_binders(void){
    if ((aidl_hci_binder != NULL) && (aidl_death_recipient != NULL)) {
        (void) AIBinder_unlinkToDeath(aidl_hci_binder, aidl_death_recipient, NULL);
    }
    if (aidl_death_recipient != NULL) {
        AIBinder_DeathRecipient_delete(aidl_death_recipient);
        aidl_death_recipient = NULL;
    }
    if (aidl_callbacks_binder != NULL) {
        AIBinder_decStrong(aidl_callbacks_binder);
        aidl_callbacks_binder = NULL;
    }
    if (aidl_hci_binder != NULL) {
        AIBinder_decStrong(aidl_hci_binder);
        aidl_hci_binder = NULL;
    }
}

static int hci_transport_android_aidl_open(void){
    log_info("open %s", aidl_service_name);
    printf("HCI transport: Android 14 IBluetoothHci AIDL (%s)\n", aidl_service_name);
    printf("Only one client may initialize this HAL. Stop com.android.bluetooth first.\n");

    if (!aidl_resolve_binder_manager()) {
        printf("AServiceManager_* not available. This NDK stub cannot look up the HAL.\n");
        printf("Run on a device/image whose libbinder_ndk.so exports AServiceManager_waitForService.\n");
        return -1;
    }
    if (aidl_ensure_classes() != 0) {
        return -1;
    }

    if (aidl_set_thread_pool_max != NULL) {
        (void) aidl_set_thread_pool_max(4);
    }
    if (aidl_start_thread_pool != NULL) {
        aidl_start_thread_pool();
    } else {
        log_info("ABinderProcess_startThreadPool missing; incoming callbacks may deadlock");
    }

    aidl_hci_binder = aidl_lookup_service();
    if (aidl_hci_binder == NULL) {
        log_error("service %s not found", aidl_service_name);
        printf("Could not bind %s.\n", aidl_service_name);
        printf("Need bluetooth UID or root, HAL access, and system Bluetooth stopped.\n");
        printf("A normal Play Store app cannot open this HAL.\n");
        return -1;
    }

    if (!AIBinder_associateClass(aidl_hci_binder, aidl_hci_class)) {
        log_error("AIBinder_associateClass(%s) failed", AIDL_HCI_DESCRIPTOR);
        printf("Binder is not %s.\n", AIDL_HCI_DESCRIPTOR);
        hci_transport_android_aidl_release_binders();
        return -1;
    }

    aidl_callbacks_binder = AIBinder_new(aidl_callbacks_class, NULL);
    if (aidl_callbacks_binder == NULL) {
        log_error("AIBinder_new(IBluetoothHciCallbacks) failed");
        hci_transport_android_aidl_release_binders();
        return -1;
    }

    aidl_death_recipient = AIBinder_DeathRecipient_new(&aidl_on_binder_died);
    if (aidl_death_recipient != NULL) {
        (void) AIBinder_linkToDeath(aidl_hci_binder, aidl_death_recipient, NULL);
    }

    pthread_mutex_lock(&aidl_mutex);
    aidl_init_done = 0;
    aidl_init_status = AIDL_STATUS_UNKNOWN;
    aidl_opened = 1;
    pthread_mutex_unlock(&aidl_mutex);

    binder_status_t status = aidl_transact_initialize(aidl_hci_binder, aidl_callbacks_binder);
    if (status != STATUS_OK) {
        log_error("initialize transact failed: %d", (int) status);
        printf("IBluetoothHci.initialize() transact failed (%d).\n", (int) status);
        pthread_mutex_lock(&aidl_mutex);
        aidl_opened = 0;
        pthread_mutex_unlock(&aidl_mutex);
        hci_transport_android_aidl_release_binders();
        return -1;
    }

    int32_t init_status = aidl_wait_init_complete();
    if (init_status < 0) {
        printf("Timed out waiting for IBluetoothHciCallbacks.initializationComplete.\n");
        pthread_mutex_lock(&aidl_mutex);
        aidl_opened = 0;
        pthread_mutex_unlock(&aidl_mutex);
        (void) aidl_transact_void(aidl_hci_binder, AIDL_TXN_CLOSE);
        hci_transport_android_aidl_release_binders();
        return -1;
    }
    if (init_status != AIDL_STATUS_SUCCESS) {
        printf("IBluetoothHci initializationComplete: %s (%" PRId32 ")\n",
               aidl_status_name(init_status), init_status);
        if (init_status == AIDL_STATUS_ALREADY_INITIALIZED) {
            printf("Another client already holds the HAL (usually com.android.bluetooth).\n");
            printf("Try: svc bluetooth disable  OR  am force-stop com.android.bluetooth\n");
        }
        pthread_mutex_lock(&aidl_mutex);
        aidl_opened = 0;
        pthread_mutex_unlock(&aidl_mutex);
        (void) aidl_transact_void(aidl_hci_binder, AIDL_TXN_CLOSE);
        hci_transport_android_aidl_release_binders();
        return -1;
    }

    log_info("IBluetoothHci initialized");
    return 0;
}

static int hci_transport_android_aidl_close(void){
    log_info("close");
    pthread_mutex_lock(&aidl_mutex);
    aidl_opened = 0;
    aidl_free_rx_queue();
    pthread_mutex_unlock(&aidl_mutex);

    if (aidl_hci_binder != NULL) {
        (void) aidl_transact_void(aidl_hci_binder, AIDL_TXN_CLOSE);
    }
    hci_transport_android_aidl_release_binders();
    return 0;
}

static void hci_transport_android_aidl_register_packet_handler(
    void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    hci_transport_android_aidl_packet_handler = handler;
}

static int hci_transport_android_aidl_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    if ((aidl_hci_binder == NULL) || (packet == NULL) || (size <= 0)) {
        return -1;
    }

    transaction_code_t code;
    switch (packet_type) {
        case HCI_COMMAND_DATA_PACKET:
            code = AIDL_TXN_SEND_HCI_COMMAND;
            break;
        case HCI_ACL_DATA_PACKET:
            code = AIDL_TXN_SEND_ACL_DATA;
            break;
        case HCI_SCO_DATA_PACKET:
            code = AIDL_TXN_SEND_SCO_DATA;
            break;
        case HCI_ISO_DATA_PACKET:
            code = AIDL_TXN_SEND_ISO_DATA;
            break;
        default:
            log_error("unsupported HCI packet type 0x%02x", packet_type);
            return -1;
    }

    binder_status_t status = aidl_transact_bytes(aidl_hci_binder, code, packet, size);
    if (status != STATUS_OK) {
        log_error("send packet type 0x%02x failed: %d", packet_type, (int) status);
        return -1;
    }
    return 0;
}

static const hci_transport_t hci_transport_android_aidl = {
    /* const char * name; */ "Android-AIDL",
    /* void   (*init) (const void *transport_config); */ &hci_transport_android_aidl_init,
    /* int    (*open)(void); */ &hci_transport_android_aidl_open,
    /* int    (*close)(void); */ &hci_transport_android_aidl_close,
    /* void   (*register_packet_handler)(void (*handler)(...)); */ &hci_transport_android_aidl_register_packet_handler,
    // NULL => synchronous transport: send_packet consumes the buffer before return.
    /* int    (*can_send_packet_now)(uint8_t packet_type); */ NULL,
    /* int    (*send_packet)(uint8_t packet_type, uint8_t *packet, int size); */ &hci_transport_android_aidl_send_packet,
    /* int    (*set_baudrate)(uint32_t baudrate); */ NULL,
    /* void   (*reset_link)(void); */ NULL,
    /* void   (*set_sco_config)(uint16_t voice_setting, int num_connections); */ NULL,
};

const hci_transport_t * hci_transport_android_aidl_instance(void){
    return &hci_transport_android_aidl;
}

#else /* !(__ANDROID__ && HAVE_ANDROID_BINDER_NDK) */

static void hci_transport_android_aidl_init(const void *transport_config){
    UNUSED(transport_config);
}

static int hci_transport_android_aidl_open(void){
    log_error("IBluetoothHci AIDL client needs Android NDK libbinder_ndk (API 29+)");
    printf("IBluetoothHci AIDL transport is a stub in this build.\n");
    printf("Rebuild with the Android NDK, ANDROID_PLATFORM=android-34, and libbinder_ndk.\n");
    return -1;
}

static int hci_transport_android_aidl_close(void){
    return 0;
}

static void hci_transport_android_aidl_register_packet_handler(
    void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    UNUSED(handler);
}

static int hci_transport_android_aidl_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    UNUSED(packet_type);
    UNUSED(packet);
    UNUSED(size);
    return -1;
}

static const hci_transport_t hci_transport_android_aidl = {
    /* const char * name; */ "Android-AIDL",
    /* void   (*init) (const void *transport_config); */ &hci_transport_android_aidl_init,
    /* int    (*open)(void); */ &hci_transport_android_aidl_open,
    /* int    (*close)(void); */ &hci_transport_android_aidl_close,
    /* void   (*register_packet_handler)(void (*handler)(...)); */ &hci_transport_android_aidl_register_packet_handler,
    /* int    (*can_send_packet_now)(uint8_t packet_type); */ NULL,
    /* int    (*send_packet)(uint8_t packet_type, uint8_t *packet, int size); */ &hci_transport_android_aidl_send_packet,
    /* int    (*set_baudrate)(uint32_t baudrate); */ NULL,
    /* void   (*reset_link)(void); */ NULL,
    /* void   (*set_sco_config)(uint16_t voice_setting, int num_connections); */ NULL,
};

const hci_transport_t * hci_transport_android_aidl_instance(void){
    return &hci_transport_android_aidl;
}

#endif
