# OSVR-Android-Samples
This is a set of samples demonstrating basic OSVR usage on Android.

### Build instructions
 1. Follow the instructions for building OSVR-Android-Build here: https://github.com/OSVR/OSVR-Android-Build
 2. Once you have a working build of OSVR for Android, set the OSVR_ANDROID environment variable to the install directory of OSVR-Android-Build (by default it's the "install" directory under the build output directory you specified when you configured CMake). Note: it should have lib, bin, and include directories.
 3. Create your local.properties file in /OSVR-Android-Samples/OSVROpenGL, setting your ndk.dir and sdk.dir property values to point to your local downloads of the android SDK and the crystax ndk folders, respectively.
 4. Open the OSVROpenGL project from Android Studio. You should be able to build and run from there.
 5. You will need to start the osvr server process before running your app.
