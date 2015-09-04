package com.osvr.osvropenglsample;

/**
 * Created by Jeremy on 9/3/2015.
 */
public class OSVRNativeActivity extends android.app.NativeActivity {
    static {
        System.loadLibrary("gnustl_shared");
        System.loadLibrary("crystax");
        System.loadLibrary("jsoncpp");
        System.loadLibrary("usb1.0");
        System.loadLibrary("osvrUtil");
        System.loadLibrary("osvrCommon");
        System.loadLibrary("osvrClient");
        System.loadLibrary("osvrClientKit");
    }
}
