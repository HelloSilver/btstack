package com.bluekitchen.btstack;

/**
 * Minimal JNI entry for the Android NDK port.
 * Load {@code libbtstack_android.so} and call {@link #init} then {@link #run}
 * from a worker thread. {@link #run} blocks in the BTstack run loop.
 */
public class BTstack {
    static {
        System.loadLibrary("btstack_android");
    }

    /**
     * @param transport {@code "h4"} UART, {@code "hci"} kernel HCI User Channel,
     *                 or {@code "aidl"} Android 14 {@code IBluetoothHci}
     * @param device UART path (h4), HCI index as a decimal string (hci),
     *               or AIDL instance name such as {@code "default"} (aidl)
     * @return 0 on success
     */
    public native int init(String transport, String device);

    /** Blocking POSIX run loop. */
    public native void run();

    /** Request HCI power-off. */
    public native void shutdown();
}
