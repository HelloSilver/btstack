/*
 * Copyright (C) 2026 BlueKitchen GmbH
 *
 * bt_interface_t export for Bluetooth.apk JNI
 * (AOSP android14-release AdapterService.cpp uses bluetoothInterface).
 */

#define BTSTACK_FILE__ "bt_interface.c"

#include "btstack_btif.h"
#include "btstack_debug.h"
#include "btstack_util.h"

#ifdef __ANDROID__
#include <android/log.h>
#define APK_LOGW(...) __android_log_print(ANDROID_LOG_WARN, "btstack_apk", __VA_ARGS__)
#else
#include <stdio.h>
#define APK_LOGW(...) do { printf("btstack_apk: " __VA_ARGS__); printf("\n"); } while (0)
#endif

static int iface_init(bt_callbacks_t *callbacks, bool guest_mode,
                      bool is_common_criteria_mode, int config_compare_result,
                      const char **init_flags, bool is_atv,
                      const char *user_data_directory){
    UNUSED(guest_mode);
    UNUSED(is_common_criteria_mode);
    UNUSED(config_compare_result);
    UNUSED(init_flags);
    UNUSED(is_atv);
    return btstack_btif_init(callbacks, user_data_directory);
}

static int iface_unsupported(void){
    APK_LOGW("bt_interface method stub");
    return BT_STATUS_UNSUPPORTED;
}

static int iface_get_remote_device_properties(RawAddress *remote_addr){
    UNUSED(remote_addr);
    return iface_unsupported();
}

static int iface_get_remote_device_property(RawAddress *remote_addr, bt_property_type_t type){
    UNUSED(remote_addr);
    UNUSED(type);
    return iface_unsupported();
}

static int iface_set_remote_device_property(RawAddress *remote_addr, const bt_property_t *property){
    UNUSED(remote_addr);
    UNUSED(property);
    return iface_unsupported();
}

static int iface_get_remote_service_record(const RawAddress *remote_addr, const void *uuid){
    UNUSED(remote_addr);
    UNUSED(uuid);
    return iface_unsupported();
}

static int iface_get_remote_services(RawAddress *remote_addr, int transport){
    UNUSED(remote_addr);
    UNUSED(transport);
    return iface_unsupported();
}

static int iface_create_bond(const RawAddress *bd_addr, int transport){
    UNUSED(bd_addr);
    UNUSED(transport);
    APK_LOGW("create_bond stub");
    return BT_STATUS_UNSUPPORTED;
}

static int iface_create_bond_le(const RawAddress *bd_addr, uint8_t addr_type){
    UNUSED(bd_addr);
    UNUSED(addr_type);
    return BT_STATUS_UNSUPPORTED;
}

static int iface_create_bond_oob(const RawAddress *bd_addr, int transport,
                                 const void *p192, const void *p256){
    UNUSED(bd_addr);
    UNUSED(transport);
    UNUSED(p192);
    UNUSED(p256);
    return BT_STATUS_UNSUPPORTED;
}

static int iface_remove_bond(const RawAddress *bd_addr){
    UNUSED(bd_addr);
    return BT_STATUS_UNSUPPORTED;
}

static int iface_cancel_bond(const RawAddress *bd_addr){
    UNUSED(bd_addr);
    return BT_STATUS_UNSUPPORTED;
}

static int iface_get_connection_state(const RawAddress *bd_addr){
    UNUSED(bd_addr);
    return 0;
}

static int iface_pin_reply(const RawAddress *bd_addr, uint8_t accept, uint8_t pin_len,
                           bt_pin_code_t *pin_code){
    UNUSED(bd_addr);
    UNUSED(accept);
    UNUSED(pin_len);
    UNUSED(pin_code);
    return BT_STATUS_UNSUPPORTED;
}

static int iface_ssp_reply(const RawAddress *bd_addr, bt_ssp_variant_t variant,
                           uint8_t accept, uint32_t passkey){
    UNUSED(bd_addr);
    UNUSED(variant);
    UNUSED(accept);
    UNUSED(passkey);
    return BT_STATUS_UNSUPPORTED;
}

static int iface_read_energy_info(void){
    return BT_STATUS_UNSUPPORTED;
}

static void iface_dump(int fd, const char **arguments){
    UNUSED(fd);
    UNUSED(arguments);
}

static void iface_dump_metrics(void *output){
    UNUSED(output);
}

static int iface_config_clear(void){
    return BT_STATUS_UNSUPPORTED;
}

static void iface_interop_clear(void){
}

static void iface_interop_add(uint16_t feature, const RawAddress *addr, size_t len){
    UNUSED(feature);
    UNUSED(addr);
    UNUSED(len);
}

bt_interface_t bluetoothInterface = {
    sizeof(bluetoothInterface),
    &iface_init,
    &btstack_btif_enable,
    &btstack_btif_disable,
    &btstack_btif_cleanup,
    &btstack_btif_get_adapter_properties,
    &btstack_btif_get_adapter_property,
    &btstack_btif_set_adapter_property,
    &iface_get_remote_device_properties,
    &iface_get_remote_device_property,
    &iface_set_remote_device_property,
    &iface_get_remote_service_record,
    &iface_get_remote_services,
    &btstack_btif_start_discovery,
    &btstack_btif_cancel_discovery,
    &iface_create_bond,
    &iface_create_bond_le,
    &iface_create_bond_oob,
    &iface_remove_bond,
    &iface_cancel_bond,
    &iface_get_connection_state,
    &iface_pin_reply,
    &iface_ssp_reply,
    &btstack_btif_get_profile_interface,
    &btstack_btif_set_os_callouts,
    &iface_read_energy_info,
    &iface_dump,
    &iface_dump_metrics,
    &iface_config_clear,
    &iface_interop_clear,
    &iface_interop_add,
};
