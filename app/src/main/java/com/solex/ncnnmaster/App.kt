package com.solex.ncnnmaster

import android.app.Application
import kotlin.properties.Delegates

class App : Application() {

    companion object {
        var instance: App by Delegates.notNull()
            private set
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
    }



}