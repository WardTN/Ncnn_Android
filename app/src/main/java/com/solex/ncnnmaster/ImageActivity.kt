package com.solex.ncnnmaster

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.Button

class ImageActivity : AppCompatActivity() {

    private var yolov8obb: Yolov8obb? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_image)
        Thread {
            copyAssetsToFileDir("resource", this)
        }.start()


        yolov8obb = Yolov8obb()
        findViewById<Button>(R.id.btn).setOnClickListener {
            yolov8obb?.loadObbModel(assets,"v8s-obb.bin","v8s-obb.param",)
            var img = getAseetFilePath() + "/" + "00007.jpg"
            yolov8obb?.obbDetect(img)
        }
    }
}