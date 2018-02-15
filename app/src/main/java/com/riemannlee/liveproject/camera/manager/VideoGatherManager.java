package com.riemannlee.liveproject.camera.manager;

import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.widget.Toast;

import com.riemannlee.liveproject.FileManager;
import com.riemannlee.liveproject.MainApplication;
import com.riemannlee.liveproject.StreamProcessManager;
import com.riemannlee.liveproject.Contacts;
import com.riemannlee.liveproject.camera.listener.CameraNVDataListener;
import com.riemannlee.liveproject.camera.listener.CameraYUVDataListener;
import com.riemannlee.liveproject.camera.util.CameraUtil;
import com.riemannlee.liveproject.camera.util.SPUtil;

import java.util.concurrent.LinkedBlockingQueue;

import static android.content.Context.SENSOR_SERVICE;

@SuppressWarnings("deprecation")
public class VideoGatherManager implements SensorEventListener, CameraNVDataListener {

    private CameraSurfaceView mCameraSurfaceView;
    private CameraUtil mCameraUtil;

    private int scaleWidth;
    private int scaleHeight;
    private int cropStartX;
    private int cropStartY;
    private int cropWidth;
    private int cropHeight;

    //传感器需要，这边使用的是重力传感器
    private SensorManager mSensorManager;
    //第一次实例化的时候是不需要的
    private boolean mInitialized = false;
    private float mLastX = 0f;
    private float mLastY = 0f;
    private float mLastZ = 0f;

    private CameraYUVDataListener yuvDataListener;
    //阻塞线程安全队列，生产者和消费者
    private LinkedBlockingQueue<byte[]> mQueue = new LinkedBlockingQueue<>();
    private Thread workThread;
    private boolean loop;

    private FileManager fileManager;
    //设置为true，可以把生成的NV21转化为YUV的数据保存起来，有专门的YUV播放器可以用来播放
    private static final boolean SAVE_FILE_FOR_TEST = false;

    public VideoGatherManager(CameraSurfaceView cameraSurfaceView) {
        mCameraSurfaceView = cameraSurfaceView;
        mCameraUtil = cameraSurfaceView.getCameraUtil();
        mCameraSurfaceView.setCameraYUVDataListener(this);

        mSensorManager = (SensorManager) MainApplication.getInstance().getSystemService(SENSOR_SERVICE);
        isSupport();
        StreamProcessManager.init(mCameraUtil.getCameraWidth(), mCameraUtil.getCameraHeight(), scaleWidth, scaleHeight);
        StreamProcessManager.encoderVideoinit(scaleWidth, scaleHeight, scaleWidth, scaleHeight);

        initWorkThread();
        loop = true;
        workThread.start();

        if (SAVE_FILE_FOR_TEST) {
            fileManager = new FileManager(FileManager.TEST_YUV_FILE);
        }
    }

    public void setYuvDataListener(CameraYUVDataListener listener) {
        this.yuvDataListener = listener;
    }

    public int changeCamera() {
        return mCameraSurfaceView.changeCamera();
    }

    public void onResume() {
        //打开摄像头
        mCameraSurfaceView.openCamera();
        //注册加速度传感器
        mSensorManager.registerListener(this, mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER), SensorManager.SENSOR_DELAY_UI);
    }

    public void onStop() {
        //释放摄像头
        mCameraSurfaceView.releaseCamera();
        mSensorManager.unregisterListener(this);
        loop = false;
        StreamProcessManager.release();

        if (SAVE_FILE_FOR_TEST) {
            fileManager.closeFile();
        }
    }

    private void initWorkThread() {
        workThread = new Thread() {
            @Override
            public void run() {
                while (loop && !Thread.interrupted()) {
                    try {
                        //获取阻塞队列中的数据，没有数据的时候阻塞
                        byte[] srcData = mQueue.take();
                        //生成I420(YUV标准格式数据及YUV420P)目标数据，生成后的数据长度width * height * 3 / 2
                        final byte[] dstData = new byte[scaleWidth * scaleHeight * 3 / 2];
                        final int morientation = mCameraUtil.getMorientation();
                        //压缩NV21(YUV420SP)数据，元素数据位1080 * 1920，很显然这样的数据推流会很占用带宽，我们压缩成480 * 640 的YUV数据
                        //为啥要转化为YUV420P数据？因为是在为转化为H264数据在做准备，NV21不是标准的，只能先通过转换，生成标准YUV420P数据，
                        //然后把标准数据encode为H264流
                        StreamProcessManager.compressYUV(srcData, mCameraUtil.getCameraWidth(), mCameraUtil.getCameraHeight(), dstData, scaleHeight, scaleWidth, 0, morientation, morientation == 270);

                        //进行YUV420P数据裁剪的操作，测试下这个借口，我们可以对数据进行裁剪，裁剪后的数据也是I420数据，我们采用的是libyuv库文件
                        //这个libyuv库效率非常高，这也是我们用它的原因
                        final byte[] cropData = new byte[cropWidth * cropHeight * 3 / 2];
                        StreamProcessManager.cropYUV(dstData, scaleWidth, scaleHeight, cropData, cropWidth, cropHeight, cropStartX, cropStartY);

                        //自此，我们得到了YUV420P标准数据，这个过程实际上就是NV21转化为YUV420P数据
                        //注意，有些机器是NV12格式，只是数据存储不一样，我们一样可以用libyuv库的接口转化
                        if (yuvDataListener != null) {
                            yuvDataListener.onYUVDataReceiver(cropData, cropWidth, cropHeight);
                        }

                        //设置为true，我们把生成的YUV文件用播放器播放一下，看我们的数据是否有误，起调试作用
                        if (SAVE_FILE_FOR_TEST) {
                            fileManager.saveFileData(cropData);
                        }
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                        break;
                    }
                }
            }
        };
    }

    @Override
    public void onCallback(final byte[] srcData) {
        if (srcData != null) {
            try {
                mQueue.put(srcData);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        //if (isGatherStream) {
//            new Thread(new Runnable() {
//                @Override
//                public void run() {
//                    //进行yuv数据的缩放，旋转镜像缩放等操作
//                    //Log.e("RiemannLee", " scaleWidth " + scaleWidth + " scaleHeight " + scaleHeight);
//                    final byte[] dstData = new byte[scaleWidth * scaleHeight * 3 / 2];
//                    final int morientation = mCameraUtil.getMorientation();
//                    StreamProcessManager.compressYUV(srcData, mCameraUtil.getCameraWidth(), mCameraUtil.getCameraHeight(), dstData, scaleHeight, scaleWidth, 0, morientation, morientation == 270);
//
//                    //进行yuv数据裁剪的操作
//                    //Log.e("RiemannLee", " cropWidth " + cropWidth + " cropWidth " + cropHeight);
//                    final byte[] cropData = new byte[cropWidth * cropHeight * 3 / 2];
//                    StreamProcessManager.cropYUV(dstData, scaleWidth, scaleHeight, cropData, cropWidth, cropHeight, cropStartX, cropStartY);
//
//                    yuvDataListener.onYUVDataReceiver(cropData, cropWidth, cropHeight);

//                    try {
//                        fileOutputStream.write(cropData);
//                        fileOutputStream.flush();
//                    } catch (Exception e) {
//                        e.printStackTrace();
//                    }

                    //这里将yuvi420转化为nv21，因为yuvimage只能操作nv21和yv12，为了演示方便，这里做一步转化的操作
//                    final byte[] nv21Data = new byte[cropWidth * cropHeight * 3 / 2];
//                    StreamProcessManager.yuvI420ToNV21(cropData, nv21Data, cropWidth, cropHeight);
//
//                    //这里采用yuvImage将yuvi420转化为图片，当然用libyuv也是可以做到的，这里主要介绍libyuv的裁剪，旋转，缩放，镜像的操作
//                    YuvImage yuvImage = new YuvImage(nv21Data, ImageFormat.NV21, cropWidth, cropHeight, null);
//                    ByteArrayOutputStream fOut = new ByteArrayOutputStream();
//                    yuvImage.compressToJpeg(new Rect(0, 0, cropWidth, cropHeight), 100, fOut);
//
//                    //将byte生成bitmap
//                    byte[] bitData = fOut.toByteArray();
//                    final Bitmap bitmap = BitmapFactory.decodeByteArray(bitData, 0, bitData.length);
//                    saveBitmapFile(fOut);
//                    new Handler(Looper.getMainLooper()).post(new Runnable() {
//                        @Override
//                        public void run() {
//                            if (listener != null) {
//                                listener.onPictureBitmap(bitmap);
//                            }
//                            //isRunning = false;
//                        }
//                    });
//                }
//            }).start();
//        }
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        float x = event.values[0];
        float y = event.values[1];
        float z = event.values[2];

        if (!mInitialized) {
            mLastX = x;
            mLastY = y;
            mLastZ = z;
            mInitialized = true;
        }

        float deltaX = Math.abs(mLastX - x);
        float deltaY = Math.abs(mLastY - y);
        float deltaZ = Math.abs(mLastZ - z);

        if (mCameraSurfaceView != null && (deltaX > 0.6 || deltaY > 0.6 || deltaZ > 0.6)) {
            mCameraSurfaceView.startAutoFocus(-1, -1);
        }

        mLastX = x;
        mLastY = y;
        mLastZ = z;
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {

    }

    //主要是对裁剪的判断
    public boolean isSupport() {
        scaleWidth = (int) SPUtil.get(Contacts.SCALE_WIDTH, 480);
        scaleHeight = (int) SPUtil.get(Contacts.SCALE_HEIGHT, 640);
        cropWidth = (int) SPUtil.get(Contacts.CROP_WIDTH, 480);
        cropHeight = (int) SPUtil.get(Contacts.CROP_HEIGHT, 640);
        cropStartX = (int) SPUtil.get(Contacts.CROP_START_X, 0);
        cropStartY = (int) SPUtil.get(Contacts.CROP_START_Y, 0);
        if (cropStartX % 2 != 0 || cropStartY % 2 != 0) {
            Toast.makeText(MainApplication.getInstance(), "裁剪的开始位置必须为偶数", Toast.LENGTH_SHORT).show();
            return false;
        }
        if (cropStartX + cropWidth > scaleWidth || cropStartY + cropHeight > scaleHeight) {
            Toast.makeText(MainApplication.getInstance(), "裁剪区域超出范围", Toast.LENGTH_SHORT).show();
            return false;
        }
        return true;
    }
}