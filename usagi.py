#!/usr/bin/env python3
"""
=============================================================================
             ★ 3D SPINNING ASCII USAGI & SPIDER-USAGI ★
            Inspired by ai_caliber's viral ASCII Usagi animation
=============================================================================
Revolves the 2D Usagi character into a full 3D rotating model and renders it
as textured, shaded ASCII letters with 24-bit TrueColor ANSI output in terminal.
"""

import os
import sys
import math
import time
import shutil
import select
import termios
import tty
import signal
from PIL import Image
import numpy as np

# -----------------------------------------------------------------------------
# 3D Model Generation via Silhouette Extraction & Revolution
# -----------------------------------------------------------------------------

def load_or_create_usagi_model():
    """Extracts silhouette from usagi.png and revolves into a dense 3D point cloud."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    img_path = os.path.join(script_dir, "usagi.png")

    if not os.path.exists(img_path):
        img_path = "/Users/adhi/.gemini/antigravity/brain/08ab71a6-456b-43d6-bb3b-48f31f5c6383/.user_uploaded/media_1795055823162_b851aa07.png"

    img = Image.open(img_path).convert('RGBA')
    w, h = img.size
    arr = np.array(img)

    CX = 191.0  # Center X axis
    PX = 100.0  # Scale factor
    DEPTH = 0.82

    # 1. Body & Head: scanlines from Y=125 to 335
    ys = []
    halfs = []
    mids = []
    for y in range(125, 335, 2):
        row = arr[y]
        xs = np.where(row[:, 3] > 60)[0]
        if len(xs) > 10:
            ys.append(y)
            halfs.append((xs[-1] - xs[0]) / 2.0)
            mids.append((xs[0] + xs[-1]) / 2.0)

    ys = np.array(ys, dtype=np.float32)
    halfs = np.array(halfs, dtype=np.float32)
    mids = np.array(mids, dtype=np.float32)

    # Compute accurate normal slopes dr/dy along the profile
    dr_dy = np.gradient(halfs, ys)
    denom = np.sqrt(1.0 + dr_dy * dr_dy)
    Nr_all = 1.0 / denom
    Ny_all = dr_dy / denom  # points up at top, down at bottom

    pts, nrms, cols, typs = [], [], [], []
    # Types:
    # 0: fur (cream yellow)
    # 1: inner ear (soft pastel pink)
    # 2: eye (jet black)
    # 3: highlight (bright white)
    # 4: blush (pink with ticks)
    # 5: mouth (tongue/open)
    # 6: tail (fluffy white)

    for row_i in range(len(ys)):
        y_int = int(ys[row_i])
        half = halfs[row_i]
        mid = mids[row_i]
        Nr = Nr_all[row_i]
        Ny = Ny_all[row_i]
        y_3d = (225.0 - y_int) / PX

        n_phi = max(42, int(half * 2.2))
        phis = np.linspace(0, 2.0 * np.pi, n_phi, endpoint=False)
        cos_p = np.cos(phis)
        sin_p = np.sin(phis)

        xs_3d = (half * cos_p) / PX
        zs_3d = (half * DEPTH * sin_p) / PX

        for i in range(n_phi):
            cp, sp = cos_p[i], sin_p[i]
            x3, z3 = xs_3d[i], zs_3d[i]
            pt_type = 0

            nx = Nr * cp
            ny = Ny
            nz = Nr * sp

            if sp >= -0.15:
                # Front face: sample exact pixel from 2D illustration
                ix = int(round(mid + half * cp))
                ix = max(0, min(w - 1, ix))
                pix = arr[y_int, ix]
                if pix[3] > 40:
                    r, g, b = int(pix[0]), int(pix[1]), int(pix[2])
                    lum = (r * 0.299 + g * 0.587 + b * 0.114) / 255.0
                    if lum < 0.22:
                        pt_type = 5 if y_int > 218 else 2
                    elif lum > 0.94:
                        pt_type = 3
                    elif r > 235 and g < 195 and b < 215:
                        pt_type = 4
                else:
                    r, g, b = 254, 238, 175
            else:
                # Back face: creamy yellow fur
                r, g, b = 254, 238, 175

            pts.append([x3, y_3d, z3])
            nrms.append([nx, ny, nz])
            cols.append([r, g, b])
            typs.append(pt_type)

    # 2. Rabbit Ears: scanlines from Y=40 to 125
    for sign in (-1.0, 1.0):
        for y_int in range(40, 125, 2):
            xs = np.where(arr[y_int][:, 3] > 60)[0]
            exs = xs[xs < CX] if sign < 0 else xs[xs > CX]
            if len(exs) < 2:
                continue
            half = (exs[-1] - exs[0]) / 2.0
            mid = (exs[0] + exs[-1]) / 2.0
            y_3d = (225.0 - y_int) / PX

            n_phi = 24
            phis = np.linspace(0, 2.0 * np.pi, n_phi, endpoint=False)
            cos_p = np.cos(phis)
            sin_p = np.sin(phis)

            for i in range(n_phi):
                cp, sp = cos_p[i], sin_p[i]
                x3 = (mid - CX + half * cp) / PX
                z3 = (half * 0.65 * sp) / PX
                pt_type = 0

                if sp >= -0.15:
                    ix = int(round(mid + half * cp))
                    ix = max(0, min(w - 1, ix))
                    pix = arr[y_int, ix]
                    if pix[3] > 40:
                        r, g, b = int(pix[0]), int(pix[1]), int(pix[2])
                        if r > 225 and g > 145 and g < 215 and b > 165:
                            pt_type = 1  # inner ear pink
                    else:
                        r, g, b = 254, 238, 175
                else:
                    r, g, b = 254, 238, 175

                pts.append([x3, y_3d, z3])
                nrms.append([cp, 0.1, sp])
                cols.append([r, g, b])
                typs.append(pt_type)

    # 3. Feet: scanlines from Y=335 to 360
    for sign in (-1.0, 1.0):
        for y_int in range(335, 360, 2):
            xs = np.where(arr[y_int][:, 3] > 60)[0]
            fxs = xs[xs < CX] if sign < 0 else xs[xs > CX]
            if len(fxs) < 2:
                continue
            half = (fxs[-1] - fxs[0]) / 2.0
            mid = (fxs[0] + fxs[-1]) / 2.0
            y_3d = (225.0 - y_int) / PX

            for p in range(16):
                phi = 2.0 * math.pi * p / 16.0
                cp, sp = math.cos(phi), math.sin(phi)
                x3 = (mid - CX + half * cp) / PX
                z3 = (half * 0.70 * sp) / PX
                pts.append([x3, y_3d, z3])
                nrms.append([cp, -0.4, sp])
                cols.append([254, 238, 175])
                typs.append(0)

    # 4. Fluffy Tail on lower back
    tail_y = (225.0 - 275.0) / PX
    tail_r = 0.22
    for u in np.linspace(0, np.pi / 2, 10):
        for v in np.linspace(0, 2 * np.pi, 20, endpoint=False):
            x3 = tail_r * np.sin(u) * np.cos(v)
            y3 = tail_y + tail_r * np.sin(u) * np.sin(v)
            z3 = - (0.80 + tail_r * np.cos(u))  # protruding back
            pts.append([x3, y3, z3])
            nrms.append([np.sin(u) * np.cos(v), np.sin(u) * np.sin(v), -np.cos(u)])
            cols.append([255, 255, 250])
            typs.append(6)

    pts = np.array(pts, dtype=np.float32)
    nrms = np.array(nrms, dtype=np.float32)
    cols = np.array(cols, dtype=np.uint8)
    typs = np.array(typs, dtype=np.uint8)

    # Center points along vertical axis so Usagi rotates cleanly around center of mass
    pts[:, 1] -= 0.25

    return pts, nrms, cols, typs

# -----------------------------------------------------------------------------
# Spider-Man Shader
# -----------------------------------------------------------------------------

def shade_spiderman(pt, nx, ny, nz):
    """Transforms point into Spider-Man suit theme with web patterns and mask eyes."""
    r, g, b = 220, 28, 36  # Scarlet red
    ch = '#'

    # Blue flanks and lower body
    if abs(pt[0]) > 0.42 and pt[1] < 0.22 and pt[2] < 0.15:
        r, g, b = 24, 76, 160
    elif pt[1] < -0.38 and pt[2] < 0.05:
        r, g, b = 24, 76, 160

    # Web lines (spider-webbing)
    if r > 150:
        r_web = math.sqrt(pt[0] * pt[0] * 1.3 + (pt[1] + 0.05) ** 2)
        ang = math.atan2(pt[1] + 0.05, pt[0])
        if math.sin(r_web * 32.0) > 0.88 or abs(math.sin(ang * 10.0)) > 0.96:
            r, g, b = 20, 20, 26
            ch = '+'

    # Spider eyes
    if pt[2] > 0.15:
        for sign in (-1.0, 1.0):
            ex = (pt[0] - sign * 0.24) * sign
            ey = pt[1] - 0.02
            if -0.09 < ex < 0.15 and -0.09 < ey < 0.12:
                dU = ey - (0.03 + 0.55 * ex)
                dL = ey - (-0.06 + 0.40 * ex * ex)
                if -0.03 < dU < 0.03 and -0.03 < dL < 0.03:
                    is_rim = dU > 0.005 or dL < -0.005 or ex < -0.07 or ex > 0.13
                    if is_rim:
                        r, g, b = 20, 20, 26
                        ch = '@'
                    else:
                        r, g, b = 255, 255, 255
                        ch = 'W'

    return r, g, b, ch

# -----------------------------------------------------------------------------
# Main Animation Loop
# -----------------------------------------------------------------------------

def main():
    pts, nrms, cols, typs = load_or_create_usagi_model()

    # Themes & Arguments
    theme = 0  # 0: Classic Usagi, 1: Spider-Man
    snapshot_mode = False
    zoom = 1.0

    args = sys.argv[1:]
    arg_i = 0
    while arg_i < len(args):
        arg = args[arg_i]
        if arg in ('--spiderman', '-s', '--spider'):
            theme = 1
        elif arg in ('--snapshot', '--test'):
            snapshot_mode = True
        elif arg in ('--big', '-b'):
            zoom = 1.35
        elif arg in ('--huge', '-H'):
            zoom = 1.65
        elif arg in ('--giant', '-g'):
            zoom = 2.00
        elif arg in ('--zoom', '-z'):
            if arg_i + 1 < len(args):
                arg_i += 1
                try:
                    zoom = float(args[arg_i])
                except ValueError:
                    pass
        elif arg.startswith('--zoom='):
            try:
                zoom = float(arg.split('=', 1)[1])
            except ValueError:
                pass
        arg_i += 1

    auto_spin = True
    spin_speed = 1.85
    yaw = 0.0
    pitch = 0.06
    total_time = 0.0

    # Directional light source (top-right-front)
    lx, ly, lz = 0.35, 0.70, 0.60
    l_mag = math.sqrt(lx * lx + ly * ly + lz * lz)
    lx /= l_mag; ly /= l_mag; lz /= l_mag

    # Setup non-blocking terminal
    is_tty = sys.stdin.isatty()
    orig_settings = None
    if is_tty and not snapshot_mode:
        orig_settings = termios.tcgetattr(sys.stdin)
        tty.setcbreak(sys.stdin.fileno())

    # Switch to alternate screen if not snapshot
    if not snapshot_mode:
        sys.stdout.write("\033[?1049h\033[?25l\033[2J\033[H")
        sys.stdout.flush()

    running = True
    def cleanup(sig=None, frame=None):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, cleanup)
    signal.signal(signal.SIGTERM, cleanup)

    last_time = time.perf_counter()

    try:
        while running:
            now = time.perf_counter()
            dt = now - last_time
            last_time = now
            total_time += dt

            # Handle non-blocking keyboard input
            if select.select([sys.stdin], [], [], 0)[0]:
                ch = sys.stdin.read(1)
                if not ch or ch in ('q', 'Q', '\x1b'):
                    if ch == '\x1b':
                        if select.select([sys.stdin], [], [], 0.02)[0]:
                            seq = sys.stdin.read(2)
                            if seq == '[A': pitch = min(1.2, pitch + 0.15)
                            elif seq == '[B': pitch = max(-1.2, pitch - 0.15)
                            elif seq == '[C': yaw -= 0.20
                            elif seq == '[D': yaw += 0.20
                        else:
                            break
                    else:
                        break
                elif ch == ' ':
                    auto_spin = not auto_spin
                elif ch in ('s', 'S'):
                    theme = 0 if theme == 1 else 1
                elif ch in ('a', 'A'):
                    if auto_spin: spin_speed = max(-6.0, spin_speed - 0.5)
                    else: yaw -= 0.15
                elif ch in ('d', 'D'):
                    if auto_spin: spin_speed = min(6.0, spin_speed + 0.5)
                    else: yaw += 0.15
                elif ch in ('w', 'W'):
                    pitch = min(1.2, pitch + 0.15)
                elif ch in ('x', 'X'):
                    pitch = max(-1.2, pitch - 0.15)
                elif ch in ('+', '='):
                    zoom = min(3.5, zoom * 1.15)
                elif ch in ('-', '_'):
                    zoom = max(0.35, zoom * 0.88)
                elif ch in ('0', ):
                    zoom = 1.0
                elif ch in ('b', 'B'):
                    zoom = 1.0 if zoom >= 1.3 else 1.35
                elif ch in ('r', 'R'):
                    yaw = 0.0; pitch = 0.06; spin_speed = 1.85; zoom = 1.0

            if auto_spin:
                yaw += spin_speed * dt
                if yaw > 2.0 * math.pi: yaw -= 2.0 * math.pi
                if yaw < 0.0: yaw += 2.0 * math.pi

            # Terminal size
            term_w, term_h = shutil.get_terminal_size((80, 36))

            # Camera Projection Parameters: Auto-fit to fill the terminal window generously!
            avail_h = max(14, term_h - 2)
            avail_w = max(24, term_w - 4)
            cam_dist = 3.0

            scale_from_h = avail_h / 1.067
            scale_from_w = avail_w / 1.517
            base_scale = min(scale_from_h, scale_from_w) * 0.96
            scale_y = base_scale * zoom
            scale_x = scale_y * 2.05

            # 3D Rotation Matrix
            cy, sy_rot = math.cos(yaw), math.sin(yaw)
            cp, sp_rot = math.cos(pitch), math.sin(pitch)

            R = np.array([
                [cy, 0, sy_rot],
                [-sy_rot * sp_rot, cp, cy * sp_rot],
                [-sy_rot * cp, -sp_rot, cy * cp]
            ], dtype=np.float32)

            rot_pts = pts @ R.T
            rot_nrms = nrms @ R.T

            d = rot_pts[:, 2] + cam_dist
            inv_d = 1.0 / np.maximum(0.2, d)

            screen_xs = (term_w * 0.5 + (rot_pts[:, 0] * inv_d) * scale_x).astype(np.int32)
            screen_ys = (term_h * 0.5 - (rot_pts[:, 1] * inv_d) * scale_y).astype(np.int32)

            # Diffuse lighting
            diffuse = np.maximum(0.0, rot_nrms[:, 0] * lx + rot_nrms[:, 1] * ly + rot_nrms[:, 2] * lz)
            shades = 0.72 + 0.32 * diffuse

            # Painter's order (back-to-front by Z)
            order = np.argsort(rot_pts[:, 2])

            total_cells = term_w * term_h
            chbuf = [' '] * total_cells
            cbuf = [(0, 0, 0)] * total_cells

            for idx in order:
                sx = screen_xs[idx]
                sy = screen_ys[idx]
                if 0 <= sx < term_w and 0 <= sy < term_h:
                    buf_idx = sy * term_w + sx
                    t = typs[idx]
                    pt = rot_pts[idx]
                    nx, ny, nz = rot_nrms[idx]
                    sh = shades[idx]

                    if theme == 1:
                        # Spider-Man theme
                        r, g, b, ch = shade_spiderman(pts[idx], nx, ny, nz)
                        r = min(255, max(0, int(r * sh)))
                        g = min(255, max(0, int(g * sh)))
                        b = min(255, max(0, int(b * sh)))
                    else:
                        # Classic Usagi
                        base_r, base_g, base_b = cols[idx]

                        # Hand-drawn anime ink contour rim
                        is_rim = abs(nz) < 0.20

                        if is_rim:
                            # Ink outline
                            r, g, b = 28, 22, 18
                            ch = '@'
                        elif t == 2:
                            # Jet black eyes
                            r, g, b = 20, 18, 18
                            ch = '@'
                        elif t == 3:
                            # Sparkling white pupil highlights
                            r, g, b = 255, 255, 255
                            ch = 'W'
                        elif t == 4:
                            # Blushing cheeks with tick marks
                            tick = ((sx + sy) % 3 == 0)
                            if tick:
                                r, g, b = 35, 22, 22
                                ch = '/'
                            else:
                                r, g, b = 255, 174, 192
                                ch = '%'
                        elif t == 5:
                            # Open mouth / cute tongue
                            r, g, b = 232, 93, 117
                            ch = 'o'
                        elif t == 1:
                            # Soft pink inner ear
                            r = min(255, max(0, int(base_r * sh)))
                            g = min(255, max(0, int(base_g * sh)))
                            b = min(255, max(0, int(base_b * sh)))
                            ch = '*'
                        elif t == 6:
                            # Fluffy white tail on back
                            tail_sh = 0.88 + 0.18 * diffuse[idx]
                            r = min(255, max(0, int(base_r * tail_sh)))
                            g = min(255, max(0, int(base_g * tail_sh)))
                            b = min(255, max(0, int(base_b * tail_sh)))
                            ch = 'o'
                        else:
                            # Creamy yellow fur
                            r = min(255, max(0, int(base_r * sh)))
                            g = min(255, max(0, int(base_g * sh)))
                            b = min(255, max(0, int(base_b * sh)))
                            ch = '#' if (sx + sy) % 2 == 0 else 'M'

                    chbuf[buf_idx] = ch
                    cbuf[buf_idx] = (r, g, b)

            # Count total active non-space letters rendered
            letter_count = sum(1 for c in chbuf if c != ' ')

            # Format HUD matching ai_caliber's viral video
            rot_deg = math.degrees(yaw) % 360.0
            puri = 1 if (140.0 < rot_deg < 220.0) else 0

            title_text = "usagi.py - spiderman" if theme == 1 else "usagi.py - ascii"
            rot_info = f"rotation {rot_deg:6.2f}°  puri {puri}"
            if abs(zoom - 1.0) > 0.04:
                rot_info += f"  zoom {zoom:.2f}x"

            hud_texts = [
                (title_text, (140, 140, 150)),
                (rot_info, (230, 230, 240)),
                (f"letters {letter_count:,}", (140, 140, 150)),
                (f"● {total_time:05.2f}s", (245, 195, 45))
            ]

            # Embed HUD directly into buffer top-left
            for row_idx, (text, col) in enumerate(hud_texts):
                for col_idx, char in enumerate(text):
                    if col_idx < term_w and row_idx < term_h:
                        buf_idx = row_idx * term_w + col_idx
                        chbuf[buf_idx] = char
                        cbuf[buf_idx] = col

            # Embed control hint at bottom line
            hint_text = "[Space] Pause  [s] Spider-Man  [+/-] Zoom  [b] Big  [a/d] Rotate  [w/x] Tilt  [q] Quit"
            if term_h > 15 and len(hint_text) < term_w:
                start_c = max(0, (term_w - len(hint_text)) // 2)
                row_idx = term_h - 1
                for c_off, char in enumerate(hint_text):
                    col_idx = start_c + c_off
                    if col_idx < term_w:
                        buf_idx = row_idx * term_w + col_idx
                        chbuf[buf_idx] = char
                        cbuf[buf_idx] = (110, 115, 125)

            # Assemble frame string with stateful ANSI color caching
            lines = []
            last_color = None
            reset_code = "\033[0m"

            for r in range(term_h):
                row_chars = []
                row_offset = r * term_w
                for c in range(term_w):
                    buf_idx = row_offset + c
                    ch = chbuf[buf_idx]
                    if ch == ' ':
                        if last_color is not None:
                            row_chars.append(reset_code)
                            last_color = None
                        row_chars.append(' ')
                    else:
                        col = cbuf[buf_idx]
                        if col != last_color:
                            row_chars.append(f"\033[38;2;{col[0]};{col[1]};{col[2]}m")
                            last_color = col
                        row_chars.append(ch)
                if last_color is not None:
                    row_chars.append(reset_code)
                    last_color = None
                lines.append("".join(row_chars))

            sys.stdout.write("\033[H" + "\n".join(lines))
            sys.stdout.flush()

            if snapshot_mode:
                break

            # Target ~35 FPS
            elapsed = time.perf_counter() - now
            time.sleep(max(0.001, 0.028 - elapsed))

    finally:
        if orig_settings and is_tty:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, orig_settings)
        if not snapshot_mode:
            sys.stdout.write("\033[?25h\033[0m\033[?1049l\n")
            sys.stdout.write("\033[1;32m★ Thank you for watching Usagi spin! Yahaha!! (o・∇・o) ★\033[0m\n\n")
            sys.stdout.flush()

if __name__ == '__main__':
    main()
