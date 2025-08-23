package com.example.joymerge

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.Button
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            val running = remember { mutableStateOf(false) }
            Surface {
                Button(onClick = {
                    if (!running.value) startForegroundService(Intent(this, JoyMergeService::class.java))
                    else stopService(Intent(this, JoyMergeService::class.java))
                    running.value = !running.value
                }) { Text(if (running.value) "Stop JoyMerge" else "Start JoyMerge") }
            }
        }
    }
}
