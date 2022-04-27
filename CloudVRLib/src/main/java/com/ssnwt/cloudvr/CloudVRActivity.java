package com.ssnwt.cloudvr;

import android.app.Activity;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
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

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (checkPermissionsIfNeccessary()) {
            SurfaceView surfaceView = new SurfaceView(this);
            setContentView(surfaceView);
            surfaceView.getHolder().addCallback(this);
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
                                           int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSIONS_REQUEST_CODE) {
            boolean hasAllPermissions = true;
            // If request is cancelled, the result arrays are empty.
            if (grantResults.length == 0) {
                hasAllPermissions = false;
            }
            for (int result : grantResults) {
                if (result != PackageManager.PERMISSION_GRANTED) {
                    hasAllPermissions = false;
                    break;
                }
            }

            if (!hasAllPermissions) {
                Log.e(TAG, "request permission failed");
                finish();
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
                if (permissionsNotGrantedYet.size() > 0) {
                    ActivityCompat.requestPermissions(this, permissionsNotGrantedYet.toArray(
                            new String[permissionsNotGrantedYet.size()]),
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
    public void surfaceCreated(@NonNull SurfaceHolder holder) {
        initialize(this, holder.getSurface(), "-s 192.168.1.106");
    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
        release();
    }

    private native void initialize(Activity activity, Surface surface, String cmdLine);

    private native void release();

    static {
        System.loadLibrary("cloudxrlib-jni");
    }
}
