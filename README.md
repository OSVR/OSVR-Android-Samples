# OSVR-Android-Samples
This is a set of samples demonstrating basic OSVR usage on Android.

### Build instructions
 1. Follow the instructions for building OSVR-Android-Build here: https://github.com/OSVR/OSVR-Android-Build You do not need to install the osvr_server binaries to the device, as these samples currently use the joint client kit functionality.
 2. Once you have a working build of OSVR for Android, set the OSVR_ANDROID environment variable to the install directory of OSVR-Android-Build (by default it's the "install" directory under the build output directory you specified when you configured CMake). Note: it should have lib, bin, and include directories.
 3. Create your local.properties file in /OSVR-Android-Samples/OSVROpenGL, setting your ndk.dir and sdk.dir property values to point to your local downloads of the android SDK and the crystax ndk folders, respectively.
 4. Open the OSVROpenGL project from Android Studio. Build the app and deploy it to the device.
 5. Use the adb push command from the android SDK to push your build of com_osvr_android_sensorTracker.so to /data/data/com.osvr.android.gles2sample/files/lib/osvr-plugins-0. You may need to create the files/lib/osvr-plugins-0 directories in adb shell first, before pushing them.
 6. You should be able to run the app now, without a separate server running.
