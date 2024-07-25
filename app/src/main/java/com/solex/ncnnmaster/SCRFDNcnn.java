package com.solex.ncnnmaster;

import android.content.res.AssetManager;

public class SCRFDNcnn extends BaseNcnn {
    public native boolean loadModel(AssetManager mgr, int modelid, int cpugpu);

    @Override
    public boolean loadCurModel(AssetManager mgr, int modelid, int cpugpu) {
        return loadModel(mgr, modelid, cpugpu);
    }
}
