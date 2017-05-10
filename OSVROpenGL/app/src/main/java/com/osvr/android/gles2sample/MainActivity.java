/*
 * Copyright (C) 2015 Sensics, Inc. and contributors.
 *
 * Based on Android NDK samples, which are:
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.osvr.android.gles2sample;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

import com.osvr.android.utils.OSVRFileExtractor;


public class MainActivity extends Activity {

    public static final String TAG = "gles2sample";
    MainActivityView mView;

    @Override protected void onCreate(Bundle icicle) {
        Log.i(TAG, "MainActivity: onCreate()");
        super.onCreate(icicle);
        OSVRFileExtractor.extractFiles(this);
        mView = new MainActivityView(getApplication());
	    setContentView(mView);
        //openCamera();
    }

    @Override protected void onPause() {
        Log.i(TAG, "MainActivity: onPause()");
        super.onPause();
        mView.onPause();
    }

    @Override protected void onResume() {
        Log.i(TAG, "MainActivity: onResume()");
        super.onResume();
        mView.onResume();
    }

    @Override protected void onStop() {
        Log.i(TAG, "MainActivity: onStop()");
        super.onStop();
        mView.onStop();
    }
}
