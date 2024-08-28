package com.solex.ncnnmaster

import android.graphics.BitmapFactory
import android.os.Bundle
import android.os.Environment
import android.widget.Button
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
            copyFilesFromAssets(this,"resource", getAseetFilePath() + "/")
        }.start()


        yolov8obb = Yolov8obb()
        findViewById<Button>(R.id.btn).setOnClickListener {
            yolov8obb?.loadObbModel(assets,"v8s-obb.bin","v8s-obb.param",)
            var img = getAseetFilePath() + File.separator + "00007.jpg"
            var bitmap = BitmapFactory.decodeFile(img)
            yolov8obb?.obbDetect(bitmap)


//            val externalDir: File = Environment.getExternalStorageDirectory()
//            val file = File(getAseetFilePath(), "example.txt")
//            LoggerUtil.dq_log("文件路径为"+ file.absolutePath)
//            try {
//                // 创建文件输出流
//                val fos = FileOutputStream(file)
//                // 写入数据
//                fos.write("Hello, World!".toByteArray())
//                // 关闭流
//                fos.close()
//            } catch (e: IOException) {
//                e.printStackTrace()
//            }


        }
    }
}