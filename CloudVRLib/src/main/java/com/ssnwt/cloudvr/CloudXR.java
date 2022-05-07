package com.ssnwt.cloudvr;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;
import android.util.Log;
import android.view.Surface;

public class CloudXR {
    private static final String TAG = "SVR_CloudXR";
    private static final String EXTRA_IP = "ip";
    private static final String EXTRA_FOV_X = "fov_x";
    private static final String EXTRA_FOV_Y = "fov_y";
    private static final String EXTRA_FPS = "fps";
    private static final String EXTRA_IPD = "ipd";
    private static final String EXTRA_PRED_OFFSET = "pred_offset";
    private static final String CMD_LINE = "-s %s";
    private static final int DEFAULT_FOV_X = 105;
    private static final int DEFAULT_FOV_Y = 105;
    private static final int DEFAULT_FPS = 72;
    private static final float DEFAULT_IPD = 0.064f;
    private static final float DEFAULT_PRED_OFFSET = 0.02f;
    private static final int UNSET = -1;

    public static class HMDInfo {
        public String cmd = null;
        public int fovX = DEFAULT_FOV_X;
        public int fovY = DEFAULT_FOV_Y;
        public int fps = DEFAULT_FPS;
        public float ipd = DEFAULT_IPD;
        public float predOffset = DEFAULT_PRED_OFFSET;
    }

    public static HMDInfo getHmdInfo(Intent intent) {
        HMDInfo info = new HMDInfo();
        if (intent == null) return info;
        String ip = intent.getStringExtra(EXTRA_IP);
        if (TextUtils.isEmpty(ip)) {
            throw new NullPointerException("ip is null");
        }
        info.cmd = String.format(CMD_LINE, ip);
        info.fovX = intent.getIntExtra(EXTRA_FOV_X, DEFAULT_FOV_X);
        info.fovY = intent.getIntExtra(EXTRA_FOV_Y, DEFAULT_FOV_Y);
        info.fps = intent.getIntExtra(EXTRA_FPS, DEFAULT_FPS);
        info.ipd = intent.getFloatExtra(EXTRA_IPD, DEFAULT_IPD);
        info.predOffset = intent.getFloatExtra(EXTRA_PRED_OFFSET, DEFAULT_PRED_OFFSET);
        return info;
    }

    public static void startCloudXR(Context context, String ip) {
        startCloudXR(context, ip, UNSET, UNSET, UNSET, UNSET, UNSET);
    }

    public static void startCloudXR(Context context, String ip, int fovX, int fovY,
        int fps, float ipd, float predOffset) {
        Intent intent = new Intent(context, CloudVRActivity.class);
        intent.putExtra(EXTRA_IP, ip);
        if (fovX > 0) intent.putExtra(EXTRA_FOV_X, fovX);
        if (fovX > 0) intent.putExtra(EXTRA_FOV_Y, fovY);
        if (fovX > 0) intent.putExtra(EXTRA_FPS, fps);
        if (fovX > 0) intent.putExtra(EXTRA_IPD, ipd);
        if (fovX > 0) intent.putExtra(EXTRA_PRED_OFFSET, predOffset);
        context.startActivity(intent);
    }

    public static native void initialize(Activity activity, String cmdLine,
        int fovX, int fovY, int fps, float ipd, float predOffset);

    public static native void setSurface(Surface surface, int width, int height);

    public static native void resume();

    public static native void pause();

    public static native void release();

    static {
        Log.d(TAG, "CloudXR version code: "
            + BuildConfig.VERSION_CODE + ", version name: " + BuildConfig.VERSION_NAME);
        System.loadLibrary("cloudxrlib-jni");
    }
}
