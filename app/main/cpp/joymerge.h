#pragma once
#include <jni.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <pthread.h>
#include <atomic>
#include <vector>
#include <string>

struct AxisState { int x=0, y=0; };
struct Triggers { int lt=0, rt=0; };

struct PadState {
    AxisState left, right; 
    Triggers trg;          
    int hat_x=0, hat_y=0;  
    uint32_t buttons=0;    
};
