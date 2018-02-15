package com.riemannlee.liveproject.audio;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Environment;

import com.riemannlee.liveproject.FileManager;

import java.util.Arrays;

public class AudioRecoderManager {

    private static final String TAG = "AudioRecoderManager";

    // 音频获取
    private final static int SOURCE = MediaRecorder.AudioSource.MIC;

    // 设置音频采样率，44100是目前的标准，但是某些设备仍然支 2050 6000 1025
    private final static int SAMPLE_HZ = 44100;

    // 设置音频的录制的声道CHANNEL_IN_STEREO为双声道，CHANNEL_CONFIGURATION_MONO为单声道
    private final static int CHANNEL_CONFIG = AudioFormat.CHANNEL_IN_STEREO;

    // 音频数据格式:PCM 16位每个样本保证设备支持。PCM 8位每个样本 不一定能得到设备支持
    private final static int FORMAT = AudioFormat.ENCODING_PCM_16BIT;

    private int mBufferSize;

    private AudioRecord mAudioRecord = null;
    private int bufferSizeInBytes = 0;

    private FileManager fileManager;
    private static final boolean SAVE_FILE_FOR_TEST = true;

    private Thread workThread;
    private boolean loop;

    public interface AudioDataListener {
        void audioData(byte[] data);
    }

    private AudioDataListener audioDataListener;

    public void setAudioDataListener(AudioDataListener audioDataListener) {
        this.audioDataListener = audioDataListener;
    }

    public AudioRecoderManager() {
        if (SAVE_FILE_FOR_TEST) {
            fileManager = new FileManager(FileManager.TEST_PCM_FILE);
        }

        bufferSizeInBytes = AudioRecord.getMinBufferSize(SAMPLE_HZ, CHANNEL_CONFIG, FORMAT);
        mAudioRecord = new AudioRecord(SOURCE, SAMPLE_HZ, CHANNEL_CONFIG, FORMAT, bufferSizeInBytes);
        mBufferSize = 4 * 1024;
    }

    public void startAudioIn() {

        workThread = new Thread() {
            @Override
            public void run() {
                mAudioRecord.startRecording();
                byte[] audioData = new byte[mBufferSize];
                int readsize = 0;
                //录音，获取PCM裸音频，这个音频数据文件很大，我们必须编码成AAC，这样才能rtmp传输
                while (loop && !Thread.interrupted()) {
                    try {
                        readsize += mAudioRecord.read(audioData, readsize, mBufferSize);
                        byte[] ralAudio = new byte[readsize];
                        //每次录音读取4K数据
                        System.arraycopy(audioData, 0, ralAudio, 0, readsize);
                        if (audioDataListener != null) {
                            //把录音的数据抛给MediaEncoder去编码AAC音频数据
                            audioDataListener.audioData(ralAudio);
                        }
                        //我们可以把裸音频以文件格式存起来，判断这个音频是否是好的，只需要加一个WAV头
                        //即形成WAV无损音频格式
                        if (SAVE_FILE_FOR_TEST) {
                            fileManager.saveFileData(ralAudio);
                        }

                        readsize = 0;
                        Arrays.fill(audioData, (byte)0);
                    }
                    catch(Exception e) {
                        e.printStackTrace();
                    }
                }
            }
        };

        loop = true;
        workThread.start();
    }

    public void stopAudioIn() {
        loop = false;
        workThread.interrupt();
        mAudioRecord.stop();
        mAudioRecord.release();
        mAudioRecord = null;

        if (SAVE_FILE_FOR_TEST) {
            fileManager.closeFile();
            //测试代码，以WAV格式保存数据啊
            PcmToWav.copyWaveFile(FileManager.TEST_PCM_FILE, FileManager.TEST_WAV_FILE, SAMPLE_HZ, bufferSizeInBytes);
        }
    }
}
