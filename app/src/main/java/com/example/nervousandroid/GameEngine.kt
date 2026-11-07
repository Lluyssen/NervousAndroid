package com.nervous.game

data class ActionResult(val ok: Boolean, val message: String)

object GameEngine {
    private const val BASE = 0
    private const val SQUARE = 1
    private const val END_TURN = 2
    private const val RELOCATE = 3

    fun newGame(w: Int = 20, h: Int = 20): Boolean = NativeBridge.nativeNewGame(w, h) == 0

    fun state(): GameState? = GameState.fromNative(NativeBridge.nativeGetState())

    fun placeBase(x: Int, y: Int) = run(BASE, x, y)
    fun placeSquare(x: Int, y: Int) = run(SQUARE, x, y)
    fun endTurn() = run(END_TURN, 0, 0)
    fun relocateBase(x: Int, y: Int) = run(RELOCATE, x, y)

    /** Positions légales sous forme de paires (x, y). kind : 0 bases, 1 carrés, 3 déplacements. */
    fun legal(kind: Int): List<Pair<Int, Int>> {
        val raw = NativeBridge.nativeGetLegal(kind)
        return List(raw.size / 2) { raw[it * 2] to raw[it * 2 + 1] }
    }

    private fun run(type: Int, x: Int, y: Int): ActionResult {
        val code = NativeBridge.nativeAction(type, x, y)
        return ActionResult(code == 0, NativeBridge.nativeDescribe(code))
    }
}