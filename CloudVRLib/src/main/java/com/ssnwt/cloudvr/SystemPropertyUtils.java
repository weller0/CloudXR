package com.ssnwt.cloudvr;

import android.annotation.SuppressLint;
import android.util.Log;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Created by kevin.xie on 2018/5/12 0012.
 */
@SuppressLint("PrivateApi")
public class SystemPropertyUtils {
    private static final String TAG = SystemPropertyUtils.class.getSimpleName();
    private static Class<?> mSystemPropertyClass;
    private static Method mSetStringMethod;
    private static Method mGetStringMethod;

    static {
        try {
            mSystemPropertyClass = Class.forName("android.os.SystemProperties");
            mSetStringMethod = mSystemPropertyClass.getMethod("set", String.class, String.class);
            mGetStringMethod = mSystemPropertyClass.getMethod("get", String.class, String.class);
        } catch (ClassNotFoundException | NoSuchMethodException e) {
            Log.w(TAG, "SystemProperties No such class", e);
        }
    }

    public static void set(String key, String value) {
        try {
            mSetStringMethod.invoke(mSystemPropertyClass, key, value);
        } catch (IllegalAccessException | InvocationTargetException e) {
            Log.w(TAG, "SystemProperties set", e);
        }
    }

    public static String get(String key, String defaultValue) {
        String value = defaultValue;
        try {
            value = (String) mGetStringMethod.invoke(mSystemPropertyClass, key, defaultValue);
        } catch (IllegalAccessException | InvocationTargetException e) {
            Log.w(TAG, "SystemProperties get", e);
        }
        return value;
    }
}
