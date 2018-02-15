LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := avformat
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/libavformat.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := avcodec
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/libavcodec.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := swscale
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/libswscale.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := avutil
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/libavutil.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := swresample
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/libswresample.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := postproc
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/libpostproc.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := x264
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/libx264.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE :=  libyuv
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/libyuv.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE :=  libfdk-aac
#LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/libfdk-aac.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE :=  polarssl
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/libpolarssl.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE :=  rtmp
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/librtmp.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libstream

LOCAL_SRC_FILES := StreamProcess.cpp FrameEncoder.cpp AudioEncoder.cpp wavreader.c RtmpLivePublish.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/
LOCAL_STATIC_LIBRARIES := libyuv avformat avcodec swscale avutil swresample postproc x264 libfdk-aac polarssl rtmp
LOCAL_LDLIBS += -L$(LOCAL_PATH)/prebuilt/ -llog -lz -Ipthread

include $(BUILD_SHARED_LIBRARY)