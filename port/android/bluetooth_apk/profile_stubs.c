/*
 * Copyright (C) 2026 BlueKitchen GmbH
 *
 * Minimal classInitNative stubs so Bluetooth.apk profile classes can load.
 * Remaining profile natives are not implemented; disable those Java services
 * or expect UnsatisfiedLinkError when they call into Fluoride-only APIs.
 *
 * Class names from AOSP android14-release packages/modules/Bluetooth.
 */

#include <jni.h>

#ifdef __ANDROID__
#include <android/log.h>
#define APK_LOGW(...) __android_log_print(ANDROID_LOG_WARN, "btstack_apk", __VA_ARGS__)
#else
#include <stdio.h>
#define APK_LOGW(...) do { printf("btstack_apk: " __VA_ARGS__); printf("\n"); } while (0)
#endif

static void stub_class_init(JNIEnv *env, jclass clazz){
    (void) env;
    (void) clazz;
    APK_LOGW("profile classInitNative stub");
}

static void stub_init(JNIEnv *env, jobject obj){
    (void) env;
    (void) obj;
    APK_LOGW("profile initNative stub");
}

static void stub_cleanup(JNIEnv *env, jobject obj){
    (void) env;
    (void) obj;
}

static const JNINativeMethod k_min_methods[] = {
    {"classInitNative", "()V", (void *) stub_class_init},
    {"initNative", "()V", (void *) stub_init},
    {"cleanupNative", "()V", (void *) stub_cleanup},
};

static const char *k_profile_classes[] = {
    "com/android/bluetooth/hfp/HeadsetNativeInterface",
    "com/android/bluetooth/hfpclient/NativeInterface",
    "com/android/bluetooth/a2dp/A2dpNativeInterface",
    "com/android/bluetooth/a2dpsink/A2dpSinkNativeInterface",
    "com/android/bluetooth/avrcp/AvrcpNativeInterface",
    "com/android/bluetooth/avrcpcontroller/AvrcpControllerNativeInterface",
    "com/android/bluetooth/hid/HidHostService",
    "com/android/bluetooth/hid/HidDeviceNativeInterface",
    "com/android/bluetooth/pan/PanNativeInterface",
    "com/android/bluetooth/gatt/GattNativeInterface",
    "com/android/bluetooth/sdp/SdpManagerNativeInterface",
    "com/android/bluetooth/hearingaid/HearingAidNativeInterface",
    "com/android/bluetooth/hap/HapClientNativeInterface",
    "com/android/bluetooth/le_audio/LeAudioNativeInterface",
    "com/android/bluetooth/vc/VolumeControlNativeInterface",
    "com/android/bluetooth/csip/CsipSetCoordinatorNativeInterface",
    "com/android/bluetooth/btservice/bluetoothkeystore/BluetoothKeystoreNativeInterface",
    "com/android/bluetooth/btservice/ActivityAttributionNativeInterface",
    "com/android/bluetooth/btservice/BluetoothQualityReportNativeInterface",
};

void bluetooth_apk_register_profile_stubs(JNIEnv *env){
    unsigned i;
    for (i = 0; i < sizeof(k_profile_classes) / sizeof(k_profile_classes[0]); i++) {
        jclass clazz = (*env)->FindClass(env, k_profile_classes[i]);
        if (clazz == NULL) {
            (*env)->ExceptionClear(env);
            continue;
        }
        if ((*env)->RegisterNatives(env, clazz, k_min_methods, 3) != 0) {
            APK_LOGW("profile stub RegisterNatives failed for %s (signature mismatch is ok)",
                     k_profile_classes[i]);
            (*env)->ExceptionClear(env);
        }
    }
}
