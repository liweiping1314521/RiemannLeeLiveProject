package com.riemannlee.liveproject;

import java.io.File;
import java.io.FileOutputStream;

public class FileManager {

    public static final String TEST_PCM_FILE = "/sdcard/123.pcm";
    public static final String TEST_WAV_FILE = "/sdcard/123.wav";
    public static final String TEST_YUV_FILE = "/sdcard/123.yuv";
    public static final String TEST_H264_FILE = "/sdcard/123.h264";
    public static final String TEST_AAC_FILE = "/sdcard/123.aac";
    private String fileName;
    private FileOutputStream fileOutputStream;
    private boolean testForWrite = true;

    public FileManager(String fileName) {
        this.fileName = fileName;
        try {
            File file = new File(fileName);
            if (file.exists()) {
                file.delete();
            } else {
                file.createNewFile();
            }
            fileOutputStream = new FileOutputStream(file);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public void saveFileData(byte[] data, int offset, int length) {
        try {
            fileOutputStream.write(data, offset, length);
            fileOutputStream.flush();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public void saveFileData(byte[] data) {
        try {
            fileOutputStream.write(data);
            fileOutputStream.flush();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public void closeFile() {
        try {
            fileOutputStream.close();
        } catch (Exception e) {
        } finally {
            try {
                fileOutputStream.close();
            } catch (Exception e1) {
            }
        }
    }
}
