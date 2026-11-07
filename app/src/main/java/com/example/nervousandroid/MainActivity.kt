package com.example.nervousandroid

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import com.nervous.game.*

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        GameEngine.newGame(20, 20)

        setContent {
            var state by remember { mutableStateOf(GameEngine.state()) }
            var message by remember { mutableStateOf("") }

            Column(Modifier.fillMaxSize().padding(16.dp)) {
                state?.let { s ->
                    Text("Phase : ${s.phase}  |  Joueur : J${s.currentPlayer + 1}  |  Placés : ${s.placedThisTurn}")
                    Text("PV  J1 : ${s.hp1}   J2 : ${s.hp2}")
                    Text(message)
                    Spacer(Modifier.height(8.dp))

                    GameGrid(s) { x, y ->
                        // On relit la phase fraîche du moteur au moment du clic
                        val current = GameEngine.state() ?: return@GameGrid
                        val r = when (current.phase) {
                            Phase.PLACE_BASE_P1,
                            Phase.PLACE_BASE_P2,
                            Phase.RELOCATE_BASE -> GameEngine.placeBase(x, y)
                            Phase.PLAY_SQUARES -> GameEngine.placeSquare(x, y)
                            else -> null
                        }
                        message = r?.message ?: ""
                        state = GameEngine.state()
                    }

                    Spacer(Modifier.height(12.dp))

                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        Button(onClick = {
                            val r = GameEngine.endTurn()
                            message = r?.message ?: ""
                            state = GameEngine.state()
                        }) { Text("Fin de tour") }

                        Button(onClick = {
                            GameEngine.newGame(20, 20)
                            message = ""
                            state = GameEngine.state()
                        }) { Text("Recommencer") }
                    }
                }
            }
        }
    }
}

@Composable
fun GameGrid(s: GameState, onCellClick: (Int, Int) -> Unit) {
    // Toujours utiliser le dernier état et le dernier callback dans le gestionnaire de tap
    val currentOnClick by rememberUpdatedState(onCellClick)
    val currentState by rememberUpdatedState(s)

    Canvas(
        Modifier
            .fillMaxWidth()
            .aspectRatio(s.width.toFloat() / s.height)
            .pointerInput(Unit) {
                detectTapGestures { offset ->
                    val st = currentState
                    val cw = size.width / st.width.toFloat()
                    val ch = size.height / st.height.toFloat()
                    val x = (offset.x / cw).toInt().coerceIn(0, st.width - 1)
                    val y = (offset.y / ch).toInt().coerceIn(0, st.height - 1)
                    currentOnClick(x, y)
                }
            }
    ) {
        val cw = size.width / s.width
        val ch = size.height / s.height
        for (y in 0 until s.height) {
            for (x in 0 until s.width) {
                val color = when (s.cellAt(x, y)) {
                    Cell.EMPTY   -> Color(0xFFEEEEEE)
                    Cell.SQUARE1 -> Color(0xFFE57373)
                    Cell.SQUARE2 -> Color(0xFF64B5F6)
                    Cell.BASE1   -> Color(0xFFB71C1C)
                    Cell.BASE2   -> Color(0xFF0D47A1)
                }
                drawRect(
                    color,
                    topLeft = Offset(x * cw, y * ch),
                    size = Size(cw - 1f, ch - 1f)
                )
            }
        }
    }
}