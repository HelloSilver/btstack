/*
 * Copyright (C) 2026 BlueKitchen GmbH
 *
 * JNI methods for AOSP android14-release
 * com.android.bluetooth.btservice.AdapterService
 * (com_android_bluetooth_btservice_AdapterService.cpp sMethods table).
 */

#define BTSTACK_FILE__ "adapter_jni.c"

#include "hardware/bluetooth.h"

#include <jni.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <android/log.h>
#define APK_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "btstack_apk", __VA_ARGS__)
#define APK_LOGW(...) __android_log_print(ANDROID_LOG_WARN, "btstack_apk", __VA_ARGS__)
#define APK_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "btstack_apk", __VA_ARGS__)
#else
#include <stdio.h>
#define APK_LOGI(...) do { printf("btstack_apk: " __VA_ARGS__); printf("\n"); } while (0)
#define APK_LOGW(...) APK_LOGI(__VA_ARGS__)
#define APK_LOGE(...) APK_LOGI(__VA_ARGS__)
#endif

static JavaVM *s_vm;
static jobject s_callbacks;
static jmethodID s_state_cb;
static jmethodID s_adapter_prop_cb;
static jmethodID s_discovery_cb;
static jmethodID s_device_found_cb;
static jmethodID s_device_prop_cb;
static const bt_interface_t *s_iface;
static int s_java_attached;

static JNIEnv *apk_env(void){
    JNIEnv *env = NULL;
    if (s_vm == NULL) {
        return NULL;
    }
    if ((*s_vm)->GetEnv(s_vm, (void **) &env, JNI_VERSION_1_6) == JNI_OK) {
        return env;
    }
    if ((*s_vm)->AttachCurrentThread(s_vm, &env, NULL) == 0) {
        s_java_attached = 1;
        return env;
    }
    return NULL;
}

static void cb_thread_evt(bt_cb_thread_evt evt){
    if (evt == ASSOCIATE_JVM) {
        (void) apk_env();
    } else if ((evt == DISASSOCIATE_JVM) && s_java_attached && (s_vm != NULL)) {
        (*s_vm)->DetachCurrentThread(s_vm);
        s_java_attached = 0;
    }
}

static void cb_state(bt_state_t state){
    JNIEnv *env = apk_env();
    if ((env == NULL) || (s_callbacks == NULL) || (s_state_cb == NULL)) {
        return;
    }
    (*env)->CallVoidMethod(env, s_callbacks, s_state_cb, (jint) state);
}

static void cb_adapter_props(bt_status_t status, int num_properties, bt_property_t *properties){
    JNIEnv *env;
    jintArray types;
    jobjectArray values;
    jclass byte_array_class;
    int i;
    if (status != BT_STATUS_SUCCESS) {
        return;
    }
    env = apk_env();
    if ((env == NULL) || (s_callbacks == NULL) || (s_adapter_prop_cb == NULL)) {
        return;
    }
    types = (*env)->NewIntArray(env, num_properties);
    byte_array_class = (*env)->FindClass(env, "[B");
    values = (*env)->NewObjectArray(env, num_properties, byte_array_class, NULL);
    if ((types == NULL) || (values == NULL)) {
        return;
    }
    for (i = 0; i < num_properties; i++) {
        jint t = (jint) properties[i].type;
        jbyteArray bytes = (*env)->NewByteArray(env, properties[i].len);
        (*env)->SetIntArrayRegion(env, types, i, 1, &t);
        if ((bytes != NULL) && (properties[i].val != NULL) && (properties[i].len > 0)) {
            (*env)->SetByteArrayRegion(env, bytes, 0, properties[i].len,
                                       (const jbyte *) properties[i].val);
        }
        (*env)->SetObjectArrayElement(env, values, i, bytes);
        if (bytes != NULL) {
            (*env)->DeleteLocalRef(env, bytes);
        }
    }
    (*env)->CallVoidMethod(env, s_callbacks, s_adapter_prop_cb, types, values);
}

static void cb_discovery(bt_discovery_state_t state){
    JNIEnv *env = apk_env();
    if ((env == NULL) || (s_callbacks == NULL) || (s_discovery_cb == NULL)) {
        return;
    }
    (*env)->CallVoidMethod(env, s_callbacks, s_discovery_cb, (jint) state);
}

static void cb_remote_props(bt_status_t status, RawAddress *bd_addr, int num_properties,
                            bt_property_t *properties){
    JNIEnv *env;
    jbyteArray addr;
    jintArray types;
    jobjectArray values;
    jclass byte_array_class;
    int i;
    if (status != BT_STATUS_SUCCESS) {
        return;
    }
    env = apk_env();
    if ((env == NULL) || (s_callbacks == NULL) || (s_device_prop_cb == NULL) || (bd_addr == NULL)) {
        return;
    }
    addr = (*env)->NewByteArray(env, (jsize) sizeof(*bd_addr));
    types = (*env)->NewIntArray(env, num_properties);
    byte_array_class = (*env)->FindClass(env, "[B");
    values = (*env)->NewObjectArray(env, num_properties, byte_array_class, NULL);
    if ((addr == NULL) || (types == NULL) || (values == NULL)) {
        return;
    }
    (*env)->SetByteArrayRegion(env, addr, 0, (jsize) sizeof(*bd_addr), (const jbyte *) bd_addr);
    for (i = 0; i < num_properties; i++) {
        jint t = (jint) properties[i].type;
        jbyteArray bytes = (*env)->NewByteArray(env, properties[i].len);
        (*env)->SetIntArrayRegion(env, types, i, 1, &t);
        if ((bytes != NULL) && (properties[i].val != NULL) && (properties[i].len > 0)) {
            (*env)->SetByteArrayRegion(env, bytes, 0, properties[i].len,
                                       (const jbyte *) properties[i].val);
        }
        (*env)->SetObjectArrayElement(env, values, i, bytes);
        if (bytes != NULL) {
            (*env)->DeleteLocalRef(env, bytes);
        }
    }
    (*env)->CallVoidMethod(env, s_callbacks, s_device_prop_cb, addr, types, values);
}

static void cb_device_found(int num_properties, bt_property_t *properties){
    JNIEnv *env;
    RawAddress *addr = NULL;
    int i;
    for (i = 0; i < num_properties; i++) {
        if (properties[i].type == BT_PROPERTY_BDADDR) {
            addr = (RawAddress *) properties[i].val;
            break;
        }
    }
    if (addr != NULL) {
        cb_remote_props(BT_STATUS_SUCCESS, addr, num_properties, properties);
    }
    env = apk_env();
    if ((env == NULL) || (s_callbacks == NULL) || (s_device_found_cb == NULL) || (addr == NULL)) {
        return;
    }
    {
        jbyteArray a = (*env)->NewByteArray(env, (jsize) sizeof(*addr));
        if (a == NULL) {
            return;
        }
        (*env)->SetByteArrayRegion(env, a, 0, (jsize) sizeof(*addr), (const jbyte *) addr);
        (*env)->CallVoidMethod(env, s_callbacks, s_device_found_cb, a);
    }
}

static bt_callbacks_t s_bt_callbacks = {
    sizeof(s_bt_callbacks),
    &cb_state,
    &cb_adapter_props,
    &cb_remote_props,
    &cb_device_found,
    &cb_discovery,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &cb_thread_evt,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static void classInitNative(JNIEnv *env, jclass clazz){
    jclass cb_class;
    jfieldID field;
    (void) clazz;
    APK_LOGI("classInitNative (AdapterService / android14-release)");
    (*env)->GetJavaVM(env, &s_vm);
    cb_class = (*env)->FindClass(env, "com/android/bluetooth/btservice/JniCallbacks");
    if (cb_class == NULL) {
        APK_LOGE("JniCallbacks class not found");
        (*env)->ExceptionClear(env);
        s_iface = &bluetoothInterface;
        return;
    }
    field = (*env)->GetFieldID(env, clazz, "mJniCallbacks",
                               "Lcom/android/bluetooth/btservice/JniCallbacks;");
    (void) field;
    s_state_cb = (*env)->GetMethodID(env, cb_class, "stateChangeCallback", "(I)V");
    s_adapter_prop_cb = (*env)->GetMethodID(env, cb_class, "adapterPropertyChangedCallback",
                                            "([I[[B)V");
    s_discovery_cb = (*env)->GetMethodID(env, cb_class, "discoveryStateChangeCallback", "(I)V");
    s_device_found_cb = (*env)->GetMethodID(env, cb_class, "deviceFoundCallback", "([B)V");
    s_device_prop_cb = (*env)->GetMethodID(env, cb_class, "devicePropertyChangedCallback",
                                           "([B[I[[B)V");
    s_iface = &bluetoothInterface;
}

static jboolean initNative(JNIEnv *env, jobject obj, jboolean isGuest,
                           jboolean isCommonCriteriaMode, jint configCompareResult,
                           jobjectArray initFlags, jboolean isAtvDevice,
                           jstring userDataDirectory){
    jfieldID field;
    jobject cb;
    const char *user_dir = NULL;
    int ret;
    (void) isGuest;
    (void) isCommonCriteriaMode;
    (void) configCompareResult;
    (void) initFlags;
    (void) isAtvDevice;

    if (s_iface == NULL) {
        s_iface = &bluetoothInterface;
    }
    field = (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "mJniCallbacks",
                               "Lcom/android/bluetooth/btservice/JniCallbacks;");
    if (field != NULL) {
        cb = (*env)->GetObjectField(env, obj, field);
        if (cb != NULL) {
            if (s_callbacks != NULL) {
                (*env)->DeleteGlobalRef(env, s_callbacks);
            }
            s_callbacks = (*env)->NewGlobalRef(env, cb);
        }
    } else {
        (*env)->ExceptionClear(env);
    }
    if (userDataDirectory != NULL) {
        user_dir = (*env)->GetStringUTFChars(env, userDataDirectory, NULL);
    }
    ret = s_iface->init(&s_bt_callbacks, 0, 0, 0, NULL, 0, user_dir);
    if (user_dir != NULL) {
        (*env)->ReleaseStringUTFChars(env, userDataDirectory, user_dir);
    }
    if (ret != BT_STATUS_SUCCESS && ret != BT_STATUS_DONE) {
        APK_LOGE("bt_interface.init failed: %d", ret);
        return JNI_FALSE;
    }
    (void) s_iface->set_os_callouts(NULL);
    return JNI_TRUE;
}

static void cleanupNative(JNIEnv *env, jobject obj){
    (void) obj;
    if (s_iface != NULL) {
        s_iface->cleanup();
    }
    if (s_callbacks != NULL) {
        (*env)->DeleteGlobalRef(env, s_callbacks);
        s_callbacks = NULL;
    }
}

static jboolean enableNative(JNIEnv *env, jobject obj){
    int ret;
    (void) env;
    (void) obj;
    if (s_iface == NULL) {
        return JNI_FALSE;
    }
    ret = s_iface->enable();
    return ((ret == BT_STATUS_SUCCESS) || (ret == BT_STATUS_DONE)) ? JNI_TRUE : JNI_FALSE;
}

static jboolean disableNative(JNIEnv *env, jobject obj){
    (void) env;
    (void) obj;
    if (s_iface == NULL) {
        return JNI_FALSE;
    }
    return (s_iface->disable() == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean setAdapterPropertyNative(JNIEnv *env, jobject obj, jint type, jbyteArray value){
    bt_property_t prop;
    jbyte *bytes;
    jsize len;
    int ret;
    (void) obj;
    if (s_iface == NULL) {
        return JNI_FALSE;
    }
    len = (*env)->GetArrayLength(env, value);
    bytes = (*env)->GetByteArrayElements(env, value, NULL);
    prop.type = (bt_property_type_t) type;
    prop.len = (int) len;
    prop.val = bytes;
    ret = s_iface->set_adapter_property(&prop);
    (*env)->ReleaseByteArrayElements(env, value, bytes, JNI_ABORT);
    return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean getAdapterPropertiesNative(JNIEnv *env, jobject obj){
    (void) env;
    (void) obj;
    if (s_iface == NULL) {
        return JNI_FALSE;
    }
    return (s_iface->get_adapter_properties() == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean getAdapterPropertyNative(JNIEnv *env, jobject obj, jint type){
    (void) env;
    (void) obj;
    if (s_iface == NULL) {
        return JNI_FALSE;
    }
    return (s_iface->get_adapter_property((bt_property_type_t) type) == BT_STATUS_SUCCESS)
               ? JNI_TRUE
               : JNI_FALSE;
}

static jboolean startDiscoveryNative(JNIEnv *env, jobject obj){
    (void) env;
    (void) obj;
    if (s_iface == NULL) {
        return JNI_FALSE;
    }
    return (s_iface->start_discovery() == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean cancelDiscoveryNative(JNIEnv *env, jobject obj){
    (void) env;
    (void) obj;
    if (s_iface == NULL) {
        return JNI_FALSE;
    }
    return (s_iface->cancel_discovery() == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean stubFalse(JNIEnv *env, jobject obj){
    (void) env;
    (void) obj;
    APK_LOGW("AdapterService JNI stub (unsupported)");
    return JNI_FALSE;
}

static jint stubZero(JNIEnv *env, jobject obj){
    (void) env;
    (void) obj;
    return 0;
}

static void stubVoid(JNIEnv *env, jobject obj){
    (void) env;
    (void) obj;
}

static jboolean getDevicePropertyNative(JNIEnv *env, jobject obj, jbyteArray addr, jint type){
    (void) env;
    (void) obj;
    (void) addr;
    (void) type;
    APK_LOGW("getDevicePropertyNative stub");
    return JNI_FALSE;
}

static jboolean setDevicePropertyNative(JNIEnv *env, jobject obj, jbyteArray addr, jint type,
                                        jbyteArray value){
    (void) env;
    (void) obj;
    (void) addr;
    (void) type;
    (void) value;
    return JNI_FALSE;
}

static jboolean createBondNative(JNIEnv *env, jobject obj, jbyteArray addr, jint transport,
                                 jint addr_type){
    (void) env;
    (void) obj;
    (void) addr;
    (void) transport;
    (void) addr_type;
    APK_LOGW("createBondNative stub");
    return JNI_FALSE;
}

static jboolean createBondOutOfBandNative(JNIEnv *env, jobject obj, jbyteArray addr, jint transport,
                                          jobject p192, jobject p256){
    (void) env;
    (void) obj;
    (void) addr;
    (void) transport;
    (void) p192;
    (void) p256;
    return JNI_FALSE;
}

static jboolean removeBondNative(JNIEnv *env, jobject obj, jbyteArray addr){
    (void) env;
    (void) obj;
    (void) addr;
    return JNI_FALSE;
}

static jboolean cancelBondNative(JNIEnv *env, jobject obj, jbyteArray addr){
    (void) env;
    (void) obj;
    (void) addr;
    return JNI_FALSE;
}

static void generateLocalOobDataNative(JNIEnv *env, jobject obj, jint transport){
    (void) env;
    (void) obj;
    (void) transport;
}

static jint getConnectionStateNative(JNIEnv *env, jobject obj, jbyteArray addr){
    (void) env;
    (void) obj;
    (void) addr;
    return 0;
}

static jboolean pinReplyNative(JNIEnv *env, jobject obj, jbyteArray addr, jboolean accept,
                               jint len, jbyteArray pin){
    (void) env;
    (void) obj;
    (void) addr;
    (void) accept;
    (void) len;
    (void) pin;
    return JNI_FALSE;
}

static jboolean sspReplyNative(JNIEnv *env, jobject obj, jbyteArray addr, jint variant,
                               jboolean accept, jint passkey){
    (void) env;
    (void) obj;
    (void) addr;
    (void) variant;
    (void) accept;
    (void) passkey;
    return JNI_FALSE;
}

static jboolean getRemoteServicesNative(JNIEnv *env, jobject obj, jbyteArray addr, jint transport){
    (void) env;
    (void) obj;
    (void) addr;
    (void) transport;
    return JNI_FALSE;
}

static void dumpNative(JNIEnv *env, jobject obj, jobject fd, jobjectArray args){
    (void) env;
    (void) obj;
    (void) fd;
    (void) args;
}

static jbyteArray dumpMetricsNative(JNIEnv *env, jobject obj){
    (void) obj;
    return (*env)->NewByteArray(env, 0);
}

static jboolean factoryResetNative(JNIEnv *env, jobject obj){
    (void) env;
    (void) obj;
    return JNI_FALSE;
}

static jbyteArray obfuscateAddressNative(JNIEnv *env, jobject obj, jbyteArray addr){
    jsize n;
    jbyteArray out;
    (void) obj;
    if (addr == NULL) {
        return (*env)->NewByteArray(env, 0);
    }
    n = (*env)->GetArrayLength(env, addr);
    out = (*env)->NewByteArray(env, n);
    if (out != NULL) {
        jbyte *b = (*env)->GetByteArrayElements(env, addr, NULL);
        (*env)->SetByteArrayRegion(env, out, 0, n, b);
        (*env)->ReleaseByteArrayElements(env, addr, b, JNI_ABORT);
    }
    return out;
}

static jboolean setBufferLengthMillisNative(JNIEnv *env, jobject obj, jint codec, jint size){
    (void) env;
    (void) obj;
    (void) codec;
    (void) size;
    return JNI_FALSE;
}

static jint getMetricIdNative(JNIEnv *env, jobject obj, jbyteArray addr){
    (void) env;
    (void) obj;
    (void) addr;
    return 0;
}

static jint connectSocketNative(JNIEnv *env, jobject obj, jbyteArray addr, jint type,
                                jbyteArray uuid, jint port, jint flag, jint callingUid){
    (void) env;
    (void) obj;
    (void) addr;
    (void) type;
    (void) uuid;
    (void) port;
    (void) flag;
    (void) callingUid;
    APK_LOGW("connectSocketNative stub");
    return -1;
}

static jint createSocketChannelNative(JNIEnv *env, jobject obj, jint type, jstring serviceName,
                                      jbyteArray uuid, jint port, jint flag, jint callingUid){
    (void) env;
    (void) obj;
    (void) type;
    (void) serviceName;
    (void) uuid;
    (void) port;
    (void) flag;
    (void) callingUid;
    APK_LOGW("createSocketChannelNative stub");
    return -1;
}

static void requestMaximumTxDataLengthNative(JNIEnv *env, jobject obj, jbyteArray addr){
    (void) env;
    (void) obj;
    (void) addr;
}

static jboolean allowLowLatencyAudioNative(JNIEnv *env, jobject obj, jboolean allowed,
                                           jbyteArray addr){
    (void) env;
    (void) obj;
    (void) allowed;
    (void) addr;
    return JNI_FALSE;
}

static void metadataChangedNative(JNIEnv *env, jobject obj, jbyteArray addr, jint key,
                                  jbyteArray value){
    (void) env;
    (void) obj;
    (void) addr;
    (void) key;
    (void) value;
}

static jboolean isLogRedactionEnabled(JNIEnv *env, jobject obj){
    (void) env;
    (void) obj;
    return JNI_FALSE;
}

static jboolean interopMatchAddrNative(JNIEnv *env, jobject obj, jstring feature, jstring addr){
    (void) env;
    (void) obj;
    (void) feature;
    (void) addr;
    return JNI_FALSE;
}

static jboolean interopMatchNameNative(JNIEnv *env, jobject obj, jstring feature, jstring name){
    (void) env;
    (void) obj;
    (void) feature;
    (void) name;
    return JNI_FALSE;
}

static jboolean interopMatchAddrOrNameNative(JNIEnv *env, jobject obj, jstring feature,
                                             jstring addr){
    (void) env;
    (void) obj;
    (void) feature;
    (void) addr;
    return JNI_FALSE;
}

static void interopDatabaseAddRemoveAddrNative(JNIEnv *env, jobject obj, jboolean doAdd,
                                               jstring feature, jstring addr, jint length){
    (void) env;
    (void) obj;
    (void) doAdd;
    (void) feature;
    (void) addr;
    (void) length;
}

static void interopDatabaseAddRemoveNameNative(JNIEnv *env, jobject obj, jboolean doAdd,
                                               jstring feature, jstring name){
    (void) env;
    (void) obj;
    (void) doAdd;
    (void) feature;
    (void) name;
}

static jint getRemotePbapPceVersionNative(JNIEnv *env, jobject obj, jstring addr){
    (void) env;
    (void) obj;
    (void) addr;
    return 0;
}

static jboolean pbapPseDynamicVersionUpgradeIsEnabledNative(JNIEnv *env, jobject obj){
    (void) env;
    (void) obj;
    return JNI_FALSE;
}

static const JNINativeMethod s_adapter_methods[] = {
    {"classInitNative", "()V", (void *) classInitNative},
    {"initNative", "(ZZI[Ljava/lang/String;ZLjava/lang/String;)Z", (void *) initNative},
    {"cleanupNative", "()V", (void *) cleanupNative},
    {"enableNative", "()Z", (void *) enableNative},
    {"disableNative", "()Z", (void *) disableNative},
    {"setAdapterPropertyNative", "(I[B)Z", (void *) setAdapterPropertyNative},
    {"getAdapterPropertiesNative", "()Z", (void *) getAdapterPropertiesNative},
    {"getAdapterPropertyNative", "(I)Z", (void *) getAdapterPropertyNative},
    {"getDevicePropertyNative", "([BI)Z", (void *) getDevicePropertyNative},
    {"setDevicePropertyNative", "([BI[B)Z", (void *) setDevicePropertyNative},
    {"startDiscoveryNative", "()Z", (void *) startDiscoveryNative},
    {"cancelDiscoveryNative", "()Z", (void *) cancelDiscoveryNative},
    {"createBondNative", "([BII)Z", (void *) createBondNative},
    {"createBondOutOfBandNative",
     "([BILandroid/bluetooth/OobData;Landroid/bluetooth/OobData;)Z",
     (void *) createBondOutOfBandNative},
    {"removeBondNative", "([B)Z", (void *) removeBondNative},
    {"cancelBondNative", "([B)Z", (void *) cancelBondNative},
    {"generateLocalOobDataNative", "(I)V", (void *) generateLocalOobDataNative},
    {"getConnectionStateNative", "([B)I", (void *) getConnectionStateNative},
    {"pinReplyNative", "([BZI[B)Z", (void *) pinReplyNative},
    {"sspReplyNative", "([BIZI)Z", (void *) sspReplyNative},
    {"getRemoteServicesNative", "([BI)Z", (void *) getRemoteServicesNative},
    {"alarmFiredNative", "()V", (void *) stubVoid},
    {"readEnergyInfo", "()I", (void *) stubZero},
    {"dumpNative", "(Ljava/io/FileDescriptor;[Ljava/lang/String;)V", (void *) dumpNative},
    {"dumpMetricsNative", "()[B", (void *) dumpMetricsNative},
    {"factoryResetNative", "()Z", (void *) factoryResetNative},
    {"obfuscateAddressNative", "([B)[B", (void *) obfuscateAddressNative},
    {"setBufferLengthMillisNative", "(II)Z", (void *) setBufferLengthMillisNative},
    {"getMetricIdNative", "([B)I", (void *) getMetricIdNative},
    {"connectSocketNative", "([BI[BIII)I", (void *) connectSocketNative},
    {"createSocketChannelNative", "(ILjava/lang/String;[BIII)I",
     (void *) createSocketChannelNative},
    {"requestMaximumTxDataLengthNative", "([B)V", (void *) requestMaximumTxDataLengthNative},
    {"allowLowLatencyAudioNative", "(Z[B)Z", (void *) allowLowLatencyAudioNative},
    {"metadataChangedNative", "([BI[B)V", (void *) metadataChangedNative},
    {"isLogRedactionEnabled", "()Z", (void *) isLogRedactionEnabled},
    {"interopMatchAddrNative", "(Ljava/lang/String;Ljava/lang/String;)Z",
     (void *) interopMatchAddrNative},
    {"interopMatchNameNative", "(Ljava/lang/String;Ljava/lang/String;)Z",
     (void *) interopMatchNameNative},
    {"interopMatchAddrOrNameNative", "(Ljava/lang/String;Ljava/lang/String;)Z",
     (void *) interopMatchAddrOrNameNative},
    {"interopDatabaseAddRemoveAddrNative", "(ZLjava/lang/String;Ljava/lang/String;I)V",
     (void *) interopDatabaseAddRemoveAddrNative},
    {"interopDatabaseAddRemoveNameNative", "(ZLjava/lang/String;Ljava/lang/String;)V",
     (void *) interopDatabaseAddRemoveNameNative},
    {"getRemotePbapPceVersionNative", "(Ljava/lang/String;)I",
     (void *) getRemotePbapPceVersionNative},
    {"pbapPseDynamicVersionUpgradeIsEnabledNative", "()Z",
     (void *) pbapPseDynamicVersionUpgradeIsEnabledNative},
};

int register_com_android_bluetooth_btservice_AdapterService(JNIEnv *env){
    jclass clazz = (*env)->FindClass(env, "com/android/bluetooth/btservice/AdapterService");
    if (clazz == NULL) {
        APK_LOGW("AdapterService class not present (ok unless loaded by Bluetooth.apk)");
        (*env)->ExceptionClear(env);
        return -1;
    }
    return (*env)->RegisterNatives(env, clazz, s_adapter_methods,
                                   (jint) (sizeof(s_adapter_methods) / sizeof(s_adapter_methods[0])));
}

void bluetooth_apk_register_profile_stubs(JNIEnv *env);

jint JNI_OnLoad(JavaVM *vm, void *reserved){
    JNIEnv *env = NULL;
    (void) reserved;
    s_vm = vm;
    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    APK_LOGI("JNI_OnLoad libbluetooth_jni (BTstack)");
    if (register_com_android_bluetooth_btservice_AdapterService(env) != 0) {
        APK_LOGW("AdapterService natives not registered — load this .so from Bluetooth.apk");
    }
    bluetooth_apk_register_profile_stubs(env);
    return JNI_VERSION_1_6;
}
