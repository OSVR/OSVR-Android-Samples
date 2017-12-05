package com.perracolabs.egl14;

import android.annotation.TargetApi;
import android.opengl.EGL14;
import android.os.Build;
import android.util.Log;

import static com.osvr.android.gles2sample.MainActivity.TAG;

/**
 * EGL 14 Config Factory class
 *
 * @author Perraco Labs (August-2015)
 * @repository https://github.com/perracolabs/GLSurfaceViewEGL14
 */
@TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR1)
public class EGL14Config
{
	/** Extension for surface recording */
	private static final int EGL_RECORDABLE_ANDROID	= 0x3142;
	private static final int EGL_MUTABLE_RENDER_BUFFER_BIT_KHR = 0x00001000;
	/**
	 * Chooses a valid EGL Config for EGL14
	 *
	 * @param eglDisplay
	 *            EGL14 Display
	 * @param recordable
	 *            True to set the recordable flag
	 * @return Resolved config
	 */
	public static android.opengl.EGLConfig chooseConfig(final android.opengl.EGLDisplay eglDisplay, final boolean recordable)
	{
		// The actual surface is generally RGBA or RGBX, so situationally omitting alpha
		// doesn't really help. It can also lead to a huge performance hit on glReadPixels()
		// when reading into a GL_RGBA buffer.
		final int[] attribList = { EGL14.EGL_RED_SIZE, 8, //
				EGL14.EGL_GREEN_SIZE, 8, //
				EGL14.EGL_BLUE_SIZE, 8, //
				EGL14.EGL_ALPHA_SIZE, 8, //
				EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT, //
				//EGL14.EGL_SURFACE_TYPE, EGL_MUTABLE_RENDER_BUFFER_BIT_KHR, //
				EGL14.EGL_NONE, 0, //
				EGL14.EGL_NONE };

		if (recordable == true)
		{
			attribList[attribList.length - 3] = EGL14Config.EGL_RECORDABLE_ANDROID;
			attribList[attribList.length - 2] = 1;
		}

		android.opengl.EGLConfig[] configList = new android.opengl.EGLConfig[1];
		final int[] numConfigs = new int[1];

		if (EGL14.eglChooseConfig(eglDisplay, attribList, 0, configList, 0, configList.length, numConfigs, 0) == false)
		{
			int error = EGL14.eglGetError();
			if(error == EGL14.EGL_BAD_DISPLAY) {
				Log.e(TAG, "EGL_BAD_DISPLAY");
			} else if(error == EGL14.EGL_BAD_ATTRIBUTE) {
				Log.e(TAG, "EGL_BAD_ATTRIBUTE");
			} else if(error == EGL14.EGL_NOT_INITIALIZED) {
				Log.e(TAG, "EGL_NOT_INITIALIZED");
			} else if(error == EGL14.EGL_BAD_PARAMETER) {
				Log.e(TAG, "EGL_BAD_PARAMETER");
			} else {
				Log.e(TAG, "Unknown egl error");
			}
			throw new RuntimeException("failed to find valid RGB8888 EGL14 EGLConfig");
		}

		return configList[0];
	}
}
