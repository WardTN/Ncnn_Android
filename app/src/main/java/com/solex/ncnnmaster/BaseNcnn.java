package com.solex.ncnnmaster;

import android.content.res.AssetManager;
import android.view.Surface;

public abstract class BaseNcnn {

    public native boolean openCamera(int facing);
    public native boolean closeCamera();
    public native boolean setOutputWindow(Surface surface);
    static {
        System.loadLibrary("ncnnmaster");
    }

    abstract boolean loadCurModel(AssetManager mgr, int modelid, int cpugpu);



}
