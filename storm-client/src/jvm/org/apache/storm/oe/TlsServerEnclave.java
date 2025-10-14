package org.apache.storm.oe;

public final class TlsServerEnclave {
    static {
        // loads liboe_jni.so/.dylib/.dll
        System.loadLibrary("oe_jni");
    }

    // NOTE: the following methods will be linked to the native methods defined in
    // `jni_bridge.cpp`, which is compiled into liboe_jni.so/.dylib/.dll.

    // returns an opaque native pointer (oe_enclave_t*) as long
    public static native long create(String signedPath, boolean debug);

    // destroys the enclave
    public static native void destroy(long handle);

    // ECALL to set up the TLS server - requires the enclave handle
    public static native int setup_tls_server(long handle, String port, boolean keepServerUp);
}
