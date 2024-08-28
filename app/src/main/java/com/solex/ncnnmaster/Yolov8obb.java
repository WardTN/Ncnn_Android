package com.solex.ncnnmaster;

import android.content.res.AssetManager;
import android.graphics.Bitmap;

public class Yolov8obb {
    public native void loadObbModel(AssetManager mgr, String bin, String param);

    public native Bitmap obbDetect(Bitmap bitmap);

    public native void bitMapToMatTest(Bitmap bitmap);

    static {
        System.loadLibrary("ncnnmaster");
    }


}