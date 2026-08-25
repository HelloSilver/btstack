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
     * @param transport {@code "h4"} for UART or {@code "hci"} for kernel HCI User Channel
     * @param device UART path (h4) or HCI index as a decimal string (hci)
     * @return 0 on success
     */
    public native int init(String transport, String device);

    /** Blocking POSIX run loop. */
    public native void run();

    /** Request HCI power-off. */
    public native void shutdown();
}
