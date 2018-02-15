package com.riemannlee.liveproject;

import android.util.Log;

import com.riemannlee.liveproject.audio.AudioData;
import com.riemannlee.liveproject.camera.manager.VideoData;

import java.util.concurrent.LinkedBlockingQueue;

/**
 * 编码类MediaEncoder，主要是把视频流YUV420P格式编码为h264格式,把PCM裸音频转化为AAC格式
 */
public class MediaEncoder {
    private static final String TAG = "MediaEncoder";

    private Thread videoEncoderThread, audioEncoderThread;
    private boolean videoEncoderLoop, audioEncoderLoop;

    //视频流队列
    private LinkedBlockingQueue<VideoData> videoQueue;
    //音频流队列
    private LinkedBlockingQueue<AudioData> audioQueue;

    private FileManager videoFileManager;
    private FileManager audioFileManager;
    private static final boolean SAVE_FILE_FOR_TEST = true;
    private int fps = 0;
    private int audioEncodeBuffer;

    private static MediaEncoderCallback sMediaEncoderCallback;
    public interface MediaEncoderCallback {
        void receiveEncoderVideoData(byte[] videoData, int totalLength, int[] segment);
        void receiveEncoderAudioData(byte[] audioData, int size);
    }

    public static void setsMediaEncoderCallback(MediaEncoderCallback callback) {
        sMediaEncoderCallback = callback;
    }

    public MediaEncoder() {
        if (SAVE_FILE_FOR_TEST) {
            videoFileManager = new FileManager(FileManager.TEST_H264_FILE);
            audioFileManager = new FileManager(FileManager.TEST_AAC_FILE);
        }
        videoQueue = new LinkedBlockingQueue<>();
        audioQueue = new LinkedBlockingQueue<>();
        //这里我们初始化音频数据，为什么要初始化音频数据呢？音频数据里面我们做了什么事情？
        audioEncodeBuffer = StreamProcessManager.encoderAudioInit(Contacts.SAMPLE_RATE,
                Contacts.CHANNELS, Contacts.BIT_RATE);
    }

    public void start() {
        startAudioEncode();
        startVideoEncode();
    }

    public void stop() {
        stopAudioEncode();
        stopVideoEncode();
        saveFileForTest();
    }

    //摄像头的YUV420P数据，put到队列中，生产者模型
    public void putVideoData(VideoData videoData) {
        try {
            videoQueue.put(videoData);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    //麦克风PCM音频数据，put到队列中，生产者模型
    public void putAudioData(AudioData audioData) {
        try {
            audioQueue.put(audioData);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public void stopVideoEncode() {
        videoEncoderLoop = false;
        videoEncoderThread.interrupt();
    }

    public void stopAudioEncode() {
        audioEncoderLoop = false;
        audioEncoderThread.interrupt();
    }

    public void startVideoEncode() {
        if (videoEncoderLoop) {
            throw new RuntimeException("必须先停止");
        }

        videoEncoderThread = new Thread() {
            @Override
            public void run() {
                //视频消费者模型，不断从队列中取出视频流来进行h264编码
                while (videoEncoderLoop && !Thread.interrupted()) {
                    try {
                        //队列中取视频数据
                        VideoData videoData = videoQueue.take();
                        fps++;
                        byte[] outbuffer = new byte[videoData.width * videoData.height];
                        int[] buffLength = new int[10];
                        //对YUV420P进行h264编码，返回一个数据大小，里面是编码出来的h264数据
                        int numNals = StreamProcessManager.encoderVideoEncode(videoData.videoData, videoData.videoData.length, fps, outbuffer, buffLength);
                        //Log.e("RiemannLee", "data.length " +  videoData.videoData.length + " h264 encode length " + buffLength[0]);
                        if (numNals > 0) {
                            int[] segment = new int[numNals];
                            System.arraycopy(buffLength, 0, segment, 0, numNals);
                            int totalLength = 0;
                            for (int i = 0; i < segment.length; i++) {
                                totalLength += segment[i];
                            }
                            //Log.i("RiemannLee", "###############totalLength " + totalLength);
                            //编码后的h264数据
                            byte[] encodeData = new byte[totalLength];
                            System.arraycopy(outbuffer, 0, encodeData, 0, encodeData.length);
                            if (sMediaEncoderCallback != null) {
                                sMediaEncoderCallback.receiveEncoderVideoData(encodeData, encodeData.length, segment);
                            }
                            //我们可以把数据在java层保存到文件中，看看我们编码的h264数据是否能播放，h264裸数据可以在VLC播放器中播放
                            if (SAVE_FILE_FOR_TEST) {
                                videoFileManager.saveFileData(encodeData);
                            }
                        }
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                        break;
                    }
                }

            }
        };
        videoEncoderLoop = true;
        videoEncoderThread.start();
    }

    public void startAudioEncode() {
        if (audioEncoderLoop) {
            throw new RuntimeException("必须先停止");
        }
        audioEncoderThread = new Thread() {
            @Override
            public void run() {
                byte[] outbuffer = new byte[1024];
                int haveCopyLength = 0;
                byte[] inbuffer = new byte[audioEncodeBuffer];
                while (audioEncoderLoop && !Thread.interrupted()) {
                    try {
                        AudioData audio = audioQueue.take();
                        //我们通过fdk-aac接口获取到了audioEncodeBuffer的数据，即每次编码多少数据为最优
                        //这里我这边的手机每次都是返回的4096即4K的数据，其实为了简单点，我们每次可以让
                        //MIC录取4K大小的数据，然后把录取的数据传递到AudioEncoder.cpp中取编码
                        //Log.e("RiemannLee", " audio.audioData.length " + audio.audioData.length + " audioEncodeBuffer " + audioEncodeBuffer);
                        final int audioGetLength = audio.audioData.length;
                        if (haveCopyLength < audioEncodeBuffer) {
                            System.arraycopy(audio.audioData, 0, inbuffer, haveCopyLength, audioGetLength);
                            haveCopyLength += audioGetLength;
                            int remain = audioEncodeBuffer - haveCopyLength;
                            if (remain == 0) {
                                //fdk-aac编码PCM裸音频数据，返回可用长度的有效字段
                                int validLength = StreamProcessManager.encoderAudioEncode(inbuffer, audioEncodeBuffer, outbuffer, outbuffer.length);
                                //Log.e("lihuzi", " validLength " + validLength);
                                final int VALID_LENGTH = validLength;
                                if (VALID_LENGTH > 0) {
                                    byte[] encodeData = new byte[VALID_LENGTH];
                                    System.arraycopy(outbuffer, 0, encodeData, 0, VALID_LENGTH);
                                    if (sMediaEncoderCallback != null) {
                                        //编码后，把数据抛给rtmp去推流
                                        sMediaEncoderCallback.receiveEncoderAudioData(encodeData, VALID_LENGTH);
                                    }
                                    //我们可以把Fdk-aac编码后的数据保存到文件中，然后用播放器听一下，音频文件是否编码正确
                                    if (SAVE_FILE_FOR_TEST) {
                                        audioFileManager.saveFileData(encodeData);
                                    }
                                }
                                haveCopyLength = 0;
                            }
                        }
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                        break;
                    }
                }

            }
        };
        audioEncoderLoop = true;
        audioEncoderThread.start();
    }

    private void saveFileForTest() {
        if (SAVE_FILE_FOR_TEST) {
            videoFileManager.closeFile();
            audioFileManager.closeFile();
        }
    }
}
