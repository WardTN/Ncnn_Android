// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.solex.ncnnmaster;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.PixelFormat;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;

import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.util.ArrayList;
import java.util.Arrays;

public class MainActivity extends Activity implements SurfaceHolder.Callback {
    public static final int REQUEST_CAMERA = 100;
    public static final int REQUEST_WRITE = 200;

    //    private SCRFDNcnn scrfdncnn = new SCRFDNcnn();
    private BaseNcnn ncnn = new Yolov8Ncnn();
    private int facing = 0;

    private Spinner spinnerModel;
    private Spinner spinnerModelWeight;
    private Spinner spinnerCPUGPU;

    private int current_model = 0;
    private int current_model_weight = 0;
    private int current_cpugpu = 0;

    private SurfaceView cameraView;

    ArrayAdapter<String> adapter;
    ArrayList<String> data = new ArrayList<>();

    private static final int TAG_SCRFD = 0;
    private static final int TAG_YOLOV8 = 1;


    /**
     * Called when the activity is first created.
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        cameraView = findViewById(R.id.cameraview);

        cameraView.getHolder().setFormat(PixelFormat.RGBA_8888);
        cameraView.getHolder().addCallback(this);

        Button buttonSwitchCamera = findViewById(R.id.buttonSwitchCamera);
        buttonSwitchCamera.setOnClickListener(arg0 -> {
            int new_facing = 1 - facing;
            ncnn.closeCamera();
            ncnn.openCamera(new_facing);
            facing = new_facing;
        });




        Button btn_jump_to_img = findViewById(R.id.btn_jump_to_img);

        btn_jump_to_img.setOnClickListener(v -> {
//            if (ContextCompat.checkSelfPermission(getApplicationContext(), Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_DENIED) {
                ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, REQUEST_WRITE);
//            }
            startActivity(new Intent(this,ImageActivity.class));
        });

        Button openCamera = findViewById(R.id.open_camera);
        openCamera.setOnClickListener(arg0 -> {
            reload();
            ncnn.openCamera(facing);
        });

        adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, data);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        changeWeight();

        spinnerModel = findViewById(R.id.spinnerType);
        spinnerModel.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> arg0, View arg1, int position, long id) {
                if (position != current_model) {
                    current_model = position;
                    changeWeight();
                    reload();
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> arg0) {
            }
        });


        spinnerModelWeight = findViewById(R.id.spinnerModel);
        spinnerModelWeight.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> arg0, View arg1, int position, long id) {
                if (position != current_model_weight) {
                    current_model_weight = position;
                    reload();
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> arg0) {
            }
        });
        spinnerModelWeight.setAdapter(adapter);

        spinnerCPUGPU = findViewById(R.id.spinnerCPUGPU);
        spinnerCPUGPU.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> arg0, View arg1, int position, long id) {
                if (position != current_cpugpu) {
                    current_cpugpu = position;
                    reload();
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> arg0) {
            }
        });
    }

    private void reload() {
        switch (current_model) {
            case TAG_SCRFD:
                ncnn = new SCRFDNcnn();
                break;
            case TAG_YOLOV8:
                ncnn = new Yolov8Ncnn();
                break;
        }

        boolean ret_init = ncnn.loadCurModel(getAssets(), current_model_weight, current_cpugpu);
        if (!ret_init) {
            Log.e("MainActivity", "scrfdncnn loadModel failed");
        }
    }

    private void changeWeight() {
        data.clear();
        ArrayList<String> weights = new ArrayList<>();
        switch (current_model) {
            case TAG_SCRFD:
                weights = getModelWeightList(R.array.model_array);
                break;
            case TAG_YOLOV8:
                weights = getModelWeightList(R.array.model_yolo_array);
                break;
        }
        data.addAll(weights);
        adapter.notifyDataSetChanged();
    }


    private ArrayList<String> getModelWeightList(int res) {
        String[] modelArray = getResources().getStringArray(res);
        // 将字符串数组转换为ArrayList
        ArrayList<String> modelList = new ArrayList<>(Arrays.asList(modelArray));
        return modelList;
    }


    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // 设置显示的SurfaceView ANativeWindow
        ncnn.setOutputWindow(holder.getSurface());
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
    }

    @Override
    public void onResume() {
        super.onResume();

        if (ContextCompat.checkSelfPermission(getApplicationContext(), Manifest.permission.CAMERA) == PackageManager.PERMISSION_DENIED) {
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.CAMERA}, REQUEST_CAMERA);
        }

    }

    @Override
    public void onPause() {
        super.onPause();
        if (ncnn != null) {
            ncnn.closeCamera();
        }

    }


}
