#ifndef RIEMANNLEELIVEPROJECT_FRAMEENCODER_H
#define RIEMANNLEELIVEPROJECT_FRAMEENCODER_H

#include "RtmpLivePublish.h"

extern "C" {
#include <libavutil/pixfmt.h>
#include <libavcodec/avcodec.h>
#include <x264.h>
}

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

class FrameEncoder {

private:
    int in_width;
    int in_height;
    int out_width;
    int out_height;
    /* e.g. 25, 60, etc.. */
    int fps;
    int bitrate;
    int i_threads;
    int i_vbv_buffer_size;
    int i_slice_max_size;
    int b_frame_frq;

    /* x264 struct*/
    x264_picture_t pic_in;
    x264_picture_t pic_out;
    x264_param_t params;
    x264_nal_t* nals;
    x264_t* encoder;
    int num_nals;

    FILE *out1;
    FILE *out2;

public:
    FrameEncoder();
    ~FrameEncoder();
    /* open for encoding */
    bool open();
    /* encode the given data */
    int encodeFrame(char* inBytes, int frameSize, int pts, char* outBytes, int *outFrameSize);
    /* close the encoder and file, frees all memory */
    bool close();
    /* validates if all params are set correctly, like width,height, etc.. */
    bool validateSettings();
    /* sets the x264 params */
    void setParams();
    int getFps() const;
    void setFps(int fps);
    int getInHeight() const;
    void setInHeight(int inHeight);
    int getInWidth() const;
    void setInWidth(int inWidth);
    int getNumNals() const;
    void setNumNals(int numNals);
    int getOutHeight() const;
    void setOutHeight(int outHeight);
    int getOutWidth() const;
    void setOutWidth(int outWidth);
    int getBitrate() const;
    void setBitrate(int bitrate);
    int getSliceMaxSize() const;
    void setSliceMaxSize(int sliceMaxSize);
    int getVbvBufferSize() const;
    void setVbvBufferSize(int vbvBufferSize);
    int getIThreads() const;
    void setIThreads(int threads);
    int getBFrameFrq() const;
    void setBFrameFrq(int frameFrq);
};


#endif //RIEMANNLEELIVEPROJECT_FRAMEENCODER_H
