package com.solex.ncnnmaster;

import android.content.res.AssetManager;

public class Yolov8Ncnn extends BaseNcnn {
    public native boolean loadModel(AssetManager mgr, int modelid, int cpugpu);

    @Override
    boolean loadCurModel(AssetManager mgr, int modelid, int cpugpu) {
        return loadModel(mgr, modelid, cpugpu);
    }
}
