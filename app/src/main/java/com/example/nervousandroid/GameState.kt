package com.nervous.game

enum class Phase { PLACE_BASE_P1, PLACE_BASE_P2, PLAY_SQUARES, RELOCATE_BASE, GAME_OVER }

enum class Cell { EMPTY, SQUARE1, SQUARE2, BASE1, BASE2 }

data class GameState(
    val width: Int,
    val height: Int,
    val phase: Phase,
    val currentPlayer: Int,      // 0 = J1 (rouge), 1 = J2 (bleu)
    val hp1: Int,
    val hp2: Int,
    val placedThisTurn: Int,
    val winner: Int,
    val relocatingPlayer: Int,
    val cells: List<Cell>
) {
    fun cellAt(x: Int, y: Int): Cell = cells[y * width + x]

    companion object {
        private const val HEADER = 9

        fun fromNative(raw: IntArray): GameState? {
            if (raw.size < HEADER) return null
            val w = raw[0]
            val h = raw[1]
            if (raw.size != HEADER + w * h) return null
            val cellValues = Cell.values()
            return GameState(
                width = w,
                height = h,
                phase = Phase.values()[raw[2]],
                currentPlayer = raw[3],
                hp1 = raw[4],
                hp2 = raw[5],
                placedThisTurn = raw[6],
                winner = raw[7],
                relocatingPlayer = raw[8],
                cells = List(w * h) { cellValues[raw[HEADER + it]] }
            )
        }
    }
}