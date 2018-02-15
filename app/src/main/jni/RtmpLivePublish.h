#ifndef RIEMANNLEELIVEPROJECT_RTMPLIVEPUBLISH_H
#define RIEMANNLEELIVEPROJECT_RTMPLIVEPUBLISH_H

#include <pthread.h>
#include <jni.h>

extern "C" {
#include <librtmp/rtmp.h>
}

class RtmpLivePublish {

public:
    unsigned char* rtmp_url;
    unsigned int start_time;
    int getSampleRateIndex(int sampleRate);
    RTMP *rtmp;

    RtmpLivePublish();
    ~RtmpLivePublish();
    void init(unsigned char* url);
    void addSequenceAacHeader(int timestamp , int sampleRate, int channel);
    void addAccBody(unsigned char* buf, int len, long timeStamp);
    void addSequenceH264Header(unsigned char* sps, int sps_len, unsigned char *pps, int pps_len);
    void addH264Body(unsigned char* buf, int len, long timeStamp);
    void release();
};


#endif //RIEMANNLEELIVEPROJECT_RTMPLIVEPUBLISH_H
