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

package com.osvr.common.jni;

// Wrapper for native library

import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import java.io.IOException;

public class JNIBridge {
    private static String TAG = "JNIBridge";
    private static boolean sLibrariesLoaded = false;
    private static boolean sPaused = false;
    private static boolean sCameraEnabled = false;
    private static boolean sIsMissingNativeButtonFuncs = false; // native button jni funcs may not be loaded

    static SurfaceTexture sCameraTexture;
    static int sCameraPreviewWidth = -1;
    static int sCameraPreviewHeight = -1;
    static Camera sCamera;

    static Camera.PreviewCallback sPreviewCallback = new Camera.PreviewCallback() {
        @Override
        public void onPreviewFrame(byte[] data, Camera camera) {
            //Log.d(TAG, "Got onPreviewFrame");
            if(!sPaused) {
                JNIBridge.reportFrame(
                    data, sCameraPreviewWidth, sCameraPreviewHeight);
            }
        }
    };

    private static void setCameraParams() {
        Camera.Parameters parms = sCamera.getParameters();
        parms.setRecordingHint(true);
        parms.setVideoStabilization(false);
        parms.setPreviewSize(640, 480);
        Camera.Size size = parms.getPreviewSize();
        sCameraPreviewWidth = size.width;
        sCameraPreviewHeight = size.height;
        sCamera.setParameters(parms);

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

        sCameraPreviewWidth = mCameraPreviewSize.width;
        sCameraPreviewHeight = mCameraPreviewSize.height;
    }

    private static void openCamera() {
        if(sCameraEnabled && sCamera == null) {
            sCameraTexture = new SurfaceTexture(123);
            sCamera = Camera.open();
            setCameraParams();
            try {
                sCamera.setPreviewTexture(sCameraTexture);
            } catch (IOException ex) {
                Log.d(TAG, "Error on setPreviewTexture: " + ex.getMessage());
                throw new RuntimeException("error during setPreviewTexture");
            }
            //mCamera.setPreviewCallbackWithBuffer(this);
            sCamera.setPreviewCallback(sPreviewCallback);
            sCamera.startPreview();
        }
    }

    protected static void stopCamera() {
        if(sCamera != null) {
            sCamera.setPreviewCallback(null);
            sCamera.stopPreview();
            sCamera.release();
            sCamera = null;
        }
    }

    public static void enableCamera() {
        sCameraEnabled = true;
    }

    public static void disableCamera() {
        sCameraEnabled = false;
        stopCamera();
    }

    public static void onResume() {
        sPaused = false;
        openCamera();
    }

    public static void onStop() {
        sPaused = true;
        stopCamera();
    }

    public static void onPause() {
        sPaused = true;
        stopCamera();
    }

    private static int sTrackedPointerId = -1;
    public static boolean onTouchEvent(MotionEvent motionEvent) {
        if(sIsMissingNativeButtonFuncs) {
            return false;
        }
        try {
            int action = motionEvent.getAction();
//            Log.i(TAG, String.format("MotionEvent action: %s", MotionEvent.actionToString(action)));

            if(action != MotionEvent.ACTION_DOWN && action != MotionEvent.ACTION_MOVE) {
                return false;
            }

            if(motionEvent.getPointerCount() == 0) {
                sTrackedPointerId = -1;
                return false;
            }

            if(sTrackedPointerId < 0) {
                // get the first pointer id of the first pointer index and keep
                // tracking that until it goes out of scope
                sTrackedPointerId = motionEvent.getPointerId(0);
            }

            // if the pointer id we were tracking is still available, get its index
            int pointerIndex = motionEvent.findPointerIndex(sTrackedPointerId);

            // if it's not still available, update the tracked pointer id
            if(pointerIndex < 0) {
                pointerIndex = 0;
                sTrackedPointerId = motionEvent.getPointerId(pointerIndex);
            }

            int eventSource = motionEvent.getSource();
            int inputDeviceId = motionEvent.getDeviceId();
            InputDevice inputDevice = InputDevice.getDevice(inputDeviceId);

            float x = motionEvent.getX(pointerIndex);
            InputDevice.MotionRange xRange = inputDevice.getMotionRange(MotionEvent.AXIS_X, eventSource);

            float y = motionEvent.getY(pointerIndex);
            InputDevice.MotionRange yRange = inputDevice.getMotionRange(MotionEvent.AXIS_Y, eventSource);

//            Log.i(TAG, String.format("x: %f, y: %f, xMin: %f, xMax: %f, yMin: %f, yMax: %f",
//                    x, y,
//                    xRange.getMin(), xRange.getMax(),
//                    yRange.getMin(), yRange.getMax()));

            reportMouse(x, y,
                    xRange == null ? 0 : xRange.getMin(),
                    xRange == null ? 0 : xRange.getMax(),
                    yRange == null ? 0 : yRange.getMin(),
                    yRange == null ? 0 : yRange.getMax());

            return true;
        } catch(Exception e) {
            sIsMissingNativeButtonFuncs = true;
            return false;
        }
    }

    public static boolean onKeyDown(int keyCode, KeyEvent keyEvent) {
        // The native code to push key down events might not be loaded,
        // we'll try to call it and gracefully recover if we fail.
        // After a failure, we'll stop trying
        if(sIsMissingNativeButtonFuncs) {
            return false;
        }
        try {
            reportKeyDown(keyCode);
            return true;
        } catch(Exception e) { // @TODO: narrow down this exception?
            sIsMissingNativeButtonFuncs = true;
            return false;
        }
    }

    public static boolean onKeyUp(int keyCode, KeyEvent keyEvent) {
        // The native code to push key down events might not be loaded,
        // we'll try to call it and gracefully recover if we fail.
        // After a failure, we'll stop trying
        if(sIsMissingNativeButtonFuncs) {
            return false;
        }
        try {
            reportKeyUp(keyCode);
            return true;
        } catch(Exception e) { // @TODO: narrow down this exception?
            sIsMissingNativeButtonFuncs = true;
            return false;
        }
    }

    public static void onSurfaceChanged() {
        openCamera();
    }

    public static void loadLibraries() {
        if(!sLibrariesLoaded) {
            System.loadLibrary("gnustl_shared");
            System.loadLibrary("crystax");
            System.loadLibrary("jsoncpp");
            System.loadLibrary("usb1.0");
            System.loadLibrary("osvrUtil");
            System.loadLibrary("osvrCommon");
            System.loadLibrary("osvrClient");
            System.loadLibrary("osvrClientKit");
            System.loadLibrary("functionality");
            System.loadLibrary("osvrConnection");
            System.loadLibrary("osvrPluginHost");
            System.loadLibrary("osvrPluginKit");
            System.loadLibrary("osvrVRPNServer");
            System.loadLibrary("osvrServer");
            System.loadLibrary("osvrJointClientKit");
            System.loadLibrary("com_osvr_android_jniImaging");
            System.loadLibrary("com_osvr_android_sensorTracker");
            System.loadLibrary("org_osvr_android_moverio");
            System.loadLibrary("com_osvr_Multiserver");
            //System.loadLibrary("org_osvr_filter_deadreckoningrotation");
            System.loadLibrary("org_osvr_filter_oneeuro");
            System.loadLibrary("native-activity");
            sLibrariesLoaded = true;
        }
    }
    static {
        loadLibraries();
    }
    /**
     * @param width the current view width
     * @param height the current view height
     */
    public static native void reportFrame(byte[] data, long width, long height);

    public static native void reportKeyDown(int keyCode); // might change the signature
    public static native void reportKeyUp(int keyUp); // might change the signature
    public static native void reportMouse(float x, float y, float minX, float maxX, float minY, float maxY);
}
