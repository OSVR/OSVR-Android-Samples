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
/*
 * Copyright (C) 2015 Sensics, Inc. and contributors.
 *
 * Based on Android NDK samples, which are:
 * Copyright (C) 2008 The Android Open Source Project
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


import android.content.Context;
import android.graphics.PixelFormat;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.opengl.EGL14;
import android.opengl.EGLContext;
import android.opengl.EGLSurface;
import android.util.Log;
import android.view.Display;
import android.view.MotionEvent;

import java.io.IOException;

//import javax.microedition.khronos.egl.EGL;
//import javax.microedition.khronos.egl.EGL10;
//import javax.microedition.khronos.egl.EGL11;
//import javax.microedition.khronos.egl.EGLConfig;
//import javax.microedition.khronos.egl.EGLContext;
//import javax.microedition.khronos.egl.EGLDisplay;
//import javax.microedition.khronos.opengles.GL10;
import com.osvr.common.jni.JNIBridge;
import com.perracolabs.egl14.GLSurfaceViewEGL14;
import com.perracolabs.egl14.EGL14Config;
import com.perracolabs.egl14.IRendererEGL14;

/**
 * A simple GLSurfaceView sub-class that demonstrate how to perform
 * OpenGL ES 2.0 rendering into a GL Surface. Note the following important
 * details:
 *
 * - The class must use a custom context factory to enable 2.0 rendering.
 *   See ContextFactory class definition below.
 *
 * - The class must use a custom EGLConfigChooser to be able to select
 *   an EGLConfig that supports 2.0. This is done by providing a config
 *   specification to eglChooseConfig() that has the attribute
 *   EGL10.ELG_RENDERABLE_TYPE containing the EGL_OPENGL_ES2_BIT flag
 *   set. See ConfigChooser class definition below.
 *
 * - The class must select the surface's format, then choose an EGLConfig
 *   that matches it exactly (with regards to red/green/blue/alpha channels
 *   bit depths). Failure to do so would result in an EGL_BAD_MATCH error.
 */
class MainActivityView extends GLSurfaceViewEGL14 {// implements Camera.PreviewCallback {
    private static String TAG = "MainActivityView";
    private static final boolean DEBUG = false;
    boolean mPaused = false;
    MainActivityView.Renderer mRenderer = null;
    private Display mDisplay = null;

    public MainActivityView(Context context, Display display) {
        super(context);
        Log.i(TAG, "MainActivityView constructor with just context.");

        mDisplay = display;

        setPreserveEGLContextOnPause(true);
        setEGLContextClientVersion(2);

        init(false, 0, 0);
    }

    public MainActivityView(Context context, Display display, boolean translucent, int depth, int stencil) {
        super(context);
        Log.i(TAG, "MainActivityView constructor with translucent, depth, and stencil arguments.");

        mDisplay = display;

        setPreserveEGLContextOnPause(true);
        setEGLContextClientVersion(2);

        init(translucent, depth, stencil);
    }

    public void onStop() {
        JNIBridge.onStop();
        MainActivityJNILib.stop();
    }

    public void onPause() {
        mPaused = true;
        JNIBridge.onPause();
    }

    public void onResume() {
        mPaused = false;
        JNIBridge.onResume();
    }

    public boolean onTouchEvent(MotionEvent motionEvent) {
        return JNIBridge.onTouchEvent(motionEvent);
    }

    public void proceedWithPermissions() {
        if(mRenderer != null) {
            mRenderer.proceedWithPermissions();
        }
    }

    private void init(boolean translucent, int depth, int stencil) {
        Log.i(TAG, "MainActivityView.init()");
        /* By default, GLSurfaceView() creates a RGB_565 opaque surface.
         * If we want a translucent one, we should change the surface's
         * format here, using PixelFormat.TRANSLUCENT for GL Surfaces
         * is interpreted as any 32-bit surface with alpha by SurfaceFlinger.
         */
        if (translucent) {
            this.getHolder().setFormat(PixelFormat.TRANSLUCENT);
        }

        /* Setup the context factory for 2.0 rendering.
         * See ContextFactory class definition below
         */
        setEGLContextFactory(new ContextFactory());

        /* We need to choose an EGLConfig that matches the format of
         * our surface exactly. This is going to be done in our
         * custom config chooser. See ConfigChooser class definition
         * below.
         */
//        setEGLConfigChooser( translucent ?
//                             new ConfigChooser(8, 8, 8, 8, depth, stencil) :
//                             new ConfigChooser(5, 6, 5, 0, depth, stencil) );

        /* Set the renderer responsible for frame rendering */
        mRenderer = new Renderer();
        setRenderer(mRenderer);
    }

    private static class ContextFactory implements GLSurfaceViewEGL14.EGLContextFactory {
        private static int EGL_CONTEXT_CLIENT_VERSION = 0x3098;

        public EGLContext createContext(android.opengl.EGLDisplay display, android.opengl.EGLConfig eglConfig) {
            Log.w(TAG, "creating OpenGL ES 2.0 context");
            checkEglError("Before eglCreateContext"/*, egl*/);
            int[] attrib_list = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE };
            EGLContext context = EGL14.eglCreateContext(display, eglConfig, EGL14.EGL_NO_CONTEXT, attrib_list, 0);
            checkEglError("After eglCreateContext"/*, egl*/);

            if(context == null) {
                Log.e(TAG, "eglCreateContext returned null context");
            }

            if(context == EGL14.EGL_NO_CONTEXT) {
                Log.e(TAG, "eglCreateContext returned EGL14.EGL_NO_CONTEXT");
            }

            if(null == EGL14.EGL_NO_CONTEXT) {
                Log.e(TAG, "EGL_NO_CONTEXT == null");
            }
//            EGLSurface surface = EGL14.eglGetCurrentSurface(EGL14.EGL_DRAW);
//            checkEglError("After eglGetCurrentSurface"/*, egl*/);
//
//            if(!EGL14.eglSurfaceAttrib(display, surface, EGL14.EGL_RENDER_BUFFER, EGL14.EGL_SINGLE_BUFFER)) {
//                Log.e(TAG, "eglSurfaceAttrib failed");
//            }
//            checkEglError("After checkEglError");
            return context;
        }

        public void destroyContext(android.opengl.EGLDisplay display, android.opengl.EGLContext context) {
            EGL14.eglDestroyContext(display, context);
        }
    }

    private static void checkEglError(String prompt/*, EGL10 egl*/) {
        int error;
        while ((error = EGL14.eglGetError()) != EGL14.EGL_SUCCESS) {
            Log.e(TAG, String.format("%s: EGL error: 0x%x", prompt, error));
        }
    }

    private class Renderer implements IRendererEGL14 {
        private boolean mProceedWithPermissions = false;
        private boolean mFirstSurfaceChanged = false;
        public void onDrawFrame() {
            MainActivityJNILib.step();
        }
        public void onSurfaceCreated() {}

        public void onSurfaceChanged(int width, int height) {
            mFirstSurfaceChanged = true;
            MainActivityJNILib.initGraphics(width, height);

            // if user has granted storage permissions, go ahead and initialize OSVR
            if(mProceedWithPermissions) {
                MainActivityJNILib.initOSVR(); // this call is idempotent, so no need to check
            }
            JNIBridge.onSurfaceChanged();
        }

        public void onDestroy() {}

        public void proceedWithPermissions() {
            mProceedWithPermissions = true;
            // if we've already initialized graphics once, then we should go ahead
            // and call initOSVR, otherwise wait until the first call to onSurfaceChanged.
            if(mFirstSurfaceChanged) {
                MainActivityJNILib.initOSVR();
            }
        }
    }
}
