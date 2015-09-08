# OSVR-Android-Samples
This is a set of samples demonstrating basic OSVR usage on Android.

### Build instructions
 1. Follow the instructions for building OSVR-Android-Build here: https://github.com/OSVR/OSVR-Android-Build
 2. Once you have a working build of OSVR for Android, set the OSVR_ANDROID environment variable to the install directory of OSVR-Android-Build (by default it's the "install" directory under the build output directory you specified when you configured CMake). Note: it should have lib, bin, and include directories.
 3. From /OSVR-Android-Samples/OSVROpenGL/app/src/main, run the ndk-build command. Note you will need to add the path to that command (in your chosen NDK) to your PATH environment variable, if you haven't already.
 4. Open the OSVROpenGL project from Android Studio. You should be able to build and run from there.
 5. You will need to start the osvr server process before running your app.
