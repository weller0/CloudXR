package com.ssnwt.cloudvr;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.util.Log;
import android.view.Surface;
import com.ssnwt.cloudvr.BuildConfig;

public class CloudXR {
    private static final String TAG = "CloudXR_JNI";

    public static void startCloudXR(Context context, String ip) {
        Intent intent = new Intent(context, CloudVRActivity.class);
        intent.putExtra("ip", ip);
        context.startActivity(intent);
    }

    public static native void initialize(Activity activity, String cmdLine);

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
