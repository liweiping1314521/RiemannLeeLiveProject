package com.riemannlee.liveproject;

import android.Manifest;
import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.view.View;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;

import com.riemannlee.liveproject.camera.manager.CameraSurfaceView;
import com.riemannlee.liveproject.camera.manager.VideoGatherManager;
import com.riemannlee.liveproject.camera.util.PermissionsUtils;
import com.riemannlee.liveproject.camera.util.SPUtil;

public class MainActivity extends Activity implements View.OnClickListener{

    private final int REQUEST_CODE_PERMISSIONS = 10;
    private VideoGatherManager manager;
    private CameraSurfaceView mSurfaceView;
    private ImageView mCameraSwitchImageView;
    private ImageView mRtmpPublishImageView;

    public TextView cameraInfoTextView;
    public TextView scaleWidthTextView;
    public TextView scaleHeightTextView;
    public TextView cropStartXTextView;
    public TextView cropStartYTextView;
    public TextView cropWidthTextView;
    public TextView cropHeightTextView;
    public EditText rmtpUrlEditText;

    private MediaPublisher mMediaPublisher;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        MainApplication.setCurrentActivity(this);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            final String[] PERMISSIONS = new String[]{Manifest.permission.CAMERA};
            PermissionsUtils.checkAndRequestMorePermissions(this, PERMISSIONS, REQUEST_CODE_PERMISSIONS,
                    new PermissionsUtils.PermissionRequestSuccessCallBack() {

                        @Override
                        public void onHasPermission() {
                            setContentView(R.layout.activity_main);
                            initView();
                        }
                    });
        }
        mMediaPublisher = new MediaPublisher();
        mMediaPublisher.setRtmpUrl("rtmp://192.168.1.109:1935/live1/room");
        //mMediaPublisher.setRtmpUrl("rtmp://192.168.0.6:1935/live1/room");
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (PermissionsUtils.isPermissionRequestSuccess(grantResults)) {
            setContentView(R.layout.activity_main);
            initView();
        }
    }

    private void initView() {
        mSurfaceView = (CameraSurfaceView) findViewById(R.id.camera_surface);
        mCameraSwitchImageView = (ImageView) findViewById(R.id.switch_camera_img);
        mRtmpPublishImageView = (ImageView) findViewById(R.id.rtmp_publish_img);

        rmtpUrlEditText = (EditText)findViewById(R.id.rtmp_url_edit_text);
        //图片的相关信息
        cameraInfoTextView = (TextView) findViewById(R.id.tv_camera_info);
        scaleHeightTextView = (TextView) findViewById(R.id.tv_scale_height);
        scaleWidthTextView = (TextView) findViewById(R.id.et_scale_width);
        cropStartXTextView = (TextView) findViewById(R.id.et_crop_start_x);
        cropStartYTextView = (TextView) findViewById(R.id.et_crop_start_y);
        cropWidthTextView = (TextView) findViewById(R.id.et_crop_width);
        cropHeightTextView = (TextView) findViewById(R.id.et_crop_height);

        mCameraSwitchImageView.setOnClickListener(this);
        mRtmpPublishImageView.setOnClickListener(this);

        manager = new VideoGatherManager(mSurfaceView);
    }

    private void initCameraInfo() {
        //摄像头预览设置
        int width = (int) SPUtil.get(Contacts.CAMERA_WIDTH, 0);
        int height = (int) SPUtil.get(Contacts.CAMERA_HEIGHT, 0);
        int morientation = (int) SPUtil.get(Contacts.CAMERA_Morientation, 0);
        cameraInfoTextView.setText(String.format("摄像头预览大小:%d*%d\n旋转的角度:%d度", width, height, morientation));

        //缩放大小设置
        scaleWidthTextView.setText(SPUtil.get(Contacts.SCALE_WIDTH, 480).toString());
        scaleHeightTextView.setText(SPUtil.get(Contacts.SCALE_HEIGHT, 640).toString());//854

        //裁剪的设置
        cropStartXTextView.setText(SPUtil.get(Contacts.CROP_START_X, 0).toString());
        cropStartYTextView.setText(SPUtil.get(Contacts.CROP_START_Y, 0).toString());//214
        cropWidthTextView.setText(SPUtil.get(Contacts.CROP_WIDTH, 480).toString());
        cropHeightTextView.setText(SPUtil.get(Contacts.CROP_HEIGHT, 640).toString());
    }

    @Override
    public void onClick(View v) {
        if (v == mCameraSwitchImageView) {
            //切换摄像头
            manager.changeCamera();
            initCameraInfo();
        } else if (v == mRtmpPublishImageView) {
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        manager.onResume();
        initCameraInfo();
        mMediaPublisher.initVideoGather(manager);
        mMediaPublisher.initAudioGather();
        mMediaPublisher.startGather();
        mMediaPublisher.startMediaEncoder();
    }

    @Override
    protected void onStop() {
        super.onStop();
        //manager.onStop();
    }

    @Override
    protected void onPause() {
        super.onPause();
        manager.onStop();
        mMediaPublisher.stopGather();
        mMediaPublisher.stopMediaEncoder();
        mMediaPublisher.relaseRtmp();
    }
}
