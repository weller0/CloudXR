package com.ssnwt.cloudxr;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import com.ssnwt.cloudvr.CloudXR;

public class MainActivity extends AppCompatActivity {
    private static final String DEFAULT_IP = "192.168.50.108";
    //private static final String DEFAULT_IP = "192.168.1.106";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        CloudXR.startCloudXR(this, DEFAULT_IP);
    }
}