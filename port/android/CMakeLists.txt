cmake_minimum_required(VERSION 3.18)

project(BTstack-android C)

set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

# Host Python generates GATT headers; the Android toolchain finds programs on the host.
find_package(Python REQUIRED COMPONENTS Interpreter)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(.)

include_directories(${BTSTACK_ROOT}/3rd-party/micro-ecc)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/lc3-google/include)
include_directories(${BTSTACK_ROOT}/3rd-party/md5)
include_directories(${BTSTACK_ROOT}/3rd-party/hxcmod-player)
include_directories(${BTSTACK_ROOT}/3rd-party/hxcmod-player/mod)
include_directories(${BTSTACK_ROOT}/3rd-party/rijndael)
include_directories(${BTSTACK_ROOT}/3rd-party/yxml)
include_directories(${BTSTACK_ROOT}/3rd-party/tinydir)
include_directories(${BTSTACK_ROOT}/src)
include_directories(${BTSTACK_ROOT}/chipset/bcm)
include_directories(${BTSTACK_ROOT}/chipset/csr)
include_directories(${BTSTACK_ROOT}/chipset/em9301)
include_directories(${BTSTACK_ROOT}/chipset/realtek)
include_directories(${BTSTACK_ROOT}/chipset/tc3566x)
include_directories(${BTSTACK_ROOT}/chipset/stlc2500d)
include_directories(${BTSTACK_ROOT}/chipset/zephyr)
include_directories(${BTSTACK_ROOT}/platform/embedded)
include_directories(${BTSTACK_ROOT}/platform/posix)
include_directories(${BTSTACK_ROOT}/platform/android)

file(GLOB SOURCES_SRC       "${BTSTACK_ROOT}/src/*.c" "${BTSTACK_ROOT}/example/*_demo_util*.c")
file(GLOB SOURCES_BLE       "${BTSTACK_ROOT}/src/ble/*.c")
file(GLOB SOURCES_BLUEDROID "${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/*.c" "${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/*.c")
file(GLOB SOURCES_CLASSIC   "${BTSTACK_ROOT}/src/classic/*.c")
file(GLOB SOURCES_GATT      "${BTSTACK_ROOT}/src/ble/gatt-service/*.c")
file(GLOB SOURCES_UECC      "${BTSTACK_ROOT}/3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_HXCMOD    "${BTSTACK_ROOT}/3rd-party/hxcmod-player/*.c"  "${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods/*.c")
file(GLOB SOURCES_LC3_GOOGLE "${BTSTACK_ROOT}/3rd-party/lc3-google/src/*.c")
file(GLOB SOURCES_MD5       "${BTSTACK_ROOT}/3rd-party/md5/md5.c")
file(GLOB SOURCES_RIJNDAEL  "${BTSTACK_ROOT}/3rd-party/rijndael/rijndael.c")
file(GLOB SOURCES_YXML      "${BTSTACK_ROOT}/3rd-party/yxml/yxml.c")
file(GLOB SOURCES_POSIX     "${BTSTACK_ROOT}/platform/posix/*.c")
file(GLOB SOURCES_ANDROID   "${BTSTACK_ROOT}/platform/android/*.c")
file(GLOB SOURCES_BCM       "${BTSTACK_ROOT}/chipset/bcm/*.c")
file(GLOB SOURCES_CSR       "${BTSTACK_ROOT}/chipset/csr/*.c")
file(GLOB SOURCES_CSR_OFF   "${BTSTACK_ROOT}/chipset/csr/csr_set_bd_addr.c")
list(REMOVE_ITEM SOURCES_CSR ${SOURCES_CSR_OFF})
file(GLOB SOURCES_EM9301    "${BTSTACK_ROOT}/chipset/em9301/*.c")
file(GLOB SOURCES_STLC2500D "${BTSTACK_ROOT}/chipset/stlc2500d/*.c")
file(GLOB SOURCES_TC2566X   "${BTSTACK_ROOT}/chipset/tc3566x/*.c")
file(GLOB SOURCES_REALTEK   "${BTSTACK_ROOT}/chipset/realtek/*.c")
file(GLOB SOURCES_ZEPHYR    "${BTSTACK_ROOT}/chipset/zephyr/*.c")

file(GLOB SOURCES_SRC_OFF "${BTSTACK_ROOT}/src/btstack_lc3plus_fraunhofer.c")
list(REMOVE_ITEM SOURCES_SRC ${SOURCES_SRC_OFF})

file(GLOB SOURCES_BLE_OFF "${BTSTACK_ROOT}/src/ble/le_device_db_memory.c")
list(REMOVE_ITEM SOURCES_BLE ${SOURCES_BLE_OFF})

file(GLOB SOURCES_POSIX_OFF
    "${BTSTACK_ROOT}/platform/posix/le_device_db_fs.c"
    "${BTSTACK_ROOT}/platform/posix/btstack_audio_portaudio.c"
    "${BTSTACK_ROOT}/platform/posix/btstack_network_posix.c"
    "${BTSTACK_ROOT}/platform/posix/btstack_sco_transport_posix_i2s_test_bridge.c"
)
list(REMOVE_ITEM SOURCES_POSIX ${SOURCES_POSIX_OFF})

set(SOURCES
    ${SOURCES_BLE}
    ${SOURCES_BCM}
    ${SOURCES_BLUEDROID}
    ${SOURCES_CLASSIC}
    ${SOURCES_CSR}
    ${SOURCES_EM9301}
    ${SOURCES_GATT}
    ${SOURCES_HXCMOD}
    ${SOURCES_LC3_GOOGLE}
    ${SOURCES_MD5}
    ${SOURCES_REALTEK}
    ${SOURCES_RIJNDAEL}
    ${SOURCES_SRC}
    ${SOURCES_STLC2500D}
    ${SOURCES_TC2566X}
    ${SOURCES_UECC}
    ${SOURCES_POSIX}
    ${SOURCES_ANDROID}
    ${SOURCES_YXML}
    ${SOURCES_ZEPHYR}
    main.c
)
list(SORT SOURCES)

add_library(btstack STATIC ${SOURCES})

option(BTSTACK_ANDROID_AIDL "Build IBluetoothHci AIDL client transport (-t aidl)" ON)

set(ANDROID_EXTRA_LIBS m)
if(ANDROID)
    list(APPEND ANDROID_EXTRA_LIBS log)
    # IBluetoothHci client: libbinder_ndk is in the NDK sysroot from API 29.
    # AServiceManager_* is resolved at runtime from the device libbinder_ndk.so.
    if(BTSTACK_ANDROID_AIDL)
        find_library(BINDER_NDK_LIB binder_ndk)
        if(BINDER_NDK_LIB)
            list(APPEND ANDROID_EXTRA_LIBS ${BINDER_NDK_LIB} dl)
            target_compile_definitions(btstack PRIVATE HAVE_ANDROID_BINDER_NDK=1)
            message(STATUS "Android IBluetoothHci AIDL client: ${BINDER_NDK_LIB}")
        else()
            message(WARNING "libbinder_ndk not found (need ANDROID_PLATFORM=android-29+). -t aidl will be a stub; h4/hci are unchanged.")
        endif()
    endif()
endif()

find_package(Threads)
if(Threads_FOUND)
    list(APPEND ANDROID_EXTRA_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../../example/CMakeLists.txt)
set(EXAMPLES ${EXAMPLES_GENERAL} ${EXAMPLES_CLASSIC_ONLY} ${EXAMPLES_LE_ONLY} ${EXAMPLES_DUAL_MODE})
list(REMOVE_DUPLICATES EXAMPLES)
list(REMOVE_ITEM EXAMPLES "mesh_node_demo")

foreach(EXAMPLE ${EXAMPLES})
    set(SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE}.c)
    if("${EXAMPLES_GATT_FILES}" MATCHES ${EXAMPLE})
        message("example ${EXAMPLE} -- with GATT DB")
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
            DEPENDS ${BTSTACK_ROOT}/example/${EXAMPLE}.gatt
            COMMAND ${Python_EXECUTABLE}
            ARGS ${BTSTACK_ROOT}/tool/compile_gatt.py ${BTSTACK_ROOT}/example/${EXAMPLE}.gatt ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
        )
        list(APPEND SOURCES_EXAMPLE ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h)
    else()
        message("example ${EXAMPLE}")
    endif()
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})
    target_link_libraries(${EXAMPLE} btstack ${ANDROID_EXTRA_LIBS})
endforeach(EXAMPLE)

# Shared library for JNI apps. Links gap_le_advertisements as the sample btstack_main().
add_library(btstack_android SHARED btstack_android_jni.c ${BTSTACK_ROOT}/example/gap_le_advertisements.c)
target_link_libraries(btstack_android btstack ${ANDROID_EXTRA_LIBS})
if(ANDROID)
    target_link_options(btstack_android PRIVATE "-Wl,-soname,libbtstack_android.so")
endif()

# Bluetooth.apk JNI / bt_interface_t shim (com.android.bluetooth).
# Replaces Fluoride libbluetooth_jni.so. Does not change h4/hci/aidl transports.
option(BTSTACK_ANDROID_BLUETOOTH_APK "Build libbluetooth_jni.so for Bluetooth.apk" ON)
if(ANDROID AND BTSTACK_ANDROID_BLUETOOTH_APK)
    add_library(bluetooth_jni SHARED
        bluetooth_apk/adapter_jni.c
        bluetooth_apk/bt_interface.c
        bluetooth_apk/btstack_btif.c
        bluetooth_apk/btstack_main_apk.c
        bluetooth_apk/profile_stubs.c
    )
    target_include_directories(bluetooth_jni PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/bluetooth_apk/include
        ${CMAKE_CURRENT_SOURCE_DIR}/bluetooth_apk
    )
    target_link_libraries(bluetooth_jni btstack ${ANDROID_EXTRA_LIBS})
    target_link_options(bluetooth_jni PRIVATE "-Wl,-soname,libbluetooth_jni.so")
    message(STATUS "Bluetooth.apk JNI shim: libbluetooth_jni.so")
endif()
