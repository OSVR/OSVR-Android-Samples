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
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.os.Bundle;
import android.util.Log;

import com.osvr.android.utils.OSVRPluginExtractor;

import java.io.IOException;


public class MainActivity extends Activity implements Camera.PreviewCallback {

    public static final String TAG = "gles2sample";
    MainActivityView mView;

    SurfaceTexture mCameraTexture;
    int mCameraPreviewWidth = -1;
    int mCameraPreviewHeight = -1;
    Camera mCamera;

    private void setCameraParams() {
        Camera.Parameters parms = mCamera.getParameters();
        parms.setRecordingHint(true);
        parms.setVideoStabilization(false);

        Camera.Size size = parms.getPreviewSize();
        mCameraPreviewWidth = size.width;
        mCameraPreviewHeight = size.height;
        mCamera.setParameters(parms);

        int[] fpsRange = new int[2];
        Camera.Size mCameraPreviewSize = parms.getPreviewSize();
        parms.getPreviewFpsRange(fpsRange);
        String previewFacts = mCameraPreviewSize.width + "x" + mCameraPreviewSize.height;
        if (fpsRange[0] == fpsRange[1]) {
            previewFacts += " @" + (fpsRange[0] / 1000.0) + "fps";
        } else {
            previewFacts += " @[" + (fpsRange[0] / 1000.0) +
                    " - " + (fpsRange[1] / 1000.0) + "] fps";
        }
        Log.i(TAG, "Camera config: " + previewFacts);

        mCameraPreviewWidth = mCameraPreviewSize.width;
        mCameraPreviewHeight = mCameraPreviewSize.height;
    }

    private void openCamera() {
        mCameraTexture = new SurfaceTexture(123);
        mCamera = Camera.open();
        setCameraParams();
        try {
            mCamera.setPreviewTexture(mCameraTexture);
        } catch(IOException ex) {
            Log.d(TAG, "Error on setPreviewTexture: " + ex.getMessage());
            throw new RuntimeException("error during setPreviewTexture");
        }
        //mCamera.setPreviewCallbackWithBuffer(this);
        mCamera.setPreviewCallback(this);
        mCamera.startPreview();
    }

    @Override protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        OSVRPluginExtractor.extractPlugins(this);
        mView = new MainActivityView(getApplication());
	    setContentView(mView);

        openCamera();
    }

    @Override protected void onPause() {
        super.onPause();
        mView.onPause();

    }

    @Override protected void onResume() {
        super.onResume();
        mView.onResume();
    }

    @Override protected void onStop() {
        super.onStop();
        mView.onStop();

        if(mCamera != null) {
            mCamera.stopPreview();
            mCamera.release();
            mCamera = null;
        }
    }

    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
//        Log.d(TAG, "Got onPreviewFrame");
        MainActivityJNILib.reportFrame(
                data, mCameraPreviewWidth, mCameraPreviewHeight, (short) 3, (short) 1);
    }
}
