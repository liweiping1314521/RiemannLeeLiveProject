#include <stdio.h>
#include <string.h>
#include "StreamProcess.h"
#include "FrameEncoder.h"
#include "AudioEncoder.h"
#include "RtmpLivePublish.h"

#include <libyuv.h>
using namespace libyuv;

#include  <android/log.h>
#define  TAG "RiemannLee"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)

jbyte *temp_i420_data;
jbyte *temp_i420_data_scale;
jbyte *temp_i420_data_rotate;
FrameEncoder* frameEncoder;
AudioEncoder* audioEncoder;
RtmpLivePublish* rtmpLivePublish;

static JavaVM *jvm = NULL;

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    jvm = vm;
    return JNI_VERSION_1_6;
}
//在卸载库时执行
void JNI_OnUnLoad(JavaVM *vm, void *reserved) {
    jvm = NULL;
}

//为中间操作需要的分配空间
void init(jint width, jint height, jint dst_width, jint dst_height) {
    temp_i420_data = (jbyte *) malloc(sizeof(jbyte) * width * height * 3 / 2);
    temp_i420_data_scale = (jbyte *) malloc(sizeof(jbyte) * dst_width * dst_height * 3 / 2);
    temp_i420_data_rotate = (jbyte *) malloc(sizeof(jbyte) * dst_width * dst_height * 3 / 2);
}

//进行缩放操作，此时是把1080 * 1920的YUV420P的数据 ==> 480 * 640的YUV420P的数据
void scaleI420(jbyte *src_i420_data, jint width, jint height, jbyte *dst_i420_data, jint dst_width,
               jint dst_height, jint mode) {
    //Y数据大小width*height，U数据大小为1/4的width*height，V大小和U一样，一共是3/2的width*height大小
    jint src_i420_y_size = width * height;
    jint src_i420_u_size = (width >> 1) * (height >> 1);
    //由于是标准的YUV420P的数据，我们可以把三个通道全部分离出来
    jbyte *src_i420_y_data = src_i420_data;
    jbyte *src_i420_u_data = src_i420_data + src_i420_y_size;
    jbyte *src_i420_v_data = src_i420_data + src_i420_y_size + src_i420_u_size;

    //由于是标准的YUV420P的数据，我们可以把三个通道全部分离出来
    jint dst_i420_y_size = dst_width * dst_height;
    jint dst_i420_u_size = (dst_width >> 1) * (dst_height >> 1);
    jbyte *dst_i420_y_data = dst_i420_data;
    jbyte *dst_i420_u_data = dst_i420_data + dst_i420_y_size;
    jbyte *dst_i420_v_data = dst_i420_data + dst_i420_y_size + dst_i420_u_size;

    //调用libyuv库，进行缩放操作
    libyuv::I420Scale((const uint8 *) src_i420_y_data, width,
                      (const uint8 *) src_i420_u_data, width >> 1,
                      (const uint8 *) src_i420_v_data, width >> 1,
                      width, height,
                      (uint8 *) dst_i420_y_data, dst_width,
                      (uint8 *) dst_i420_u_data, dst_width >> 1,
                      (uint8 *) dst_i420_v_data, dst_width >> 1,
                      dst_width, dst_height,
                      (libyuv::FilterMode) mode);
}

void rotateI420(jbyte *src_i420_data, jint width, jint height, jbyte *dst_i420_data, jint degree) {
    jint src_i420_y_size = width * height;
    jint src_i420_u_size = (width >> 1) * (height >> 1);

    jbyte *src_i420_y_data = src_i420_data;
    jbyte *src_i420_u_data = src_i420_data + src_i420_y_size;
    jbyte *src_i420_v_data = src_i420_data + src_i420_y_size + src_i420_u_size;

    jbyte *dst_i420_y_data = dst_i420_data;
    jbyte *dst_i420_u_data = dst_i420_data + src_i420_y_size;
    jbyte *dst_i420_v_data = dst_i420_data + src_i420_y_size + src_i420_u_size;

    //要注意这里的width和height在旋转之后是相反的
    if (degree == libyuv::kRotate90 || degree == libyuv::kRotate270) {
        libyuv::I420Rotate((const uint8 *) src_i420_y_data, width,
                           (const uint8 *) src_i420_u_data, width >> 1,
                           (const uint8 *) src_i420_v_data, width >> 1,
                           (uint8 *) dst_i420_y_data, height,
                           (uint8 *) dst_i420_u_data, height >> 1,
                           (uint8 *) dst_i420_v_data, height >> 1,
                           width, height,
                           (libyuv::RotationMode) degree);
    }
}

void mirrorI420(jbyte *src_i420_data, jint width, jint height, jbyte *dst_i420_data) {
    jint src_i420_y_size = width * height;
    jint src_i420_u_size = (width >> 1) * (height >> 1);

    jbyte *src_i420_y_data = src_i420_data;
    jbyte *src_i420_u_data = src_i420_data + src_i420_y_size;
    jbyte *src_i420_v_data = src_i420_data + src_i420_y_size + src_i420_u_size;

    jbyte *dst_i420_y_data = dst_i420_data;
    jbyte *dst_i420_u_data = dst_i420_data + src_i420_y_size;
    jbyte *dst_i420_v_data = dst_i420_data + src_i420_y_size + src_i420_u_size;

    libyuv::I420Mirror((const uint8 *) src_i420_y_data, width,
                       (const uint8 *) src_i420_u_data, width >> 1,
                       (const uint8 *) src_i420_v_data, width >> 1,
                       (uint8 *) dst_i420_y_data, width,
                       (uint8 *) dst_i420_u_data, width >> 1,
                       (uint8 *) dst_i420_v_data, width >> 1,
                       width, height);
}

//NV21转化为YUV420P数据
void nv21ToI420(jbyte *src_nv21_data, jint width, jint height, jbyte *src_i420_data) {
    //Y通道数据大小
    jint src_y_size = width * height;
    //U通道数据大小
    jint src_u_size = (width >> 1) * (height >> 1);

    //NV21中Y通道数据
    jbyte *src_nv21_y_data = src_nv21_data;
    //由于是连续存储的Y通道数据后即为VU数据，它们的存储方式是交叉存储的
    jbyte *src_nv21_vu_data = src_nv21_data + src_y_size;

    //YUV420P中Y通道数据
    jbyte *src_i420_y_data = src_i420_data;
    //YUV420P中U通道数据
    jbyte *src_i420_u_data = src_i420_data + src_y_size;
    //YUV420P中V通道数据
    jbyte *src_i420_v_data = src_i420_data + src_y_size + src_u_size;

    //直接调用libyuv中接口，把NV21数据转化为YUV420P标准数据，此时，它们的存储大小是不变的
    libyuv::NV21ToI420((const uint8 *) src_nv21_y_data, width,
                       (const uint8 *) src_nv21_vu_data, width,
                       (uint8 *) src_i420_y_data, width,
                       (uint8 *) src_i420_u_data, width >> 1,
                       (uint8 *) src_i420_v_data, width >> 1,
                       width, height);
}

JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_init
        (JNIEnv *env, jclass jclass1, jint jwidth, jint jheight, jint outWidth, jint outHeight)
{
    init(jwidth, jheight, outWidth, outHeight);
    return 0;
}

JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_release
        (JNIEnv *env, jclass jclass1)
{
    free(temp_i420_data);
    free(temp_i420_data_scale);
    free(temp_i420_data_rotate);
    free(frameEncoder);
    free(audioEncoder);
    free(rtmpLivePublish);
    return 0;
}

JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_yuvI420ToNV21
  (JNIEnv *env, jclass type, jbyteArray i420Src,
   jbyteArray nv21Src,
   jint width, jint height)
{
    jbyte *src_i420_data = env->GetByteArrayElements(i420Src, NULL);
    jbyte *src_nv21_data = env->GetByteArrayElements(nv21Src, NULL);

    jint src_y_size = width * height;
    jint src_u_size = (width >> 1) * (height >> 1);

    jbyte *src_i420_y_data = src_i420_data;
    jbyte *src_i420_u_data = src_i420_data + src_y_size;
    jbyte *src_i420_v_data = src_i420_data + src_y_size + src_u_size;

    jbyte *src_nv21_y_data = src_nv21_data;
    jbyte *src_nv21_vu_data = src_nv21_data + src_y_size;


    libyuv::I420ToNV21(
            (const uint8 *) src_i420_y_data, width,
            (const uint8 *) src_i420_u_data, width >> 1,
            (const uint8 *) src_i420_v_data, width >> 1,
            (uint8 *) src_nv21_y_data, width,
            (uint8 *) src_nv21_vu_data, width,
            width, height);

    return 0;
}

JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_compressYUV
    (JNIEnv *env, jclass type,
     jbyteArray src_, jint width,
     jint height, jbyteArray dst_,
     jint dst_width, jint dst_height,
     jint mode, jint degree,
     jboolean isMirror) {

    jbyte *Src_data = env->GetByteArrayElements(src_, NULL);
    jbyte *Dst_data = env->GetByteArrayElements(dst_, NULL);
    //nv21转化为i420(标准YUV420P数据) 这个temp_i420_data大小是和Src_data是一样的
    nv21ToI420(Src_data, width, height, temp_i420_data);
    //进行缩放的操作，这个缩放，会把数据压缩
    scaleI420(temp_i420_data, width, height, temp_i420_data_scale, dst_width, dst_height, mode);
    //如果是前置摄像头，进行镜像操作
    if (isMirror) {
        //进行旋转的操作
        rotateI420(temp_i420_data_scale, dst_width, dst_height, temp_i420_data_rotate, degree);
        //因为旋转的角度都是90和270，那后面的数据width和height是相反的
        mirrorI420(temp_i420_data_rotate, dst_height, dst_width, Dst_data);
    } else {
        //进行旋转的操作
        rotateI420(temp_i420_data_scale, dst_width, dst_height, Dst_data, degree);
    }
    env->ReleaseByteArrayElements(dst_, Dst_data, 0);
    env->ReleaseByteArrayElements(src_, Src_data, 0);

    return 0;
}

JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_cropYUV(
        JNIEnv *env, jclass type, jbyteArray src_, jint width,
        jint height, jbyteArray dst_, jint dst_width, jint dst_height,
        jint left, jint top) {
    //裁剪的区域大小不对
    if (left + dst_width > width || top + dst_height > height) {
        return -1;
    }

    //left和top必须为偶数，否则显示会有问题
    if (left % 2 != 0 || top % 2 != 0) {
        return -1;
    }

    jint src_length = env->GetArrayLength(src_);
    jbyte *src_i420_data = env->GetByteArrayElements(src_, NULL);
    jbyte *dst_i420_data = env->GetByteArrayElements(dst_, NULL);


    jint dst_i420_y_size = dst_width * dst_height;
    jint dst_i420_u_size = (dst_width >> 1) * (dst_height >> 1);

    jbyte *dst_i420_y_data = dst_i420_data;
    jbyte *dst_i420_u_data = dst_i420_data + dst_i420_y_size;
    jbyte *dst_i420_v_data = dst_i420_data + dst_i420_y_size + dst_i420_u_size;

    libyuv::ConvertToI420((const uint8 *) src_i420_data, src_length,
                          (uint8 *) dst_i420_y_data, dst_width,
                          (uint8 *) dst_i420_u_data, dst_width >> 1,
                          (uint8 *) dst_i420_v_data, dst_width >> 1,
                          left, top,
                          width, height,
                          dst_width, dst_height,
                          libyuv::kRotate0, libyuv::FOURCC_I420);

    env->ReleaseByteArrayElements(src_, src_i420_data, 0);
    env->ReleaseByteArrayElements(dst_, dst_i420_data, 0);

    return 0;
}

//初始化视频编码
JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_encoderVideoinit
        (JNIEnv *env, jclass type, jint jwidth, jint jheight, jint joutwidth, jint joutheight)
{
    frameEncoder = new FrameEncoder();
    frameEncoder->setInWidth(jwidth);
    frameEncoder->setInHeight(jheight);
    frameEncoder->setOutWidth(joutwidth);
    frameEncoder->setOutHeight(joutheight);
    frameEncoder->setBitrate(128);
    frameEncoder->open();
    return 0;
}

//视频编码主要函数，注意JNI函数GetByteArrayElements和ReleaseByteArrayElements成对出现，否则回内存泄露
JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_encoderVideoEncode
        (JNIEnv *env, jclass type, jbyteArray jsrcFrame, jint jframeSize, jint counter, jbyteArray jdstFrame, jintArray jdstFrameSize)
{
    jbyte *Src_data = env->GetByteArrayElements(jsrcFrame, NULL);
    jbyte *Dst_data = env->GetByteArrayElements(jdstFrame, NULL);
    jint *dstFrameSize = env->GetIntArrayElements(jdstFrameSize, NULL);

    int numNals = frameEncoder->encodeFrame((char*)Src_data, jframeSize, counter, (char*)Dst_data, dstFrameSize);

    env->ReleaseByteArrayElements(jdstFrame, Dst_data, 0);
    env->ReleaseByteArrayElements(jsrcFrame, Src_data, 0);
    env->ReleaseIntArrayElements(jdstFrameSize, dstFrameSize, 0);

    return numNals;
}

//音频初始化
JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_encoderAudioInit
        (JNIEnv *env, jclass type, jint jsampleRate, jint jchannels, jint jbitRate)
{
    audioEncoder = new AudioEncoder(jchannels, jsampleRate, jbitRate);
    int value = audioEncoder->init();
    return value;
}

JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_encoderAudioEncode
        (JNIEnv *env, jclass type , jbyteArray jsrcFrame, jint jframeSize, jbyteArray jdstFrame, jint jdstSize)
{
    jbyte *Src_data = env->GetByteArrayElements(jsrcFrame, NULL);
    jbyte *Dst_data = env->GetByteArrayElements(jdstFrame, NULL);

    int validlength = audioEncoder->encodeAudio((unsigned char*)Src_data, jframeSize, (unsigned char*)Dst_data, jdstSize);

    env->ReleaseByteArrayElements(jdstFrame, Dst_data, 0);
    env->ReleaseByteArrayElements(jsrcFrame, Src_data, 0);

    return validlength;
}

//初始化rtmp，主要是在RtmpLivePublish类完成的
JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_initRtmpData
        (JNIEnv *env, jclass type, jstring jurl)
{
    const char *url_cstr = env->GetStringUTFChars(jurl, NULL);
    //复制url_cstr内容到rtmp_path
    char *rtmp_path = (char*)malloc(strlen(url_cstr) + 1);
    memset(rtmp_path, 0, strlen(url_cstr) + 1);
    memcpy(rtmp_path, url_cstr, strlen(url_cstr));

    rtmpLivePublish = new RtmpLivePublish();
    rtmpLivePublish->init((unsigned char*)rtmp_path);

    return 0;
}

//发送sps，pps数据
JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_sendRtmpVideoSpsPPS
        (JNIEnv *env, jclass type, jbyteArray jspsArray, jint spsLen, jbyteArray ppsArray, jint ppsLen, jlong jstamp)
{
    if (rtmpLivePublish) {
        jbyte *sps_data = env->GetByteArrayElements(jspsArray, NULL);
        jbyte *pps_data = env->GetByteArrayElements(ppsArray, NULL);

        rtmpLivePublish->addSequenceH264Header((unsigned char*) sps_data, spsLen, (unsigned char*) pps_data, ppsLen);

        env->ReleaseByteArrayElements(jspsArray, sps_data, 0);
        env->ReleaseByteArrayElements(ppsArray, pps_data, 0);
    }
    return 0;
}

//发送视频数据
JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_sendRtmpVideoData
        (JNIEnv *env, jclass type, jbyteArray jvideoData, jint dataLen, jlong jstamp)
{
    if (rtmpLivePublish) {
        jbyte *video_data = env->GetByteArrayElements(jvideoData, NULL);

        rtmpLivePublish->addH264Body((unsigned char*)video_data, dataLen, jstamp);

        env->ReleaseByteArrayElements(jvideoData, video_data, 0);
    }
    return 0;
}

//发送音频Sequence头数据
JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_sendRtmpAudioSpec
        (JNIEnv *env, jclass type, jlong jstamp)
{
    if (rtmpLivePublish) {
        rtmpLivePublish->addSequenceAacHeader(44100, 2, 0);
    }
    return 0;
}

//发送音频Audio数据
JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_sendRtmpAudioData
        (JNIEnv *env, jclass type, jbyteArray jaudiodata, jint dataLen, jlong jstamp)
{
    if (rtmpLivePublish) {
        jbyte *audio_data = env->GetByteArrayElements(jaudiodata, NULL);

        rtmpLivePublish->addAccBody((unsigned char*) audio_data, dataLen, jstamp);

        env->ReleaseByteArrayElements(jaudiodata, audio_data, 0);
    }
    return 0;
}

//释放RTMP连接
JNIEXPORT jint JNICALL Java_com_riemannlee_liveproject_StreamProcessManager_releaseRtmp
        (JNIEnv *env, jclass type)
{
    if (rtmpLivePublish) {
        rtmpLivePublish->release();
    }
    return 0;
}