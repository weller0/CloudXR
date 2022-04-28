package com.ssnwt.cloudvr;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.util.ArrayList;
import java.util.List;

public class CloudVRActivity extends Activity implements SurfaceHolder.Callback {
    private static final String TAG = "CloudXR_CloudVRActivity";
    private static final int PERMISSIONS_REQUEST_CODE = 12345;
    private static final int RETRY_LAUNCH_COUNT_MAX = 3;
    private static final String CMD_LINE = "-s %s";
    private String mCmdLine = null;
    private static int sRetryLaunchCount = 0;
    private Surface mSurface;
    private boolean hasAllPermissions = false;
    private boolean isInitialized = false;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        String ip = getIntent().getStringExtra("ip");
        if (!TextUtils.isEmpty(ip)) {
            mCmdLine = getCmdLine(ip);
        }
        if (hasAllPermissions = checkPermissionsIfNeccessary()) {
            if (TextUtils.isEmpty(mCmdLine)) {
                mCmdLine = getCmdLine("192.168.1.106");
            }
            SurfaceView surfaceView = new SurfaceView(this);
            setContentView(surfaceView);
            surfaceView.getHolder().addCallback(this);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        SystemPropertyUtils.set("ssnwt.key.home", "0");
        //SystemPropertyUtils.set("ssnwt.ignore_home", "1");
    }

    @Override
    protected void onPause() {
        super.onPause();
        SystemPropertyUtils.set("ssnwt.key.home", "1");
        //SystemPropertyUtils.set("ssnwt.ignore_home", "0");
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
        int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSIONS_REQUEST_CODE) {
            // If request is cancelled, the result arrays are empty.
            for (int i = 0; i < grantResults.length; i++) {
                if (grantResults[i] != PackageManager.PERMISSION_GRANTED) {
                    hasAllPermissions = false;
                    Log.w(TAG, "Permission:" + permissions[i] + " is DENIED!!");
                }
            }

            if (!hasAllPermissions) {
                finish();
                if (sRetryLaunchCount < RETRY_LAUNCH_COUNT_MAX) {
                    sRetryLaunchCount++;
                    Log.w(TAG,
                        "Retry launch CloudXR " + sRetryLaunchCount + "/" + RETRY_LAUNCH_COUNT_MAX);
                    startActivity(new Intent(this, CloudVRActivity.class));
                } else {
                    Log.e(TAG, "Request permission failed, finish current activity");
                }
            } else {
                start();
            }
        }
    }

    private boolean checkPermissionsIfNeccessary() {
        try {
            PackageInfo info = getPackageManager()
                .getPackageInfo(this.getPackageName(), PackageManager.GET_PERMISSIONS);
            if (info.requestedPermissions != null) {
                List<String> permissionsNotGrantedYet =
                    new ArrayList<>(info.requestedPermissions.length);
                for (String p : info.requestedPermissions) {
                    if (ContextCompat.checkSelfPermission(this, p)
                        != PackageManager.PERMISSION_GRANTED) {
                        permissionsNotGrantedYet.add(p);
                    }
                }
                Log.w(TAG, "permissionsNotGrantedYet:" + permissionsNotGrantedYet);
                if (permissionsNotGrantedYet.size() > 0) {
                    ActivityCompat.requestPermissions(this, permissionsNotGrantedYet.toArray(
                        new String[0]),
                        PERMISSIONS_REQUEST_CODE);
                    return false;
                }
            }
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Not found package!", e);
        }
        return true;
    }

    private String getCmdLine(String ip) {
        return String.format(CMD_LINE, ip);
    }

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder holder) {
        Log.d(TAG, "surfaceCreated");
        mSurface = holder.getSurface();
        start();
    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
        Log.d(TAG, "surfaceChanged (" + width + ", " + height + ").");
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
        Log.d(TAG, "surfaceDestroyed");
        stop();
    }

    private synchronized void start() {
        if (!isInitialized) {
            isInitialized = true;
            Log.d(TAG, "start CloudXR");
            if (mSurface != null && hasAllPermissions && !TextUtils.isEmpty(mCmdLine)) {
                CloudXR.initialize(this, mSurface, mCmdLine);
            }
        }
    }

    private synchronized void stop() {
        if (isInitialized) {
            isInitialized = false;
            CloudXR.release();
            Log.d(TAG, "stop CloudXR");
        }
    }
}
