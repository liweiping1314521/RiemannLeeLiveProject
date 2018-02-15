#include <android/log.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <fdk-aac/aacenc_lib.h>
#include "AudioEncoder.h"
#include "wavreader.h"

#define  TAG "RiemannLee"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)

AudioEncoder::AudioEncoder(int channels, int sampleRate, int bitRate)
{
    this->channels = channels;
    this->sampleRate = sampleRate;
    this->bitRate = bitRate;
}

AudioEncoder::~AudioEncoder() {
    close();
}

/**
 * 初始化fdk-aac的参数，设置相关接口使得
 * @return
 */
int AudioEncoder::init() {
    //打开AAC音频编码引擎，创建AAC编码句柄
    if (aacEncOpen(&handle, 0, channels) != AACENC_OK) {
        LOGI("Unable to open fdkaac encoder\n");
        return -1;
    }

    // AACENC_AOT设置为aac lc
    if (aacEncoder_SetParam(handle, AACENC_AOT, 2) != AACENC_OK) {
        LOGI("Unable to set the AOT\n");
        return -1;
    }

    if (aacEncoder_SetParam(handle, AACENC_SAMPLERATE, sampleRate) != AACENC_OK) {
        LOGI("Unable to set the sampleRate\n");
        return -1;
    }

    // AACENC_CHANNELMODE设置为双通道
    if (aacEncoder_SetParam(handle, AACENC_CHANNELMODE, MODE_2) != AACENC_OK) {
        LOGI("Unable to set the channel mode\n");
        return -1;
    }

    if (aacEncoder_SetParam(handle, AACENC_CHANNELORDER, 1) != AACENC_OK) {
        LOGI("Unable to set the wav channel order\n");
        return 1;
    }
    if (aacEncoder_SetParam(handle, AACENC_BITRATE, bitRate) != AACENC_OK) {
        LOGI("Unable to set the bitrate\n");
        return -1;
    }
    if (aacEncoder_SetParam(handle, AACENC_TRANSMUX, 2) != AACENC_OK) { //0-raw 2-adts
        LOGI("Unable to set the ADTS transmux\n");
        return -1;
    }

    if (aacEncoder_SetParam(handle, AACENC_AFTERBURNER, 1) != AACENC_OK) {
        LOGI("Unable to set the ADTS AFTERBURNER\n");
        return -1;
    }

    if (aacEncEncode(handle, NULL, NULL, NULL, NULL) != AACENC_OK) {
        LOGI("Unable to initialize the encoder\n");
        return -1;
    }

    AACENC_InfoStruct info = { 0 };
    if (aacEncInfo(handle, &info) != AACENC_OK) {
        LOGI("Unable to get the encoder info\n");
        return -1;
    }

    //返回数据给上层，表示每次传递多少个数据最佳，这样encode效率最高
    int inputSize = channels * 2 * info.frameLength;
    LOGI("inputSize = %d", inputSize);

    return inputSize;
}

/**
 * Fdk-AAC库压缩裸音频PCM数据，转化为AAC，这里为什么用fdk-aac，这个库相比普通的aac库，压缩效率更高
 * @param inBytes
 * @param length
 * @param outBytes
 * @param outLength
 * @return
 */
int AudioEncoder::encodeAudio(unsigned char *inBytes, int length, unsigned char *outBytes, int outLength) {
    void *in_ptr, *out_ptr;
    AACENC_BufDesc in_buf = {0};
    int in_identifier = IN_AUDIO_DATA;
    int in_elem_size = 2;
    //传递input数据给in_buf
    in_ptr = inBytes;
    in_buf.bufs = &in_ptr;
    in_buf.numBufs = 1;
    in_buf.bufferIdentifiers = &in_identifier;
    in_buf.bufSizes = &length;
    in_buf.bufElSizes = &in_elem_size;

    AACENC_BufDesc out_buf = {0};
    int out_identifier = OUT_BITSTREAM_DATA;
    int elSize = 1;
    //out数据放到out_buf中
    out_ptr = outBytes;
    out_buf.bufs = &out_ptr;
    out_buf.numBufs = 1;
    out_buf.bufferIdentifiers = &out_identifier;
    out_buf.bufSizes = &outLength;
    out_buf.bufElSizes = &elSize;

    AACENC_InArgs in_args = {0};
    in_args.numInSamples = length / 2;  //size为pcm字节数

    AACENC_OutArgs out_args = {0};
    AACENC_ERROR err;

    //利用aacEncEncode来编码PCM裸音频数据，上面的代码都是fdk-aac的流程步骤
    if ((err = aacEncEncode(handle, &in_buf, &out_buf, &in_args, &out_args)) != AACENC_OK) {
        LOGI("Encoding aac failed\n");
        return err;
    }
    //返回编码后的有效字段长度
    return out_args.numOutBytes;
}

bool AudioEncoder::close() {
    if (handle) {
        aacEncClose(&handle);
        handle = NULL;
    }
    return true;
}

/**
 * 测试接口，直接可以录音，来保证PCM encode后数据是否可以播放
 * @return
 */
int AudioEncoder::encodePCMAudioFile() {
    int bitrate = 64000;
    int ch;
    const char *pcmfile, *outfile;
    FILE *out, *inFile;
    int format = 1, sample_rate = 44100, bits_per_sample = 16;
    int input_size;
    uint8_t* input_buf;
    int16_t* convert_buf;
    int aot = 2;
    int afterburner = 1;
    int eld_sbr = 0;
    int vbr = 0;
    HANDLE_AACENCODER handle;
    CHANNEL_MODE mode;
    AACENC_InfoStruct info = { 0 };
    int channels = 2;

    pcmfile = "/sdcard/789.pcm";
    outfile = "/sdcard/888.aac";

    inFile = fopen(pcmfile, "rb");
    if (!inFile) {
        perror(pcmfile);
        return 1;
    }
    if (format != 1) {
        LOGI("Unsupported WAV format %d\n", format);
        return 1;
    }
    if (bits_per_sample != 16) {
        LOGI("Unsupported WAV sample depth %d\n", bits_per_sample);
        return 1;
    }
    switch (channels) {
        case 1: mode = MODE_1;       break;
        case 2: mode = MODE_2;       break;
        case 3: mode = MODE_1_2;     break;
        case 4: mode = MODE_1_2_1;   break;
        case 5: mode = MODE_1_2_2;   break;
        case 6: mode = MODE_1_2_2_1; break;
        default:
            LOGI("Unsupported WAV channels %d\n", channels);
            return 1;
    }
    if (aacEncOpen(&handle, 0, channels) != AACENC_OK) {
        LOGI("Unable to open encoder\n");
        return 1;
    }
    if (aacEncoder_SetParam(handle, AACENC_AOT, aot) != AACENC_OK) {
        LOGI("Unable to set the AOT\n");
        return 1;
    }
    if (aot == 39 && eld_sbr) {
        if (aacEncoder_SetParam(handle, AACENC_SBR_MODE, 1) != AACENC_OK) {
            fprintf(stderr, "Unable to set SBR mode for ELD\n");
            return 1;
        }
    }
    if (aacEncoder_SetParam(handle, AACENC_SAMPLERATE, sample_rate) != AACENC_OK) {
        LOGI("Unable to set the AOT\n");
        return 1;
    }
    if (aacEncoder_SetParam(handle, AACENC_CHANNELMODE, mode) != AACENC_OK) {
        LOGI("Unable to set the channel mode\n");
        return 1;
    }
    if (aacEncoder_SetParam(handle, AACENC_CHANNELORDER, 1) != AACENC_OK) {
        LOGI("Unable to set the wav channel order\n");
        return 1;
    }
    if (vbr) {
        if (aacEncoder_SetParam(handle, AACENC_BITRATEMODE, vbr) != AACENC_OK) {
            LOGI("Unable to set the VBR bitrate mode\n");
            return 1;
        }
    } else {
        if (aacEncoder_SetParam(handle, AACENC_BITRATE, bitrate) != AACENC_OK) {
            LOGI("Unable to set the bitrate\n");
            return 1;
        }
    }
    if (aacEncoder_SetParam(handle, AACENC_TRANSMUX, 2) != AACENC_OK) {
        LOGI("Unable to set the ADTS transmux\n");
        return 1;
    }
    if (aacEncoder_SetParam(handle, AACENC_AFTERBURNER, afterburner) != AACENC_OK) {
        LOGI("Unable to set the afterburner mode\n");
        return 1;
    }
    if (aacEncEncode(handle, NULL, NULL, NULL, NULL) != AACENC_OK) {
        LOGI("Unable to initialize the encoder\n");
        return 1;
    }
    if (aacEncInfo(handle, &info) != AACENC_OK) {
        LOGI("Unable to get the encoder info\n");
        return 1;
    }

    out = fopen(outfile, "wb");
    if (!out) {
        perror(outfile);
        return 1;
    }

    input_size = channels*2*info.frameLength;
    input_buf = (uint8_t*) malloc(input_size);
    convert_buf = (int16_t*) malloc(input_size);
    LOGI("input_size %d", input_size);

    while (1) {
        AACENC_BufDesc in_buf = { 0 }, out_buf = { 0 };
        AACENC_InArgs in_args = { 0 };
        AACENC_OutArgs out_args = { 0 };
        int in_identifier = IN_AUDIO_DATA;
        int in_size, in_elem_size;
        int out_identifier = OUT_BITSTREAM_DATA;
        int out_size, out_elem_size;
        int read, i;
        void *in_ptr, *out_ptr;
        uint8_t outbuf[20480];
        AACENC_ERROR err;

        read = fread(input_buf, 1, input_size, inFile);
        encodeAudio(input_buf, input_size, outbuf, out_size);
        if (read <= 0) {
            in_args.numInSamples = -1;
        } else {
            in_ptr = input_buf;
            in_size = read;
            in_elem_size = 2;

            in_args.numInSamples = read/2;
            in_buf.numBufs = 1;
            in_buf.bufs = &in_ptr;
            in_buf.bufferIdentifiers = &in_identifier;
            in_buf.bufSizes = &in_size;
            in_buf.bufElSizes = &in_elem_size;
        }
        out_ptr = outbuf;
        out_size = sizeof(outbuf);
        out_elem_size = 1;
        out_buf.numBufs = 1;
        out_buf.bufs = &out_ptr;
        out_buf.bufferIdentifiers = &out_identifier;
        out_buf.bufSizes = &out_size;
        out_buf.bufElSizes = &out_elem_size;

        if ((err = aacEncEncode(handle, &in_buf, &out_buf, &in_args, &out_args)) != AACENC_OK) {
            if (err == AACENC_ENCODE_EOF)
                break;
            fprintf(stderr, "Encoding failed\n");
            return 1;
        }

        if (out_args.numOutBytes == 0)
            continue;
        fwrite(outbuf, 1, out_args.numOutBytes, out);
    }
    free(input_buf);
    free(convert_buf);
    fclose(inFile);
    fclose(out);
    aacEncClose(&handle);

    return 0;
}

/**
 * 测试接口，直接可以录音，来保证encode后数据是否可以播放
 * @return
 */
int AudioEncoder::encodeWAVAudioFile() {
    int bitrate = 64000;
    int ch;
    const char *infile, *outfile;
    FILE *out;
    void *wav;
    int format, sample_rate, bits_per_sample;
    int input_size;
    uint8_t* input_buf;
    int16_t* convert_buf;
    int aot = 2;
    int afterburner = 1;
    int eld_sbr = 0;
    int vbr = 0;
    HANDLE_AACENCODER handle;
    CHANNEL_MODE mode;
    AACENC_InfoStruct info = { 0 };
    int channels = 2;

    infile = "/sdcard/789.wav";
    outfile = "/sdcard/888.aac";

    wav = wav_read_open(infile);
    if (!wav) {
        LOGI("Unable to open wav file %s\n", infile);
        return 1;
    }
    if (!wav_get_header(wav, &format, &channels, &sample_rate, &bits_per_sample, NULL)) {
        LOGI("Bad wav file %s\n", infile);
        return 1;
    }
    if (format != 1) {
        LOGI("Unsupported WAV format %d\n", format);
        return 1;
    }
    if (bits_per_sample != 16) {
        LOGI("Unsupported WAV sample depth %d\n", bits_per_sample);
        return 1;
    }
    switch (channels) {
        case 1: mode = MODE_1;       break;
        case 2: mode = MODE_2;       break;
        case 3: mode = MODE_1_2;     break;
        case 4: mode = MODE_1_2_1;   break;
        case 5: mode = MODE_1_2_2;   break;
        case 6: mode = MODE_1_2_2_1; break;
        default:
            LOGI("Unsupported WAV channels %d\n", channels);
            return 1;
    }
    if (aacEncOpen(&handle, 0, channels) != AACENC_OK) {
        LOGI("Unable to open encoder\n");
        return 1;
    }
    if (aacEncoder_SetParam(handle, AACENC_AOT, aot) != AACENC_OK) {
        LOGI("Unable to set the AOT\n");
        return 1;
    }
    if (aot == 39 && eld_sbr) {
        if (aacEncoder_SetParam(handle, AACENC_SBR_MODE, 1) != AACENC_OK) {
            fprintf(stderr, "Unable to set SBR mode for ELD\n");
            return 1;
        }
    }
    if (aacEncoder_SetParam(handle, AACENC_SAMPLERATE, sample_rate) != AACENC_OK) {
        LOGI("Unable to set the AOT\n");
        return 1;
    }
    if (aacEncoder_SetParam(handle, AACENC_CHANNELMODE, mode) != AACENC_OK) {
        LOGI("Unable to set the channel mode\n");
        return 1;
    }
    if (aacEncoder_SetParam(handle, AACENC_CHANNELORDER, 1) != AACENC_OK) {
        LOGI("Unable to set the wav channel order\n");
        return 1;
    }
    if (vbr) {
        if (aacEncoder_SetParam(handle, AACENC_BITRATEMODE, vbr) != AACENC_OK) {
            LOGI("Unable to set the VBR bitrate mode\n");
            return 1;
        }
    } else {
        if (aacEncoder_SetParam(handle, AACENC_BITRATE, bitrate) != AACENC_OK) {
            LOGI("Unable to set the bitrate\n");
            return 1;
        }
    }
    if (aacEncoder_SetParam(handle, AACENC_TRANSMUX, 2) != AACENC_OK) {
        LOGI("Unable to set the ADTS transmux\n");
        return 1;
    }
    if (aacEncoder_SetParam(handle, AACENC_AFTERBURNER, afterburner) != AACENC_OK) {
        LOGI("Unable to set the afterburner mode\n");
        return 1;
    }
    if (aacEncEncode(handle, NULL, NULL, NULL, NULL) != AACENC_OK) {
        LOGI("Unable to initialize the encoder\n");
        return 1;
    }
    if (aacEncInfo(handle, &info) != AACENC_OK) {
        LOGI("Unable to get the encoder info\n");
        return 1;
    }

    out = fopen(outfile, "wb");
    if (!out) {
        perror(outfile);
        return 1;
    }

    input_size = channels*2*info.frameLength;
    input_buf = (uint8_t*) malloc(input_size);
    convert_buf = (int16_t*) malloc(input_size);
    LOGI("input_size %d", input_size);

    while (1) {
        AACENC_BufDesc in_buf = { 0 }, out_buf = { 0 };
        AACENC_InArgs in_args = { 0 };
        AACENC_OutArgs out_args = { 0 };
        int in_identifier = IN_AUDIO_DATA;
        int in_size, in_elem_size;
        int out_identifier = OUT_BITSTREAM_DATA;
        int out_size, out_elem_size;
        int read, i;
        void *in_ptr, *out_ptr;
        uint8_t outbuf[20480];
        AACENC_ERROR err;

        read = wav_read_data(wav, input_buf, input_size);
        for (i = 0; i < read/2; i++) {
            const uint8_t* in = &input_buf[2 * i];
            convert_buf[i] = in[0] | (in[1] << 8);
            LOGI("convert_buf[%d] = %d ", i, convert_buf[i]);
        }
        if (read <= 0) {
            in_args.numInSamples = -1;
        } else {
            in_ptr = convert_buf;
            in_size = read;
            in_elem_size = 2;

            in_args.numInSamples = read/2;
            in_buf.numBufs = 1;
            in_buf.bufs = &in_ptr;
            in_buf.bufferIdentifiers = &in_identifier;
            in_buf.bufSizes = &in_size;
            in_buf.bufElSizes = &in_elem_size;
        }
        out_ptr = outbuf;
        out_size = sizeof(outbuf);
        out_elem_size = 1;
        out_buf.numBufs = 1;
        out_buf.bufs = &out_ptr;
        out_buf.bufferIdentifiers = &out_identifier;
        out_buf.bufSizes = &out_size;
        out_buf.bufElSizes = &out_elem_size;

        if ((err = aacEncEncode(handle, &in_buf, &out_buf, &in_args, &out_args)) != AACENC_OK) {
            if (err == AACENC_ENCODE_EOF)
                break;
            fprintf(stderr, "Encoding failed\n");
            return 1;
        }

        if (out_args.numOutBytes == 0)
            continue;
        fwrite(outbuf, 1, out_args.numOutBytes, out);
    }
    free(input_buf);
    free(convert_buf);
    fclose(out);
    wav_read_close(wav);
    aacEncClose(&handle);

    return 0;
}