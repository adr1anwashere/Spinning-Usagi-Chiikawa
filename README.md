# 🐰 3D Spinning ASCII Usagi & Spider-Usagi in Terminal

Inspired by **ai_caliber**'s viral ASCII Usagi animation post (`usagi.py - ascii`), this project renders a full 3D spinning **Usagi** (from *Chiikawa*) directly inside your terminal with authentic anime colors, 24-bit TrueColor ANSI output, and an interactive **Spider-Man mode**!

---

## 🚀 Quick Start (Python - Matching ai_caliber's viral video)

Run directly in your terminal:

```bash
# Default Auto-Fit (generously fills terminal)
python3 usagi.py

# Big Mode (extra large view)
python3 usagi.py --big

# Huge Mode (~3,600+ letters, matching the viral video!)
python3 usagi.py --huge

# Or launch directly in Spider-Man Mode
python3 usagi.py --spiderman
```

*(You can also set custom zoom via `python3 usagi.py --zoom 1.5` or test with `--snapshot`)*

---

## ✨ Features

- **Exact Viral HUD & Aesthetics**:
  ```text
  usagi.py - ascii
  rotation 356.15°  puri 0
  letters 1,482
  ● 01.98s
  ```
- **"Puri" Detection**: When Usagi spins around and shows its cute fluffy tail/butt, the HUD automatically triggers `puri 1`!
- **3D Surface Revolution & Silhouette Extraction**: Samples authentic colors, blushing cheeks with tick marks (`///`), open smiling mouth, and shiny black eyes directly from the original Usagi illustration.
- **Hand-Drawn Anime Ink Contour**: Uses view-space normal culling to render crisp black ink outlines around the silhouette during 3D rotation.
- **🕸️ Spider-Man / Spider-Usagi Mode**:
  - Toggle anytime during animation with `[s]`!
  - Scarlet suit (`#DC1C28`), Marvel blue flanks, radial webbing lines, and sharp white mask eyes.
- **Dynamic Terminal Sizing**: Adapts automatically to any terminal size (`80x24`, `100x40`, or full-screen) with 2:1 character aspect ratio compensation.
- **Silky Smooth 35+ FPS**: Vectorized NumPy transformation and stateful ANSI buffering for flicker-free performance.

---

## 🎮 Interactive Controls

While running in your terminal:

| Key | Action |
| :--- | :--- |
| `[Space]` | **Pause / Resume** auto-rotation |
| `[s]` | **Toggle Spider-Man** $\leftrightarrow$ **Classic Usagi** |
| `[b]` | **Toggle Big Mode** (instantly enlarges Usagi!) |
| `[+]` / `[=]` | **Zoom In** (up to 3.5x close-up) |
| `[-]` / `[_]` | **Zoom Out** |
| `[0]` | **Reset Zoom** to 1.0x |
| `[a]` / `[d]` or `[←]` / `[→]` | Rotate manually or change spin speed |
| `[w]` / `[x]` or `[↑]` / `[↓]` | Tilt pitch up / down |
| `[r]` | Reset rotation, tilt, and zoom |
| `[q]` or `[Esc]` | Clean exit (safely restores terminal buffer and cursor) |

---

## ⚡ Alternative C++ Implementation

A standalone C++17 raymarched SDF version is also provided in the repository:

```bash
make
./Usagi             # Standard C++ raymarched dual-pixel mode
./Usagi --spiderman # Launch directly into Spider-Man mode
```
