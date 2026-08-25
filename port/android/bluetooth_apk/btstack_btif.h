/*
 * Copyright (C) 2026 BlueKitchen GmbH
 *
 * See the file LICENSE in the BTstack repository for licensing terms.
 */

#ifndef BTSTACK_BTIF_H
#define BTSTACK_BTIF_H

#include "hardware/bluetooth.h"

#ifdef __cplusplus
extern "C" {
#endif

int btstack_btif_init(bt_callbacks_t *callbacks, const char *user_data_directory);
int btstack_btif_enable(void);
int btstack_btif_disable(void);
void btstack_btif_cleanup(void);
int btstack_btif_get_adapter_properties(void);
int btstack_btif_get_adapter_property(bt_property_type_t type);
int btstack_btif_set_adapter_property(const bt_property_t *property);
int btstack_btif_start_discovery(void);
int btstack_btif_cancel_discovery(void);
int btstack_btif_set_os_callouts(bt_os_callouts_t *callouts);
const void *btstack_btif_get_profile_interface(const char *profile_id);

#ifdef __cplusplus
}
#endif
#endif
