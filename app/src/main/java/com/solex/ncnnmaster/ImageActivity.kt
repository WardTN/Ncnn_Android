package com.solex.ncnnmaster

import android.graphics.BitmapFactory
import android.os.Bundle
import android.os.Environment
import android.widget.Button
import android.widget.ImageView
import androidx.appcompat.app.AppCompatActivity
import java.io.File
import java.io.FileOutputStream
import java.io.IOException


class ImageActivity : AppCompatActivity() {

    private var yolov8obb: Yolov8obb? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_image)
        Thread {
            copyFilesFromAssets(this, "resource", getAseetFilePath() + "/")
        }.start()


        yolov8obb = Yolov8obb()
        findViewById<Button>(R.id.btn).setOnClickListener {
            yolov8obb?.loadObbModel(assets, "v8s-obb.bin", "v8s-obb.param")
            var img = getAseetFilePath() + File.separator + "00007.jpg"
            var bitmap = BitmapFactory.decodeFile(img)
            findViewById<ImageView>(R.id.iv).setImageBitmap(bitmap)
            var outBitmap = yolov8obb?.obbDetect(bitmap)
            findViewById<ImageView>(R.id.iv).setImageBitmap(outBitmap)
        }
    }
}