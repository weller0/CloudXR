package com.ssnwt.cloudvr;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.util.ArrayList;
import java.util.List;

public class CloudVRActivity extends Activity implements SurfaceHolder.Callback {
    private static final String TAG = "CloudXR_CloudVRActivity";
    private static final int PERMISSIONS_REQUEST_CODE = 12345;
    private static final int RETRY_LAUNCH_COUNT_MAX = 3;
    private CloudXR.HMDInfo mHMDInfo;
    private static int sRetryLaunchCount = 0;
    private boolean isInitialized = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mHMDInfo = CloudXR.getHmdInfo(getIntent());
        if (checkPermissionsIfNeccessary()) {
            start();
        }
        SurfaceView surfaceView = new SurfaceView(this);
        setContentView(surfaceView);
        surfaceView.getHolder().addCallback(this);
    }

    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        mHMDInfo = CloudXR.getHmdInfo(getIntent());
        if (checkPermissionsIfNeccessary()) {
            start();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        CloudXR.resume();
        //SystemPropertyUtils.set("ssnwt.key.home", "0");
        //SystemPropertyUtils.set("ssnwt.ignore_home", "1");
    }

    @Override
    protected void onPause() {
        super.onPause();
        //SystemPropertyUtils.set("ssnwt.key.home", "1");
        //SystemPropertyUtils.set("ssnwt.ignore_home", "0");
        CloudXR.pause();
    }

    @Override protected void onDestroy() {
        super.onDestroy();
        stop();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
        int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSIONS_REQUEST_CODE) {
            boolean hasAllPermissions = true;
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
                    if (checkSelfPermission(p) != PackageManager.PERMISSION_GRANTED) {
                        permissionsNotGrantedYet.add(p);
                    }
                }
                Log.w(TAG, "permissionsNotGrantedYet:" + permissionsNotGrantedYet);
                if (permissionsNotGrantedYet.size() > 0) {
                    requestPermissions(permissionsNotGrantedYet.toArray(new String[0]),
                        PERMISSIONS_REQUEST_CODE);
                    return false;
                }
            }
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Not found package!", e);
        }
        return true;
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.d(TAG, "surfaceCreated");
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.d(TAG, "surfaceChanged (" + width + ", " + height + ").");
        CloudXR.setSurface(holder.getSurface(), width, height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d(TAG, "surfaceDestroyed");
    }

    private synchronized void start() {
        if (!isInitialized) {
            isInitialized = true;
            Log.d(TAG, "start CloudXR");
            if (mHMDInfo != null) {
                CloudXR.initialize(this, mHMDInfo.cmd, mHMDInfo.fovX, mHMDInfo.fovY,
                    mHMDInfo.fps, mHMDInfo.ipd, mHMDInfo.predOffset);
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
