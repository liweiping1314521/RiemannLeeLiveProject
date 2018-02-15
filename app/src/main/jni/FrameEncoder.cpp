#include <android/log.h>
#include "FrameEncoder.h"
extern "C" {
#include <libswscale/swscale.h>
}

#define  TAG "RiemannLee"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)

//供测试文件使用,测试的时候打开
//#define ENCODE_OUT_FILE_1
//供测试文件使用
//#define ENCODE_OUT_FILE_2

FrameEncoder::FrameEncoder() : in_width(0), in_height(0), out_width(
        0), out_height(0), fps(0), encoder(NULL), num_nals(0) {

#ifdef ENCODE_OUT_FILE_1
    const char *outfile1 = "/sdcard/2222.h264";
    out1 = fopen(outfile1, "wb");
#endif

#ifdef ENCODE_OUT_FILE_2
    const char *outfile2 = "/sdcard/3333.h264";
    out2 = fopen(outfile2, "wb");
#endif
}

FrameEncoder::~FrameEncoder() {
}

bool FrameEncoder::open() {
    int r = 0;
    int nheader = 0;
    int header_size = 0;

    if (!validateSettings()) {
        return false;
    }

    if (encoder) {
        LOGI("Already opened. first call close()");
        return false;
    }

    // set encoder parameters
    setParams();
    //按照色度空间分配内存，即为图像结构体x264_picture_t分配内存，并返回内存的首地址作为指针
    //i_csp(图像颜色空间参数，目前只支持I420/YUV420)为X264_CSP_I420
    x264_picture_alloc(&pic_in, params.i_csp, params.i_width, params.i_height);
    //create the encoder using our params 打开编码器
    encoder = x264_encoder_open(&params);

    if (!encoder) {
        LOGI("Cannot open the encoder");
        close();
        return false;
    }

    // write headers
    r = x264_encoder_headers(encoder, &nals, &nheader);
    if (r < 0) {
        LOGI("x264_encoder_headers() failed");
        return false;
    }

    return true;
}

int FrameEncoder::encodeFrame(char* inBytes, int frameSize, int pts,
                               char* outBytes, int *outFrameSize) {
    //YUV420P数据转化为h264
    int i420_y_size = in_width * in_height;
    int i420_u_size = (in_width >> 1) * (in_height >> 1);
    int i420_v_size = i420_u_size;

    uint8_t *i420_y_data = (uint8_t *)inBytes;
    uint8_t *i420_u_data = (uint8_t *)inBytes + i420_y_size;
    uint8_t *i420_v_data = (uint8_t *)inBytes + i420_y_size + i420_u_size;
    //将Y,U,V数据保存到pic_in.img的对应的分量中，还有一种方法是用AV_fillPicture和sws_scale来进行变换
    memcpy(pic_in.img.plane[0], i420_y_data, i420_y_size);
    memcpy(pic_in.img.plane[1], i420_u_data, i420_u_size);
    memcpy(pic_in.img.plane[2], i420_v_data, i420_v_size);

    // and encode and store into pic_out
    pic_in.i_pts = pts;
    //最主要的函数，x264编码，pic_in为x264输入，pic_out为x264输出
    int frame_size = x264_encoder_encode(encoder, &nals, &num_nals, &pic_in,
                                         &pic_out);

    if (frame_size) {
        /*Here first four bytes proceeding the nal unit indicates frame length*/
        int have_copy = 0;
        //编码后，h264数据保存为nal了，我们可以获取到nals[i].type的类型判断是sps还是pps
        //或者是否是关键帧，nals[i].i_payload表示数据长度，nals[i].p_payload表示存储的数据
        //编码后，我们按照nals[i].i_payload的长度来保存copy h264数据的，然后抛给java端用作
        //rtmp发送数据，outFrameSize是变长的，当有sps pps的时候大于1，其它时候值为1
        for (int i = 0; i < num_nals; i++) {
            outFrameSize[i] = nals[i].i_payload;
            memcpy(outBytes + have_copy, nals[i].p_payload, nals[i].i_payload);
            have_copy += nals[i].i_payload;
        }
#ifdef ENCODE_OUT_FILE_1
        fwrite(outBytes, 1, frame_size, out1);
#endif

#ifdef ENCODE_OUT_FILE_2
        for (int i = 0; i < frame_size; i++) {
            outBytes[i] = (char) nals[0].p_payload[i];
        }
        fwrite(outBytes, 1, frame_size, out2);
        *outFrameSize = frame_size;
#endif

        return num_nals;
    }
    return -1;
}

bool FrameEncoder::close() {
    if (encoder) {
        x264_picture_clean(&pic_in);
        memset((char*) &pic_in, 0, sizeof(pic_in));
        memset((char*) &pic_out, 0, sizeof(pic_out));

        x264_encoder_close(encoder);
        encoder = NULL;
    }

#ifdef ENCODE_OUT_FILE_1
    if (out1) {
        fclose(out1);
    }
#endif
#ifdef ENCODE_OUT_FILE_2
    if (out2) {
        fclose(out2);
    }
#endif

    return true;
}

void FrameEncoder::setParams() {
    //preset
    //默认：medium
    //一些在压缩效率和运算时间中平衡的预设值。如果指定了一个预设值，它会在其它选项生效前生效。
    //可选：ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow and placebo.
    //建议：可接受的最慢的值
    //tune
    //默认：无
    //说明：在上一个选项基础上进一步优化输入。如果定义了一个tune值，它将在preset之后，其它选项之前生效。
    //可选：film, animation, grain, stillimage, psnr, ssim, fastdecode, zerolatency and touhou.
    //建议：根据输入选择。如果没有合适的就不要指定。
    //后来发现设置x264_param_default_preset(&param, "fast" , "zerolatency" );后就能即时编码了
    x264_param_default_preset(&params, "veryfast", "zerolatency");

    //I帧间隔
    params.i_csp = X264_CSP_I420;
    params.i_width = getOutWidth();
    params.i_height = getOutHeight();

    //并行编码多帧
    params.i_threads = X264_SYNC_LOOKAHEAD_AUTO;
    params.i_fps_num = 25;//getFps();
    params.i_fps_den = 1;

    // B frames 两个相关图像间B帧的数目 */
    params.i_bframe = 5;//getBFrameFrq();
    params.b_sliced_threads = true;
    params.b_vfr_input = 0;
    params.i_timebase_num = params.i_fps_den;
    params.i_timebase_den = params.i_fps_num;

    // Intra refres:
    params.i_keyint_max = 25;
    params.i_keyint_min = 1;
    params.b_intra_refresh = 1;

    //参数i_rc_method表示码率控制，CQP(恒定质量)，CRF(恒定码率)，ABR(平均码率)
    //恒定码率，会尽量控制在固定码率
    params.rc.i_rc_method = X264_RC_CRF;
    //图像质量控制,rc.f_rf_constant是实际质量，越大图像越花，越小越清晰
    //param.rc.f_rf_constant_max ，图像质量的最大值
    params.rc.f_rf_constant = 25;
    params.rc.f_rf_constant_max = 35;

    // For streaming:
    //* 码率(比特率,单位Kbps)x264使用的bitrate需要/1000
    params.rc.i_bitrate = getBitrate() / 1000;
    //瞬时最大码率,平均码率模式下，最大瞬时码率，默认0(与-B设置相同)
    params.rc.i_vbv_max_bitrate = getBitrate() / 1000 * 1.2;
    params.b_repeat_headers = 1;
    params.b_annexb = 1;

    //是否把SPS和PPS放入每一个关键帧
    //SPS Sequence Parameter Set 序列参数集，PPS Picture Parameter Set 图像参数集
    //为了提高图像的纠错能力,该参数设置是让每个I帧都附带sps/pps。
    //param.b_repeat_headers = 1;
    //设置Level级别,编码复杂度
    params.i_level_idc = 51;

    //profile
    //默认：无
    //说明：限制输出文件的profile。这个参数将覆盖其它所有值，此选项能保证输出profile兼容的视频流。如果使用了这个选项，将不能进行无损压缩（qp 0 or crf 0）。
    //可选：baseline，main，high
    //建议：不设置。除非解码环境只支持main或者baseline profile的解码。
    x264_param_apply_profile(&params, "baseline");
}

bool FrameEncoder::validateSettings() {
    if (!in_width) {
        LOGI("No in_width set");
        return false;
    }
    if (!in_height) {
        LOGI("No in_height set");
        return false;
    }
    if (!out_width) {
        LOGI("No out_width set");
        return false;
    }
    if (!out_height) {
        LOGI("No out_height set");
        return false;
    }

    return true;
}

int FrameEncoder::getFps() const {
    return fps;
}

void FrameEncoder::setFps(int fps) {
    this->fps = fps;
}

int FrameEncoder::getInHeight() const {
    return in_height;
}

void FrameEncoder::setInHeight(int inHeight) {
    in_height = inHeight;
}

int FrameEncoder::getInWidth() const {
    return in_width;
}

void FrameEncoder::setInWidth(int inWidth) {
    in_width = inWidth;
}

int FrameEncoder::getNumNals() const {
    return num_nals;
}

void FrameEncoder::setNumNals(int numNals) {
    num_nals = numNals;
}

int FrameEncoder::getOutHeight() const {
    return out_height;
}

void FrameEncoder::setOutHeight(int outHeight) {
    out_height = outHeight;
}

int FrameEncoder::getOutWidth() const {
    return out_width;
}

void FrameEncoder::setOutWidth(int outWidth) {
    out_width = outWidth;
}

int FrameEncoder::getBitrate() const {
    return bitrate;
}

void FrameEncoder::setBitrate(int bitrate) {
    this->bitrate = bitrate;
}

int FrameEncoder::getSliceMaxSize() const {
    return i_slice_max_size;
}

void FrameEncoder::setSliceMaxSize(int sliceMaxSize) {
    i_slice_max_size = sliceMaxSize;
}

int FrameEncoder::getVbvBufferSize() const {
    return i_vbv_buffer_size;
}

void FrameEncoder::setVbvBufferSize(int vbvBufferSize) {
    i_vbv_buffer_size = vbvBufferSize;
}

int FrameEncoder::getIThreads() const {
    return i_threads;
}

void FrameEncoder::setIThreads(int threads) {
    i_threads = threads;
}

int FrameEncoder::getBFrameFrq() const {
    return b_frame_frq;
}

void FrameEncoder::setBFrameFrq(int frameFrq) {
    b_frame_frq = frameFrq;
}
