package com.example.joymerge

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.IBinder
import androidx.core.app.NotificationCompat

class JoyMergeService : Service() {
    external fun nativeStart(rootHint: Boolean): Boolean
    external fun nativeStop()

    companion object { init { System.loadLibrary("joyuinput") } }

    override fun onCreate() {
        super.onCreate()
        startForeground(42, buildNotification())
        val ok = nativeStart(true)
        if (!ok) stopSelf()
    }

    override fun onDestroy() {
        nativeStop()
        super.onDestroy()
    }

    override fun onBind(intent: Intent?) = null

    private fun buildNotification(): Notification {
        val chanId = "joymerge"
        val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        if (nm.getNotificationChannel(chanId) == null) {
            nm.createNotificationChannel(NotificationChannel(chanId, "JoyMerge", NotificationManager.IMPORTANCE_LOW))
        }
        return NotificationCompat.Builder(this, chanId)
            .setContentTitle("JoyMerge is running")
            .setContentText("Merging two Joy‑Con into one virtual gamepad")
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
            .build()
    }
}
