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

H4 framing follows Bluetooth Core Specification, Volume 4, Part A (UART Transport
Layer): packet indicator `0x01`..`0x05` plus the HCI packet. The Linux HCI User
Channel uses the same on-the-wire packet shape.

Talking to the **on-board** controller typically needs one of:

- root plus stopping system Bluetooth (`svc bluetooth disable`), then `-t hci`
  if the kernel was built with `CONFIG_BT` and exposes `hci0`;
- root plus the vendor UART device (paths vary: `/dev/ttyHS0`, `/dev/ttyBT0`, …);
- custom firmware / engineering builds that leave HCI free;
- an **external HCI dongle** (USB-UART or USB Bluetooth presented as a TTY).

Stock user builds often keep HCI inside the vendor HAL. In that case this port
cannot attach to the radio, which is the same constraint other userspace stacks
have on Android.

## Requirements

- Android NDK r26 or newer (r27 LTS tested)
- CMake 3.18+
- Host Python 3 (GATT `.h` generation)
- Target API 24+; **arm64-v8a** is the primary ABI

## Build

```sh
export ANDROID_NDK=/path/to/android-ndk-r27d

cmake -S port/android -B port/android/build-arm64-v8a \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DANDROID_STL=c++_static \
  -DCMAKE_BUILD_TYPE=Release

cmake --build port/android/build-arm64-v8a --target gap_le_advertisements gatt_counter btstack_android
```

Or: `ANDROID_NDK=... ./port/android/build.sh`

Verified on this tree with Android NDK r27d, CMake 3.28.3, host Python 3.12:

```
cmake --build port/android/build-arm64-v8a --target gap_le_advertisements gatt_counter btstack_android
```

Result: **success**. `file` reports `ELF 64-bit LSB pie executable, ARM aarch64` for the examples and `ELF 64-bit LSB shared object, ARM aarch64` for `libbtstack_android.so`.

Additional ABIs (`armeabi-v7a`, `x86_64`) work the same way with `-DANDROID_ABI=...`.

Outputs in the build directory:

- native examples, e.g. `gap_le_advertisements`, `gatt_counter`
- `libbtstack.a` for linking your own `btstack_main()`
- `libbtstack_android.so` (JNI + `gap_le_advertisements` as the sample app)

## Run on a device

```sh
adb push port/android/build-arm64-v8a/gap_le_advertisements /data/local/tmp/
adb shell chmod 755 /data/local/tmp/gap_le_advertisements

# External H4 UART dongle (example)
adb shell /data/local/tmp/gap_le_advertisements -t h4 -u /dev/ttyUSB0

# Kernel HCI User Channel after: adb shell svc bluetooth disable
adb shell su -c '/data/local/tmp/gap_le_advertisements -t hci -d 0'
```

Logs: PacketLogger file `/data/local/tmp/hci_dump.pklg` (override with `-l`), or
`-c` / `--logcat` for the `BTstack` logcat tag. TLV bonding data is stored under
`/data/local/tmp/btstack_*.tlv`.

## Using the library from an app

1. Link `libbtstack.a` or load `libbtstack_android.so`.
2. Provide `btstack_main()` (or use the bundled LE scanner in the JNI `.so`).
3. Call `btstack_android_init()` then `btstack_android_run()` from a dedicated thread.

Java stub: `port/android/java/com/bluekitchen/btstack/BTstack.java`

```java
BTstack stack = new BTstack();
stack.init("h4", "/dev/ttyUSB0"); // or init("hci", "0")
stack.run();                      // blocking
```

The app needs the usual Bluetooth permissions **and** a privileged path to HCI.
A normal Play Store app cannot open the on-board controller.

## 中文摘要

这是通用 Android NDK 端口，不是系统蓝牙替换，也不动 `port/mtk`。用 NDK CMake
交叉编译 `arm64-v8a` 原生示例；HCI 走 H4 UART 或内核 HCI User Channel。访问板载
控制器通常需要 root / 停掉系统蓝牙 / 定制固件，或外接 HCI 模块。
