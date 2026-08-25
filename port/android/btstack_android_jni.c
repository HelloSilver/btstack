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

#define BTSTACK_FILE__ "btstack_android_jni.c"

#include "btstack_android.h"

#include <jni.h>
#include <string.h>

int btstack_main(int argc, const char * argv[]);

static int btstack_android_jni_initialized;

JNIEXPORT jint JNICALL
Java_com_bluekitchen_btstack_BTstack_init(JNIEnv *env, jobject thiz, jstring transport, jstring device){
    (void) thiz;

    const char * transport_c = (*env)->GetStringUTFChars(env, transport, NULL);
    const char * device_c = (*env)->GetStringUTFChars(env, device, NULL);
    if ((transport_c == NULL) || (device_c == NULL)) {
        if (transport_c) (*env)->ReleaseStringUTFChars(env, transport, transport_c);
        if (device_c) (*env)->ReleaseStringUTFChars(env, device, device_c);
        return -1;
    }

    const char * argv_storage[6];
    int argc = 0;
    argv_storage[argc++] = "btstack";
    argv_storage[argc++] = "-t";
    argv_storage[argc++] = transport_c;
    if (strcmp(transport_c, "hci") == 0) {
        argv_storage[argc++] = "-d";
    } else if (strcmp(transport_c, "aidl") == 0) {
        argv_storage[argc++] = "-i";
    } else {
        argv_storage[argc++] = "-u";
    }
    argv_storage[argc++] = device_c;
    argv_storage[argc++] = "-c";

    int rc = btstack_android_init(argc, argv_storage);
    if (rc == 0) {
        btstack_main(0, NULL);
        btstack_android_jni_initialized = 1;
    }

    (*env)->ReleaseStringUTFChars(env, transport, transport_c);
    (*env)->ReleaseStringUTFChars(env, device, device_c);
    return rc;
}

JNIEXPORT void JNICALL
Java_com_bluekitchen_btstack_BTstack_run(JNIEnv *env, jobject thiz){
    (void) env;
    (void) thiz;
    if (!btstack_android_jni_initialized) {
        return;
    }
    btstack_android_run();
}

JNIEXPORT void JNICALL
Java_com_bluekitchen_btstack_BTstack_shutdown(JNIEnv *env, jobject thiz){
    (void) env;
    (void) thiz;
    btstack_android_shutdown();
}
