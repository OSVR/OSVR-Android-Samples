# OSVR-Android-Samples
This is a set of samples demonstrating basic OSVR usage on Android.

### Build instructions
 1. Follow the instructions for building OSVR-Android-Build here: https://github.com/OSVR/OSVR-Android-Build You do not need to install the osvr_server binaries to the device, as these samples currently use the joint client kit functionality.
 2. Once you have a working build of OSVR for Android, set the OSVR_ANDROID environment variable to the install directory of OSVR-Android-Build (by default it's the "install" directory under the build output directory you specified when you configured CMake). Note: it should have lib, bin, and include directories.
 3. Create your local.properties file in /OSVR-Android-Samples/OSVROpenGL, setting your ndk.dir and sdk.dir property values to point to your local downloads of the android SDK and the crystax ndk folders, respectively.
 4. Open the OSVROpenGL project from Android Studio. You should be able to build and run the project from there.

 ### PluginExtractor
Please note the addition of com.osvr.android.utils.OSVRPluginExtractor. This code is based off of code originally authored by Koushik Dutta for the androidmono project under the MIT license. Though modified for OSVR plugin extraction, it remains under the MIT license (see file header for the full license). This will be moved to a separate library.
