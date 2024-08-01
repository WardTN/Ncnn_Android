package com.solex.ncnnmaster;

import android.content.res.AssetManager;

public class Yolov8obb {
    public native void loadObbModel(AssetManager mgr, String bin , String param);
    public native void obbDetect(String imgPath);
    static {
        System.loadLibrary("ncnnmaster");
    }


}