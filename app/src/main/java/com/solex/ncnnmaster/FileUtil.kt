package com.solex.ncnnmaster

import android.content.Context
import android.os.Build
import android.os.Environment
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream

// 主目录名
private const val HOME_PATH_NAME = "MediaStream"


private const val ASSETS = "Assets"

/**
 * 获取应用数据主目录
 * @return 主目录路径
 */
fun getHomePath(): String? {
    return getSDPath() + "/BallSkindev/"
}

fun getSDPath(): String? {
    var sdDir: File? = null
    val sdCardExist =
        Environment.getExternalStorageState() == Environment.MEDIA_MOUNTED // 判断sd卡是否存在
    sdDir = if (sdCardExist) {
        if (Build.VERSION.SDK_INT >= 29) {
            //Android10之后
            App.instance.getExternalFilesDir(null)
        } else {
            Environment.getExternalStorageDirectory() // 获取SD卡根目录
        }
    } else {
        Environment.getRootDirectory() // 获取跟目录
    }
    return sdDir.toString()
}


/**
 * 获取父目录下子目录
 */
fun getSubDir(parent: String?, dir: String): String? {
    if (parent == null) return null
    var subDirPath: String? = null
    try {
        // 获取展开的子目录路径
        val subDirFile = File(parent, dir)
        if (!subDirFile.exists()) subDirFile.mkdirs()
        subDirPath = subDirFile.canonicalPath
    } catch (e: Exception) {
        e.printStackTrace()
    }
    return subDirPath
}


fun getAseetFilePath(): String? {
    return getSubDir(getHomePath(), ASSETS)
}

/**
 * 从Asset 中拷贝文件 到 Files 下
 *
 * @param filePath 外部文件夹名
 */
fun copyFilesFromAssets(context: Context, oldPath: String, newPath: String) {
    try {
        val fileNames = context.assets.list(oldPath)
        if (fileNames!!.isNotEmpty()) {
            // directory
            val file = File(newPath)
            if (!file.mkdir()) {
                Log.d("mkdir", "can't make folder")
            }
            for (fileName in fileNames) {
                copyFilesFromAssets(context, "$oldPath/$fileName", "$newPath/$fileName")
            }
        } else {
            // file
            val `is` = context.assets.open(oldPath)
            val fos = FileOutputStream(File(newPath))
            val buffer = ByteArray(1024)
            var byteCount: Int
            while (`is`.read(buffer).also { byteCount = it } != -1) {
                fos.write(buffer, 0, byteCount)
            }
            fos.flush()
            `is`.close()
            fos.close()
        }
    } catch (e: java.lang.Exception) {
        // TODO Auto-generated catch block
        e.printStackTrace()
    }
}


