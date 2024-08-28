package com.solex.ncnnmaster;


import android.content.res.AssetManager;
import android.graphics.Bitmap;

public class PaddleOCRNcnn {

    public native boolean Init(AssetManager mgr);
    public class Obj
    {
        public float x0;
        public float y0;
        public float x1;
        public float y1;
        public float x2;
        public float y2;
        public float x3;
        public float y3;
        public String label;
        public float prob;
    }

//    public native Obj[] Detect(Bitmap bitmap, boolean use_gpu);

    static {
        System.loadLibrary("ncnnmaster");
    }

}
