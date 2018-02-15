package com.riemannlee.liveproject.camera.listener;

public interface CameraYUVDataListener {

    void onYUVDataReceiver(byte[] data, int width, int height);
}
