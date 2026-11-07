package com.nervous.game   // adapte à ton package

object NativeBridge {
    init {
        System.loadLibrary("nervous_jni")   // garde le nom que tu utilisais déjà
    }

    external fun nativeNewGame(w: Int, h: Int): Int
    external fun nativeAction(type: Int, x: Int, y: Int): Int
    external fun nativeGetState(): IntArray
    external fun nativeGetLegal(kind: Int): IntArray
    external fun nativeDescribe(code: Int): String
}