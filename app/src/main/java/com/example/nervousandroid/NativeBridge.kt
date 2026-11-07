package com.example.nervousandroid

object NativeBridge {
    init { System.loadLibrary("nervous_jni") }
    external fun selfTest(): String
}