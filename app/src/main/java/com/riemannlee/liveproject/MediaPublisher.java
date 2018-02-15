package com.riemannlee.liveproject;

import android.util.Log;

import com.riemannlee.liveproject.audio.AudioData;
import com.riemannlee.liveproject.audio.AudioRecoderManager;
import com.riemannlee.liveproject.camera.listener.CameraYUVDataListener;
import com.riemannlee.liveproject.camera.manager.VideoData;
import com.riemannlee.liveproject.camera.manager.VideoGatherManager;

import java.util.concurrent.LinkedBlockingQueue;

public class MediaPublisher {
    private static final String TAG = "MediaPublisher";

    public boolean isPublish = true;

    public static final int NAL_UNKNOWN     = 0;
    public static final int NAL_SLICE       = 1; /* 非关键帧 */
    public static final int NAL_SLICE_DPA   = 2;
    public static final int NAL_SLICE_DPB   = 3;
    public static final int NAL_SLICE_DPC   = 4;
    public static final int NAL_SLICE_IDR   = 5; /* 关键帧 */
    public static final int NAL_SEI         = 6;
    public static final int NAL_SPS         = 7; /* SPS帧 */
    public static final int NAL_PPS         = 8; /* PPS帧 */
    public static final int NAL_AUD         = 9;
    public static final int NAL_FILLER      = 12;

    private VideoGatherManager videoGatherManager;
    private AudioRecoderManager audioGathererManager;
    private MediaEncoder mediaEncoder;

    private LinkedBlockingQueue<Runnable> mRunnables = new LinkedBlockingQueue<>();
    private Thread rtmpThread;
    private boolean loop;

    private long videoID = 0;
    private long audioID = 0;
    private boolean isSendAudioSpec = false;

    public MediaPublisher() {
        mediaEncoder = new MediaEncoder();

        MediaEncoder.setsMediaEncoderCallback(new MediaEncoder.MediaEncoderCallback() {
            @Override
            public void receiveEncoderVideoData(byte[] videoData, int totalLength, int[] segment) {
                onEncoderVideoData(videoData, totalLength, segment);
            }

            @Override
            public void receiveEncoderAudioData(byte[] audioData, int size) {
                onEncoderAudioData(audioData, size);
            }
        });

        rtmpThread = new Thread("publish-thread") {
            @Override
            public void run() {
                while (loop && !Thread.interrupted()) {
                    try {
                        Runnable runnable = mRunnables.take();
                        runnable.run();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            }
        };

        loop = true;
        rtmpThread.start();
    }

    public void initVideoGather(VideoGatherManager manager) {
        videoGatherManager = manager;
        videoGatherManager.setYuvDataListener(new CameraYUVDataListener() {
            @Override
            public void onYUVDataReceiver(byte[] data, int width, int height) {
                if (isPublish) {
                    VideoData videoData = new VideoData(data, width, height);
                    mediaEncoder.putVideoData(videoData);
                }
            }
        });
    }

    public void initAudioGather() {
        audioGathererManager = new AudioRecoderManager();
        audioGathererManager.setAudioDataListener(new AudioRecoderManager.AudioDataListener() {
            @Override
            public void audioData(byte[] data) {
                if (isPublish) {
                    AudioData audioData = new AudioData(data);
                    mediaEncoder.putAudioData(audioData);
                }
            }
        });
    }

    public void startMediaEncoder() {
        mediaEncoder.start();
    }

    public void stopMediaEncoder() {
        mediaEncoder.stop();
    }

    public void startGather() {
        audioGathererManager.startAudioIn();
    }

    public void stopGather() {
        audioGathererManager.stopAudioIn();
    }

    public void setRtmpUrl(final String url) {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                StreamProcessManager.initRtmpData(url);
            }
        };
        try {
            mRunnables.put(runnable);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public void relaseRtmp() {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                StreamProcessManager.releaseRtmp();
            }
        };
        try {
            mRunnables.put(runnable);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void onEncoderVideoData(byte[] encodeVideoData, int totalLength, int[] segment) {
        int spsLen = 0;
        int ppsLen = 0;
        byte[] sps = null;
        byte[] pps = null;
        int haveCopy = 0;
        //segment为C++传递上来的数组，当为SPS，PPS的时候，视频NALU数组大于1，其它时候等于1
        for (int i = 0; i < segment.length; i++) {
            int segmentLength = segment[i];
            byte[] segmentByte = new byte[segmentLength];
            System.arraycopy(encodeVideoData, haveCopy, segmentByte, 0, segmentLength);
            haveCopy += segmentLength;

            int offset = 4;
            if (segmentByte[2] == 0x01) {
                offset = 3;
            }
            int type = segmentByte[offset] & 0x1f;
            //Log.d("RiemannLee", "type= " + type);
            //获取到NALU的type，SPS，PPS，SEI，还是关键帧
            if (type == NAL_SPS) {
                spsLen = segment[i] - 4;
                sps = new byte[spsLen];
                System.arraycopy(segmentByte, 4, sps, 0, spsLen);
                //Log.e("RiemannLee", "NAL_SPS spsLen " + spsLen);
            } else if (type == NAL_PPS) {
                ppsLen = segment[i] - 4;
                pps = new byte[ppsLen];
                System.arraycopy(segmentByte, 4, pps, 0, ppsLen);
                //Log.e("RiemannLee", "NAL_PPS ppsLen " + ppsLen);
                sendVideoSpsAndPPS(sps, spsLen, pps, ppsLen, 0);
            } else {
                //如果是关键帧，则在发送该帧之前先发送SPS和PPS
                sendVideoData(segmentByte, segmentLength, videoID++);
            }
        }
    }

    private void sendVideoSpsAndPPS(final byte[] sps, final int spsLen, final byte[] pps, final int ppsLen, final long timeStamp) {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                StreamProcessManager.sendRtmpVideoSpsPPS(sps, spsLen, pps, ppsLen, timeStamp);
            }
        };
        try {
            mRunnables.put(runnable);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void sendVideoData(final byte[] data, final int dataLen, final long timeStamp) {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                StreamProcessManager.sendRtmpVideoData(data, dataLen, timeStamp);
            }
        };
        try {
            mRunnables.put(runnable);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void sendAudioSpec(final long timeStamp) {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                StreamProcessManager.sendRtmpAudioSpec(timeStamp);
            }
        };
        try {
            mRunnables.put(runnable);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void sendAudioData(final byte[] data, final int dataLen, final long timeStamp) {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                StreamProcessManager.sendRtmpAudioData(data, dataLen, timeStamp);
            }
        };
        try {
            mRunnables.put(runnable);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void onEncoderAudioData(byte[] encodeAudioData, int size) {
        if (!isSendAudioSpec) {
            Log.e("RiemannLee", "#######sendAudioSpec######");
            sendAudioSpec(0);
            isSendAudioSpec = true;
        }
        sendAudioData(encodeAudioData, size, audioID++);
    }
}