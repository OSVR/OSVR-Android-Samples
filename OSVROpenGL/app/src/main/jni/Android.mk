# Copyright (C) 2015 Sensics, Inc. and contributors
#
# Based on Android NDK samples, which are:
# Copyright (C) 2010 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := usb1.0
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\libusb1.0.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := osvrClient
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\libosvrClient.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := osvrClientKit
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\libosvrClientKit.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := osvrCommon
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\libosvrCommon.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := osvrUtil
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\libosvrUtil.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := jsoncpp
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\libjsoncpp.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := functionality
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\libfunctionality.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := osvrJointClientKit
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\libosvrJointClientKit.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := osvrServer
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\libosvrServer.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := osvrConnection
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\libosvrConnection.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := osvrPluginKit
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\libosvrPluginKit.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := osvrPluginHost
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\libosvrPluginHost.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := osvrVRPNServer
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\libosvrVRPNServer.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := com_osvr_android_jniImaging
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\osvr-plugins-0\libcom_osvr_android_jniImaging.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := com_osvr_android_sensorTracker
LOCAL_SRC_FILES := ${OSVR_ANDROID}\lib\osvr-plugins-0\libcom_osvr_android_sensorTracker.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE    := native-activity
LOCAL_SRC_FILES := main.cpp
LOCAL_CFLAGS    := -I${OSVR_ANDROID}\include
LOCAL_LDLIBS    := -llog -landroid -lEGL -lGLESv2
LOCAL_STATIC_LIBRARIES := android_native_app_glue boost_serialization_static
LOCAL_SHARED_LIBRARIES := osvrClient osvrClientKit functionality osvrCommon osvrUtil osvrServer osvrJointClientKit osvrConnection osvrPluginKit osvrPluginHost osvrVRPNServer usb1.0 gnustl_shared jsoncpp
include $(BUILD_SHARED_LIBRARY)

$(call import-module,boost/1.57.0)

$(call import-module,android/native_app_glue)
