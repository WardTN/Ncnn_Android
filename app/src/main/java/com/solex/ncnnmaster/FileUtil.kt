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
fun copyAssetsToFileDir(filePath: String, context: Context) {
    var filePath = filePath
    try {
        val fileList = context.assets.list(filePath)
        if (fileList!!.size > 0) { //如果是目录
            val file = File(getAseetFilePath() + "/" + filePath)
            file.mkdirs() //如果文件夹不存在，则递归
            for (fileName in fileList) {
                filePath = filePath + File.separator + fileName
                copyAssetsToFileDir(filePath, context)
                filePath = filePath.substring(0, filePath.lastIndexOf(File.separator))
            }
        } else { //如果是文件
            val inputStream: InputStream = context.assets.open(filePath)
            val file = File(getAseetFilePath() + "/" + filePath)
            if (!file.exists() || file.length() == 0L) {
                Log.e("CHEN",("copyFile $filePath"))
                val fos = FileOutputStream(file)
                var len = -1
                val buffer = ByteArray(1024)
                while (inputStream.read(buffer).also { len = it } != -1) {
                    fos.write(buffer, 0, len)
                }
                fos.flush()
                inputStream.close()
                fos.close()
            }
        }
    } catch (e: Exception) {
        LoggerUtil.dq_log("拷贝 Asset 文件出错")
        e.printStackTrace()
    }
}

