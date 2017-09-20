package com.osvr.common.jni;

import android.app.Activity;
import android.util.Log;
import android.view.KeyEvent;

/**
 * Created by bellj on 9/19/2017.
 */

public class OSVRActivity extends Activity {
    private static String TAG = "OSVRActivity";
    @Override public boolean onKeyDown(int keyCode, KeyEvent keyEvent) {
        Log.i(TAG, String.format("onKeyDown(%d)", keyCode));
        return JNIBridge.onKeyDown(keyCode, keyEvent);
    }

    @Override public boolean onKeyUp(int keyCode, KeyEvent keyEvent) {
        Log.i(TAG, String.format("onKeyUp(%d)", keyCode));
        return JNIBridge.onKeyUp(keyCode, keyEvent);
    }
}
