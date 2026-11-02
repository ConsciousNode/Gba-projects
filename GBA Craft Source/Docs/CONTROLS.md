# GBA-Craft Controls & Menus

## In-Game Controls

| Input | Action |
|---|---|
| **D-pad ↑ / ↓** | Walk forward / backward (`~4.3 blocks/sec`) |
| **D-pad ← / →** | Turn camera left / right (yaw) |
| **L held + D-pad ↑ / ↓** | Look up / down (slides y-shear horizon `40 ± 32`) |
| **L held + D-pad ← / →** | Strafe left / right |
| **R** | Jump (`1.25 blocks` apex; clears 1-block ledges) |
| **A (hold)** | Break targeted block (progress bar shown under crosshair) |
| **B** | Place selected hotbar block on targeted block face |
| **SELECT** | Cycle to next hotbar slot (`1..9`) |
| **SELECT held + A** | Craft: converts `1 WOOD` in hotbar into `4 PLANKS` |
| **START** | Open Pause Menu |

---

## Block Break Times & Drops

| Block Type | Break Time | Drop into Hotbar |
|---|---|---|
| `GRASS` | `~0.3 s` (`18 VBlanks`) | `1 DIRT` |
| `DIRT` | `~0.3 s` (`18 VBlanks`) | `1 DIRT` |
| `SAND` | `~0.3 s` (`18 VBlanks`) | `1 SAND` |
| `LEAVES` | `~0.3 s` (`18 VBlanks`) | `1 LEAVES` (`75%` chance) or nothing (`25%` chance) |
| `WOOD` (log) | `~0.6 s` (`36 VBlanks`) | `1 WOOD` |
| `PLANKS` | `~0.6 s` (`36 VBlanks`) | `1 PLANKS` |
| `STONE` | `~1.0 s` (`60 VBlanks`) | `1 STONE` |
| `BEDROCK` | Unbreakable | None |

---

## Menus & Self-Test

### Title Screen
- **New World**: Opens the 8-hex-digit seed editor initialized from the frame counter.
- **Continue**: Appears and is selectable only when a valid `"GBAC"` save with matching FNV-1a checksum exists in SRAM.
- **Hold `SELECT`**: Immediately runs the 9 built-in acceptance self-tests and displays `PASS`/`FAIL` results plus render scanline timing.

### Seed Entry Screen
- **D-pad ← / →**: Select hex digit (`0..7`).
- **D-pad ↑ / ↓**: Increment / decrement hex nybble (`0..F`).
- **A / START**: Generate world from seed and spawn at world center.
- **B**: Cancel and return to previous menu.

### Pause Menu (`START`)
- **Resume**: Return to gameplay.
- **Save World**: Compares all `4,096` columns against `gen_column(seed, x, z)` and writes diff records to 32 KB SRAM. Displays `"World too changed to save fully: N edits dropped"` if edits exceed `8,176` records.
- **Load World**: Restores world and player state from SRAM.
- **New World**: Opens the 8-hex-digit seed editor.
- **Toggle FPS Counter**: Shows or hides the top-left `FPS` and `X / Y / Z` block coordinate HUD.
- **Help / Controls**: Displays the in-game controls reference screen.
