/*
 * C subset of AOSP android14-release
 * packages/modules/Bluetooth/system/include/hardware/bluetooth.h
 *
 * Used by the BTstack Bluetooth.apk JNI / bt_interface_t shim.
 * Not a full Fluoride header: C++ types (std::string, bluetooth::Uuid,
 * RawAddress methods) are replaced with C layouts the AdapterService JNI
 * actually reads for enable/disable/properties/discovery.
 *
 * Licensed under the Apache License, Version 2.0.
 */

#ifndef BTSTACK_ANDROID_HARDWARE_BLUETOOTH_H
#define BTSTACK_ANDROID_HARDWARE_BLUETOOTH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLUETOOTH_INTERFACE_STRING "bluetoothInterface"

#define BT_PROFILE_HANDSFREE_ID "handsfree"
#define BT_PROFILE_HANDSFREE_CLIENT_ID "handsfree_client"
#define BT_PROFILE_ADVANCED_AUDIO_ID "a2dp"
#define BT_PROFILE_ADVANCED_AUDIO_SINK_ID "a2dp_sink"
#define BT_PROFILE_SOCKETS_ID "socket"
#define BT_PROFILE_HIDHOST_ID "hidhost"
#define BT_PROFILE_HIDDEV_ID "hiddev"
#define BT_PROFILE_PAN_ID "pan"
#define BT_PROFILE_GATT_ID "gatt"
#define BT_PROFILE_AV_RC_ID "avrcp"
#define BT_PROFILE_AV_RC_CTRL_ID "avrcp_ctrl"
#define BT_PROFILE_SDP_CLIENT_ID "sdp"
#define BT_PROFILE_HEARING_AID_ID "hearing_aid"
#define BT_PROFILE_LE_AUDIO_ID "le_audio"

typedef struct {
    uint8_t address[6];
} RawAddress;

typedef struct {
    uint8_t name[249];
} __attribute__((packed)) bt_bdname_t;

typedef struct {
    uint8_t pin[16];
} __attribute__((packed)) bt_pin_code_t;

typedef enum {
    BT_SCAN_MODE_NONE,
    BT_SCAN_MODE_CONNECTABLE,
    BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE,
    BT_SCAN_MODE_CONNECTABLE_LIMITED_DISCOVERABLE
} bt_scan_mode_t;

typedef enum {
    BT_STATE_OFF,
    BT_STATE_ON
} bt_state_t;

typedef enum {
    BT_STATUS_SUCCESS = 0,
    BT_STATUS_FAIL,
    BT_STATUS_NOT_READY,
    BT_STATUS_NOMEM,
    BT_STATUS_BUSY,
    BT_STATUS_DONE,
    BT_STATUS_UNSUPPORTED,
    BT_STATUS_PARM_INVALID,
    BT_STATUS_UNHANDLED,
    BT_STATUS_AUTH_FAILURE,
    BT_STATUS_RMT_DEV_DOWN,
    BT_STATUS_AUTH_REJECTED,
    BT_STATUS_JNI_ENVIRONMENT_ERROR,
    BT_STATUS_JNI_THREAD_ATTACH_ERROR,
    BT_STATUS_WAKELOCK_ERROR
} bt_status_t;

typedef enum {
    BT_DISCOVERY_STOPPED,
    BT_DISCOVERY_STARTED
} bt_discovery_state_t;

typedef enum {
    BT_ACL_STATE_CONNECTED,
    BT_ACL_STATE_DISCONNECTED
} bt_acl_state_t;

typedef enum {
    BT_DEVICE_DEVTYPE_BREDR = 0x1,
    BT_DEVICE_DEVTYPE_BLE,
    BT_DEVICE_DEVTYPE_DUAL
} bt_device_type_t;

typedef enum {
    BT_BOND_STATE_NONE,
    BT_BOND_STATE_BONDING,
    BT_BOND_STATE_BONDED
} bt_bond_state_t;

typedef enum {
    BT_SSP_VARIANT_PASSKEY_CONFIRMATION,
    BT_SSP_VARIANT_PASSKEY_ENTRY,
    BT_SSP_VARIANT_CONSENT,
    BT_SSP_VARIANT_PASSKEY_NOTIFICATION
} bt_ssp_variant_t;

typedef enum {
    BT_PROPERTY_BDNAME = 0x1,
    BT_PROPERTY_BDADDR,
    BT_PROPERTY_UUIDS,
    BT_PROPERTY_CLASS_OF_DEVICE,
    BT_PROPERTY_TYPE_OF_DEVICE,
    BT_PROPERTY_SERVICE_RECORD,
    BT_PROPERTY_ADAPTER_SCAN_MODE,
    BT_PROPERTY_ADAPTER_BONDED_DEVICES,
    BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT,
    BT_PROPERTY_REMOTE_FRIENDLY_NAME,
    BT_PROPERTY_REMOTE_RSSI,
    BT_PROPERTY_REMOTE_VERSION_INFO,
    BT_PROPERTY_LOCAL_LE_FEATURES,
    BT_PROPERTY_REMOTE_DEVICE_TIMESTAMP = 0xFF
} bt_property_type_t;

typedef struct {
    bt_property_type_t type;
    int len;
    void *val;
} bt_property_t;

typedef enum {
    ASSOCIATE_JVM,
    DISASSOCIATE_JVM
} bt_cb_thread_evt;

typedef void (*adapter_state_changed_callback)(bt_state_t state);
typedef void (*adapter_properties_callback)(bt_status_t status, int num_properties,
                                            bt_property_t *properties);
typedef void (*remote_device_properties_callback)(bt_status_t status, RawAddress *bd_addr,
                                                  int num_properties, bt_property_t *properties);
typedef void (*device_found_callback)(int num_properties, bt_property_t *properties);
typedef void (*discovery_state_changed_callback)(bt_discovery_state_t state);
typedef void (*pin_request_callback)(RawAddress *remote_bd_addr, bt_bdname_t *bd_name,
                                     uint32_t cod, bool min_16_digit);
typedef void (*ssp_request_callback)(RawAddress *remote_bd_addr, bt_bdname_t *bd_name,
                                     uint32_t cod, bt_ssp_variant_t pairing_variant,
                                     uint32_t pass_key);
typedef void (*bond_state_changed_callback)(bt_status_t status, RawAddress *remote_bd_addr,
                                            bt_bond_state_t state, int fail_reason);
typedef void (*acl_state_changed_callback)(bt_status_t status, RawAddress *remote_bd_addr,
                                           bt_acl_state_t state, int transport,
                                           uint8_t handle_hi, uint8_t handle_lo);
typedef void (*callback_thread_event)(bt_cb_thread_evt evt);
typedef void (*address_consolidate_callback)(RawAddress *main_bd_addr,
                                             RawAddress *secondary_bd_addr);
typedef void (*le_address_associate_callback)(RawAddress *main_bd_addr,
                                              RawAddress *secondary_bd_addr);

typedef struct {
    size_t size;
    adapter_state_changed_callback adapter_state_changed_cb;
    adapter_properties_callback adapter_properties_cb;
    remote_device_properties_callback remote_device_properties_cb;
    device_found_callback device_found_cb;
    discovery_state_changed_callback discovery_state_changed_cb;
    pin_request_callback pin_request_cb;
    ssp_request_callback ssp_request_cb;
    bond_state_changed_callback bond_state_changed_cb;
    address_consolidate_callback address_consolidate_cb;
    le_address_associate_callback le_address_associate_cb;
    acl_state_changed_callback acl_state_changed_cb;
    callback_thread_event thread_evt_cb;
    void *energy_info_cb;
    void *link_quality_report_cb;
    void *generate_local_oob_data_cb;
    void *switch_buffer_size_cb;
    void *switch_codec_cb;
    void *le_rand_cb;
} bt_callbacks_t;

typedef void (*alarm_cb)(void *data);
typedef bool (*set_wake_alarm_callout)(uint64_t delay_millis, bool should_wake,
                                       alarm_cb cb, void *data);
typedef int (*acquire_wake_lock_callout)(const char *lock_name);
typedef int (*release_wake_lock_callout)(const char *lock_name);

typedef struct {
    size_t size;
    set_wake_alarm_callout set_wake_alarm;
    acquire_wake_lock_callout acquire_wake_lock;
    release_wake_lock_callout release_wake_lock;
} bt_os_callouts_t;

/*
 * Function-pointer table used by AOSP AdapterService JNI
 * (android14-release com_android_bluetooth_btservice_AdapterService.cpp).
 * Only the early C-shaped slots are implemented; later C++-only slots
 * stay NULL and are logged if reached via get_profile_interface stubs.
 */
typedef struct {
    size_t size;
    int (*init)(bt_callbacks_t *callbacks, bool guest_mode,
                bool is_common_criteria_mode, int config_compare_result,
                const char **init_flags, bool is_atv,
                const char *user_data_directory);
    int (*enable)(void);
    int (*disable)(void);
    void (*cleanup)(void);
    int (*get_adapter_properties)(void);
    int (*get_adapter_property)(bt_property_type_t type);
    int (*set_adapter_property)(const bt_property_t *property);
    int (*get_remote_device_properties)(RawAddress *remote_addr);
    int (*get_remote_device_property)(RawAddress *remote_addr, bt_property_type_t type);
    int (*set_remote_device_property)(RawAddress *remote_addr, const bt_property_t *property);
    int (*get_remote_service_record)(const RawAddress *remote_addr, const void *uuid);
    int (*get_remote_services)(RawAddress *remote_addr, int transport);
    int (*start_discovery)(void);
    int (*cancel_discovery)(void);
    int (*create_bond)(const RawAddress *bd_addr, int transport);
    int (*create_bond_le)(const RawAddress *bd_addr, uint8_t addr_type);
    int (*create_bond_out_of_band)(const RawAddress *bd_addr, int transport,
                                   const void *p192_data, const void *p256_data);
    int (*remove_bond)(const RawAddress *bd_addr);
    int (*cancel_bond)(const RawAddress *bd_addr);
    int (*get_connection_state)(const RawAddress *bd_addr);
    int (*pin_reply)(const RawAddress *bd_addr, uint8_t accept, uint8_t pin_len,
                     bt_pin_code_t *pin_code);
    int (*ssp_reply)(const RawAddress *bd_addr, bt_ssp_variant_t variant,
                     uint8_t accept, uint32_t passkey);
    const void *(*get_profile_interface)(const char *profile_id);
    int (*set_os_callouts)(bt_os_callouts_t *callouts);
    int (*read_energy_info)(void);
    void (*dump)(int fd, const char **arguments);
    void (*dumpMetrics)(void *output);
    int (*config_clear)(void);
    void (*interop_database_clear)(void);
    void (*interop_database_add)(uint16_t feature, const RawAddress *addr, size_t len);
} bt_interface_t;

extern bt_interface_t bluetoothInterface;

#ifdef __cplusplus
}
#endif
#endif
