package com.solex.ncnnmaster

//import android.Manifest
//import android.content.Intent
//import android.os.Bundle
//import android.view.View
//import androidx.appcompat.app.AppCompatActivity
//import pub.devrel.easypermissions.EasyPermissions

//class SplashActivity : AppCompatActivity() {
//    val perms = arrayOf(
//        Manifest.permission.WRITE_EXTERNAL_STORAGE,
//        Manifest.permission.READ_EXTERNAL_STORAGE,
//        Manifest.permission.ACCESS_FINE_LOCATION,
//        Manifest.permission.CHANGE_NETWORK_STATE,
//        Manifest.permission.ACCESS_WIFI_STATE,
//        Manifest.permission.CAMERA
//    )
//
//
//    override fun onCreate(savedInstanceState: Bundle?) {
//        super.onCreate(savedInstanceState)
//        setContentView(R.layout.activity_splash)
//
//        findViewById<View>(R.id.btn_permission).setOnClickListener {
//            getLocationCheck()
//        }
//
//        findViewById<View>(R.id.btn_jump).setOnClickListener {
//            val intent = Intent(this, MainActivity::class.java)
//            startActivity(intent)
//            finish()
//        }
//        getLocationCheck()
//
//    }
//
//    private fun getLocationCheck() {
//        if (!hasPermissions()) {
//            EasyPermissions.requestPermissions(this,
//                "",
//                1,
//                perms[0],
//                perms[1],
//                perms[2],
//                perms[3],
//                perms[4],
//                perms[5])
//        } else {
//        }
//    }
//
//    private fun hasPermissions(): Boolean {
//        return EasyPermissions.hasPermissions(this,
//            perms[0],
//            perms[1],
//            perms[2],
//            perms[3],
//            perms[4],
//            perms[5])
//    }
//
//
//}