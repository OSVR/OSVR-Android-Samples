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

LOCAL_MODULE    := native-activity
LOCAL_SRC_FILES := main.c
LOCAL_LDLIBS    := -llog -landroid -lEGL -lGLESv1_CM
LOCAL_STATIC_LIBRARIES := android_native_app_glue
LOCAL_SHARED_LIBRARIES := osvrClient osvrClientKit osvrCommon osvrUtil usb1.0 gnustl_shared jsoncpp
include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
