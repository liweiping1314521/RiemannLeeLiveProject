#ifndef RIEMANNLEELIVEPROJECT_AUDIOENCODER_H
#define RIEMANNLEELIVEPROJECT_AUDIOENCODER_H

#include "RtmpLivePublish.h"

extern "C" {
#include <fdk-aac/aacenc_lib.h>
}

class AudioEncoder {

private:
    int sampleRate;
    int channels;
    int bitRate;
    HANDLE_AACENCODER handle;

public:
    AudioEncoder(int channels, int sampleRate, int bitRate);
    ~AudioEncoder();
    int init();

    int encodeAudio(unsigned char* inBytes, int length, unsigned char* outBytes, int outlength);
    int encodeWAVAudioFile();
    int encodePCMAudioFile();
    bool close();

};


#endif //RIEMANNLEELIVEPROJECT_AUDIOENCODER_H
