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

/**
 * @title HCI Transport client of Android 14 IBluetoothHci (AIDL)
 *
 * Same role as Fluoride/GD: bind
 * `android.hardware.bluetooth.IBluetoothHci/<instance>` and exchange HCI
 * packets with the vendor HAL. Only one client may call initialize().
 * System Bluetooth (`com.android.bluetooth`) must be stopped first.
 *
 * HCI command/ACL/SCO/ISO payloads follow Bluetooth Core Specification
 * Vol 2, Part 5 (no H4 packet-indicator byte). The AIDL itself is
 * hardware/interfaces/bluetooth/aidl on android14-release.
 */

#ifndef HCI_TRANSPORT_ANDROID_AIDL_H
#define HCI_TRANSPORT_ANDROID_AIDL_H

#include "hci_transport.h"

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
    hci_transport_config_type_t type;
    const char *instance;
} hci_transport_config_android_aidl_t;

/**
 * @brief Get IBluetoothHci AIDL client transport
 */
const hci_transport_t * hci_transport_android_aidl_instance(void);

#if defined __cplusplus
}
#endif
#endif
