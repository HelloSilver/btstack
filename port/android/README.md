# BTstack Port for Android (NDK)

General Android NDK port. An application can link BTstack and run the usual
`example/` programs as native executables (via `adb`) or as `libbtstack_android.so`
through a small JNI wrapper.

This is **not** a replacement for Android's system Bluetooth stack (Fluoride /
`android.hardware.bluetooth`). It does not integrate with `BluetoothAdapter`,
does not ship a Play Store app, and does not uninstall or patch vendor Bluetooth
daemons.

`port/mtk` is a separate, 2010s RugGear / MediaTek Android 4.x port that replaces
`mtkbt` with `BTstackDaemonRespawn`. Leave that tree alone; this port does not
supersede it.

## HCI transports

| Transport | CLI | What it talks to | Privileges |
|-----------|-----|------------------|------------|
| **H4 UART** (default) | `-t h4 -u /dev/ttyUSB0` | External UART HCI dongle, or the on-board controller UART if you can open it | Root is usually required for `/dev/tty*` |
| **HCI User Channel** | `-t hci -d 0` | Kernel `hci0` via `AF_BLUETOOTH` / `BTPROTO_HCI` / `HCI_CHANNEL_USER` | `CAP_NET_ADMIN` (root). Device must not be owned by the system stack. |
| **IBluetoothHci AIDL** (Android 14+) | `-t aidl -i default` | Vendor Bluetooth HAL `android.hardware.bluetooth.IBluetoothHci/<instance>` | `bluetooth` UID or root, HAL / SELinux access. System Bluetooth must be stopped. **A normal Play Store app cannot open this HAL.** |

H4 framing follows Bluetooth Core Specification, Volume 4, Part A (UART Transport
Layer): packet indicator `0x01`..`0x05` plus the HCI packet. The Linux HCI User
Channel uses the same on-the-wire packet shape.

AIDL payloads are HCI command / ACL / SCO / ISO **without** an H4 packet-indicator
byte (Core Spec Volume 2, Part 5). BTstack is a *client* of `IBluetoothHci` — the
same role as Fluoride/GD — not a vendor HAL implementation.

Talking to the **on-board** controller typically needs one of:

- root (or `bluetooth` UID) plus stopping system Bluetooth, then `-t aidl` on
  Android 14+ (AIDL HAL) or `-t hci` if the kernel exposes `hci0`;
- root plus the vendor UART device (paths vary: `/dev/ttyHS0`, `/dev/ttyBT0`, …);
- custom firmware / engineering builds that leave HCI free;
- an **external HCI dongle** (USB-UART or USB Bluetooth presented as a TTY).

Stock user builds often keep HCI inside the vendor HAL. `-t aidl` is the path
that talks to that HAL; `-t h4` / `-t hci` do not go through it.

## Android 14 IBluetoothHci (AIDL)

On Android 14 the Bluetooth *hardware* path is AIDL, not HIDL. Official interface
(AOSP `android14-release`):

- `hardware/interfaces/bluetooth/aidl/android/hardware/bluetooth/IBluetoothHci.aidl`
- `IBluetoothHci`: `initialize(IBluetoothHciCallbacks)`, `close()`,
  `sendHciCommand(byte[])`, `sendAclData(byte[])`, `sendScoData(byte[])`,
  `sendIsoData(byte[])`
- `IBluetoothHciCallbacks`: `initializationComplete(Status)`, `hciEventReceived`,
  `aclDataReceived`, `scoDataReceived`, `isoDataReceived`
- Typical service: `android.hardware.bluetooth.IBluetoothHci/default`

Vendored copies live under `platform/android/aidl/` for reference. The transport
(`platform/android/hci_transport_android_aidl.c`) is a C client of that interface
using the public NDK binder APIs (`AIBinder_*` / `AParcel_*`) plus runtime
`dlsym` of `AServiceManager_waitForService` / `ABinderProcess_startThreadPool`
from the **device** `libbinder_ndk.so`. The public NDK API-34 stub does not
export those service-manager symbols.

This is **not** Fluoride's Java JNI under `packages/modules/Bluetooth/android/app/jni`
(`com_android_bluetooth_*` / `BluetoothAdapter`). That JNI is the
`android.bluetooth` API bridge, not the HCI hardware interface.

CMake option: `-DBTSTACK_ANDROID_AIDL=ON` (default). Needs
`ANDROID_PLATFORM=android-29` or newer so `libbinder_ndk` exists. `-t h4` and
`-t hci` are unchanged if AIDL is off or the library is missing.

### Privileges (AIDL)

Only **one** client can `initialize()` `IBluetoothHci`. Stop the system stack
first:

```sh
adb shell svc bluetooth disable
# or
adb shell am force-stop com.android.bluetooth
```

Then run as root or `bluetooth` UID, with SELinux allowing the process to find
the HAL (`hwservice_manager` / `hal_bluetooth`). On many user builds you also
need `setenforce 0` or a dedicated sepolicy. A Play Store app signed with a
normal key cannot bind this HAL.

```sh
adb push port/android/build-arm64-v8a/gap_le_advertisements /data/local/tmp/
adb shell su -c '/data/local/tmp/gap_le_advertisements -t aidl -i default'
```

`-i` is the instance (`default`) or a full name such as
`android.hardware.bluetooth.IBluetoothHci/default`.

## Requirements

- Android NDK r26 or newer (r27 LTS tested)
- CMake 3.18+
- Host Python 3 (GATT `.h` generation)
- Target API 24+ for H4 / HCI User Channel; **API 34 (Android 14)** for the
  AIDL client. **arm64-v8a** is the primary ABI

## Build

```sh
export ANDROID_NDK=/path/to/android-ndk-r27d

cmake -S port/android -B port/android/build-arm64-v8a \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-34 \
  -DANDROID_STL=c++_static \
  -DCMAKE_BUILD_TYPE=Release

cmake --build port/android/build-arm64-v8a --target gap_le_advertisements gatt_counter btstack_android bluetooth_jni
```

Or: `ANDROID_NDK=... ./port/android/build.sh` (defaults to `android-34`).

Verified on this tree with Android NDK r27d, CMake 3.28.3, host Python 3.12,
`-DANDROID_PLATFORM=android-34 -DANDROID_ABI=arm64-v8a`:

```
cmake --build port/android/build-arm64-v8a --target gap_le_advertisements gatt_counter btstack_android bluetooth_jni
```

Result: **success**. CMake reported
`Android IBluetoothHci AIDL client: .../34/libbinder_ndk.so` and
`Bluetooth.apk JNI shim: libbluetooth_jni.so`.
`file` reports `ELF 64-bit LSB pie executable, ARM aarch64` for the examples and
`ELF 64-bit LSB shared object, ARM aarch64` for `libbtstack_android.so` and
`libbluetooth_jni.so`. The apk shim exports `JNI_OnLoad` and
`bluetoothInterface`. `readelf -d` shows `NEEDED libbinder_ndk.so`
(plus `liblog`, `libdl`, `libm`, `libc`).

H4 / HCI-only on older devices: `-DANDROID_PLATFORM=android-24
-DBTSTACK_ANDROID_AIDL=OFF`.

Additional ABIs (`armeabi-v7a`, `x86_64`) work the same way with `-DANDROID_ABI=...`.

Outputs in the build directory:

- native examples, e.g. `gap_le_advertisements`, `gatt_counter`
- `libbtstack.a` for linking your own `btstack_main()`
- `libbtstack_android.so` (JNI + `gap_le_advertisements` as the sample app)
- `libbluetooth_jni.so` (Bluetooth.apk `AdapterService` JNI + `bt_interface_t`)

## Run on a device

```sh
adb push port/android/build-arm64-v8a/gap_le_advertisements /data/local/tmp/
adb shell chmod 755 /data/local/tmp/gap_le_advertisements

# External H4 UART dongle (example)
adb shell /data/local/tmp/gap_le_advertisements -t h4 -u /dev/ttyUSB0

# Kernel HCI User Channel after: adb shell svc bluetooth disable
adb shell su -c '/data/local/tmp/gap_le_advertisements -t hci -d 0'

# Android 14 vendor HAL (stop system Bluetooth first)
adb shell su -c '/data/local/tmp/gap_le_advertisements -t aidl -i default'
```

Logs: PacketLogger file `/data/local/tmp/hci_dump.pklg` (override with `-l`), or
`-c` / `--logcat` for the `BTstack` logcat tag. TLV bonding data is stored under
`/data/local/tmp/btstack_*.tlv`.

## Bluetooth.apk JNI (`libbluetooth_jni.so`)

This is **above** BTstack. `IBluetoothHci` is **below** BTstack. Both are needed
for a system integration:

```
App  →  android.bluetooth SDK
     →  Bluetooth.apk (com.android.bluetooth)
     →  libbluetooth_jni.so  (this shim)
     →  bt_interface_t
     →  BTstack
     →  IBluetoothHci AIDL HAL  (or -t h4 / -t hci)
```

Do **not** confuse this with replacing `android.bluetooth` APIs in Play Store
apps. The shim is only for a privileged `Bluetooth.apk` / AOSP tree.

Verified against AOSP **android14-release**
`packages/modules/Bluetooth/android/app`:

- `AdapterApp` / `AdapterService` loads `libbluetooth_jni` via
  `System.loadLibrary("bluetooth_jni")`
- JNI class is still `com.android.bluetooth.btservice.AdapterService`
  (`com_android_bluetooth_btservice_AdapterService.cpp`). The later
  `AdapterNativeInterface` split is not what Android 14 uses.
- Native then calls `bt_interface_t`
  (`system/include/hardware/bluetooth.h`): `init` / `enable` / `disable` /
  adapter properties / discovery / `get_profile_interface`

CMake: `-DBTSTACK_ANDROID_BLUETOOTH_APK=ON` (default) produces
`libbluetooth_jni.so`. Fluoride’s `libbluetooth_jni.so` **cannot stay loaded
at the same time**.

### Adapter APIs: real vs stubbed

| API | Status |
|-----|--------|
| `classInitNative` / `initNative` / `cleanupNative` | Real — binds `JniCallbacks`, starts BTstack |
| `enableNative` / `disableNative` | Real — `hci_power_control` on the POSIX run loop thread |
| `get/setAdapterPropertyNative`, `getAdapterPropertiesNative` | Real — address, name, scan mode, discovery timeout, CoD, dual-mode type |
| `startDiscoveryNative` / `cancelDiscoveryNative` | Real — Classic inquiry + LE scan |
| `createBond*` / sockets / interop / metrics / OOB | Stub — log + fail |
| `get_profile_interface` (GATT, HFP, A2DP, …) | Stub — returns NULL + log |

Default HCI transport for the apk shim is **AIDL** (`-t aidl -i default`).
Override with environment variables in the Bluetooth process:

- `BTSTACK_TRANSPORT=aidl|h4|hci`
- `BTSTACK_AIDL_INSTANCE=default`
- `BTSTACK_H4_TTY=/dev/ttyUSB0`
- `BTSTACK_HCI_DEV=0`

### How a system integrator points Bluetooth.apk at this library

1. Build `libbluetooth_jni.so` (arm64-v8a / API 34) from this port.
2. Stop and replace Fluoride’s library, for example on a userdebug device:

   ```sh
   adb root
   adb remount
   adb shell stop
   # Typical APEX path (device-specific):
   adb push libbluetooth_jni.so \
     /apex/com.android.btservices/lib64/libbluetooth_jni.so
   # or /system/lib64/libbluetooth_jni.so on non-APEX builds
   adb shell start
   ```

3. The process is `com.android.bluetooth` (bluetooth UID). It must be the
   **only** `IBluetoothHci` client — do not also run a `-t aidl` CLI example.
4. Disable unused profile Java services (HFP/A2DP/…) in an overlay or config
   until those JNI tables are filled in. `JNI_OnLoad` registers `classInitNative`
   stubs for common profile classes so they can *load*; calling Fluoride-only
   natives still fails.
5. SELinux / `file_contexts` must allow the bluetooth domain to execute the
   replacement `.so` and to bind the HAL (same constraints as `-t aidl`).

This `.so` is **not** for Play Store apps.

## Using the library from an app

1. Link `libbtstack.a` or load `libbtstack_android.so`.
2. Provide `btstack_main()` (or use the bundled LE scanner in the JNI `.so`).
3. Call `btstack_android_init()` then `btstack_android_run()` from a dedicated thread.

Java stub: `port/android/java/com/bluekitchen/btstack/BTstack.java`

```java
BTstack stack = new BTstack();
stack.init("h4", "/dev/ttyUSB0"); // or init("hci", "0") or init("aidl", "default")
stack.run();                      // blocking
```

The app needs the usual Bluetooth permissions **and** a privileged path to HCI.
A normal Play Store app cannot open the on-board controller or `IBluetoothHci`.

## 中文摘要

这是通用 Android NDK 端口，不是系统蓝牙替换，也不动 `port/mtk`。用 NDK CMake
交叉编译 `arm64-v8a` 原生示例。HCI 传输：

- `-t h4`：H4 UART（外接模块或能打开的板载串口）
- `-t hci`：内核 HCI User Channel
- `-t aidl`：Android 14 `IBluetoothHci` AIDL **客户端**（与 Fluoride/GD 同一角色），
  服务名通常为 `android.hardware.bluetooth.IBluetoothHci/default`

系统集成另有 `libbluetooth_jni.so`：App → Bluetooth.apk → 本 JNI →
`bt_interface_t` → BTstack → `IBluetoothHci`。这与应用商店里的
`android.bluetooth` SDK 不是一层。AIDL 同一时刻只能有一个
`initialize()` 客户端。普通应用商店 App 打不开 HAL / 不能替换系统
`libbluetooth_jni.so`。
