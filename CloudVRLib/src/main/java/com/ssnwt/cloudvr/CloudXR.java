package com.ssnwt.cloudvr;

import android.app.Activity;
import android.util.Log;
import android.view.Surface;

public class CloudXR {
    private static final String TAG = "CloudXR_JNI";

    public static native void initialize(Activity activity, String cmdLine);

    public static native void setSurface(Surface surface, int width, int height);

    public static native void resume();

    public static native void pause();

    public static native void release();

    static {
        Log.w(TAG, "System.loadLibrary(\"cloudxrlib-jni\")");
        System.loadLibrary("cloudxrlib-jni");
    }
}
