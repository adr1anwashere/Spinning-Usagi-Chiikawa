/*
 * ==============================================================================
 *                     ★ 3D SPINNING USAGI & SPIDER-USAGI ★
 *                       Chiikawa Usagi Terminal Animation
 *                               in C++ (C++17)
 * ==============================================================================
 *
 * Features:
 *   - 3D Raymarching SDF renderer directly in the terminal
 *   - Dual-pixel TrueColor (24-bit RGB) rendering via Unicode half-blocks ('▀')
 *   - Multiple render modes:
 *       1. High-Res TrueColor Half-Blocks (Super smooth anime cell-shading)
 *       2. TrueColor Shaded ASCII (24-bit RGB + ASCII luminance ramp)
 *       3. Retro Monochrome ASCII (Classic donut.c aesthetic)
 *   - Character Themes:
 *       [1] Classic Usagi (Iconic Chiikawa bunny: yellow fur, pink ears & blush,
 *                          shiny black anime eyes, open cat mouth, stubby limbs,
 *                          fluffy white tail!)
 *       [2] Spider-Usagi (Spider-Man suit, web pattern, spider eyes & emblem!)
 *       [3] Rainbow Party (Pulsing psychedelic disco Usagi!)
 *   - Interactive Controls:
 *       [Space]       Pause / Resume auto-rotation
 *       [s]           Toggle Spider-Usagi / Classic Usagi theme
 *       [c]           Cycle through all themes (Usagi -> Spider-Usagi -> Party)
 *       [m]           Cycle display modes (Half-block -> Colored ASCII -> Mono)
 *       [a] / [d]     Spin faster left / right (or manual spin when paused)
 *       [w] / [s]     Tilt pitch up / down
 *       [r]           Reset view & orientation
 *       [+] / [-]     Zoom in / Zoom out
 *       [y]           Trigger battle cry! ("YAHAHA!!", "URA!!", "SPIDEY-SENSE!")
 *       [h]           Toggle on-screen HUD & controls help
 *       [q] / [Esc]   Quit cleanly
 *
 * Compilation:
 *   clang++ -std=c++17 -O3 Usagi.cpp -o Usagi
 * Execution:
 *   ./Usagi
 *   ./Usagi --spiderman   (Start directly in Spider-Man mode)
 *   ./Usagi --ascii       (Start in colored ASCII mode)
 *   ./Usagi --mono        (Start in classic monochrome mode)
 * ==============================================================================
 */

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>

// -----------------------------------------------------------------------------
// Terminal & Signal Handling
// -----------------------------------------------------------------------------

static struct termios g_orig_termios;
static bool g_raw_mode_enabled = false;
static volatile sig_atomic_t g_running = 1;

void disableRawMode() {
    if (g_raw_mode_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        g_raw_mode_enabled = false;
    }
    // Show cursor, reset colors, switch back from alternate screen buffer
    std::cout << "\033[?25h\033[0m\033[?1049l" << std::flush;
}

void enableRawMode() {
    // Set non-blocking I/O on stdin
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags != -1) {
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    if (!isatty(STDIN_FILENO)) return;
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    std::atexit(disableRawMode);

    struct termios raw = g_orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    g_raw_mode_enabled = true;

    // Switch to alternate screen buffer, hide cursor, clear screen
    std::cout << "\033[?1049h\033[?25l\033[2J\033[H" << std::flush;
}

void handleSignal(int sig) {
    (void)sig;
    g_running = 0;
}

void setupSignalHandlers() {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);
}

// -----------------------------------------------------------------------------
// Math & Vector Utilities
// -----------------------------------------------------------------------------

struct Vec3 {
    float x, y, z;
    constexpr Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(float s) const { return Vec3(x / s, y / s, z / s); }

    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    float lengthSq() const { return dot(*this); }
    float length() const { return std::sqrt(lengthSq()); }

    Vec3 normalize() const {
        float l = length();
        return l > 1e-6f ? (*this) / l : Vec3(0, 1, 0);
    }
};

struct Color {
    int r, g, b;
    constexpr Color(int r = 0, int g = 0, int b = 0) : r(r), g(g), b(b) {}

    Color mix(const Color& o, float t) const {
        t = std::max(0.0f, std::min(1.0f, t));
        return Color(
            int(r * (1.0f - t) + o.r * t),
            int(g * (1.0f - t) + o.g * t),
            int(b * (1.0f - t) + o.b * t)
        );
    }

    Color scale(float s) const {
        return Color(
            std::clamp(int(r * s), 0, 255),
            std::clamp(int(g * s), 0, 255),
            std::clamp(int(b * s), 0, 255)
        );
    }
};

inline float clamp(float v, float a, float b) {
    return std::max(a, std::min(b, v));
}

inline float smin(float a, float b, float k) {
    float h = clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
    return (b * (1.0f - h) + a * h) - k * h * (1.0f - h);
}

// -----------------------------------------------------------------------------
// 3D Rotation Matrices
// -----------------------------------------------------------------------------

struct Mat3 {
    Vec3 r0, r1, r2;

    Vec3 mul(const Vec3& v) const {
        return Vec3(
            r0.x * v.x + r0.y * v.y + r0.z * v.z,
            r1.x * v.x + r1.y * v.y + r1.z * v.z,
            r2.x * v.x + r2.y * v.y + r2.z * v.z
        );
    }

    Mat3 transpose() const {
        return Mat3{
            Vec3(r0.x, r1.x, r2.x),
            Vec3(r0.y, r1.y, r2.y),
            Vec3(r0.z, r1.z, r2.z)
        };
    }

    static Mat3 rotation(float yaw, float pitch, float roll) {
        float cy = std::cos(yaw), sy = std::sin(yaw);
        float cp = std::cos(pitch), sp = std::sin(pitch);
        float cr = std::cos(roll), sr = std::sin(roll);

        // Combined rotation R = Rz(roll) * Rx(pitch) * Ry(yaw)
        return Mat3{
            Vec3(cy * cr + sy * sp * sr,   cp * sr,   -sy * cr + cy * sp * sr),
            Vec3(-cy * sr + sy * sp * cr,  cp * cr,    sy * sr + cy * sp * cr),
            Vec3(sy * cp,                 -sp,        cy * cp)
        };
    }
};

// -----------------------------------------------------------------------------
// Signed Distance Functions (SDF) for Usagi
// -----------------------------------------------------------------------------

inline float distSphere(Vec3 p, Vec3 c, float r) {
    return (p - c).length() - r;
}

inline float distCapsule(Vec3 p, Vec3 a, Vec3 b, float r) {
    Vec3 pa = p - a;
    Vec3 ba = b - a;
    float h = clamp(pa.dot(ba) / ba.dot(ba), 0.0f, 1.0f);
    return (pa - ba * h).length() - r;
}

// Full 3D model of Usagi
float usagiSDF(Vec3 p) {
    // 1. Chubby mochi/pear body (wider towards cheeks/bottom)
    float y_norm = clamp((p.y + 0.35f) / 0.85f, 0.0f, 1.0f);
    float width_factor = 1.0f + 0.16f * (1.0f - y_norm);

    Vec3 p_body = p;
    p_body.x /= (0.64f * width_factor);
    p_body.y /= 0.58f;
    p_body.z /= (0.52f * width_factor);
    float d_body = (p_body.length() - 1.0f) * 0.50f;

    // Cheeks bulge slightly forward & outward
    float d_lcheek = distSphere(p - Vec3(-0.36f, -0.06f, 0.10f), Vec3(0, 0, 0), 0.28f);
    float d_rcheek = distSphere(p - Vec3(0.36f, -0.06f, 0.10f), Vec3(0, 0, 0), 0.28f);
    float d_head = smin(d_body, smin(d_lcheek, d_rcheek, 0.15f), 0.12f);

    // 2. Long bunny ears on top
    float d_lear = distCapsule(p, Vec3(-0.16f, 0.38f, 0.0f), Vec3(-0.18f, 1.22f, -0.02f), 0.095f);
    float d_rear = distCapsule(p, Vec3(0.16f, 0.38f, 0.0f), Vec3(0.18f, 1.22f, -0.02f), 0.095f);
    float d_ears = std::min(d_lear, d_rear);

    // 3. Stubby cute arms pointing slightly outwards and down
    float d_larm = distCapsule(p, Vec3(-0.48f, -0.18f, 0.05f), Vec3(-0.66f, -0.32f, 0.10f), 0.055f);
    float d_rarm = distCapsule(p, Vec3(0.48f, -0.18f, 0.05f), Vec3(0.66f, -0.32f, 0.10f), 0.055f);
    float d_arms = std::min(d_larm, d_rarm);

    // 4. Stubby little feet at the bottom
    float d_lfoot = distCapsule(p, Vec3(-0.20f, -0.48f, 0.0f), Vec3(-0.22f, -0.68f, 0.05f), 0.060f);
    float d_rfoot = distCapsule(p, Vec3(0.20f, -0.48f, 0.0f), Vec3(0.22f, -0.68f, 0.05f), 0.060f);
    float d_feet = std::min(d_lfoot, d_rfoot);

    // 5. Fluffy round tail on the back
    float d_tail = distSphere(p, Vec3(0.0f, -0.32f, -0.50f), 0.14f);

    // Smooth organic blend
    float d = smin(d_head, d_ears, 0.06f);
    d = smin(d, d_arms, 0.05f);
    d = smin(d, d_feet, 0.05f);
    d = smin(d, d_tail, 0.06f);

    return d;
}

Vec3 calcNormal(Vec3 p) {
    const float eps = 0.002f;
    float d = usagiSDF(p);
    return Vec3(
        usagiSDF(p + Vec3(eps, 0, 0)) - d,
        usagiSDF(p + Vec3(0, eps, 0)) - d,
        usagiSDF(p + Vec3(0, 0, eps)) - d
    ).normalize();
}

// -----------------------------------------------------------------------------
// Shading & Character Surface Material
// -----------------------------------------------------------------------------

enum CharacterTheme {
    THEME_USAGI = 0,
    THEME_SPIDERMAN = 1,
    THEME_PARTY = 2,
    THEME_COUNT = 3
};

enum RenderMode {
    MODE_HALF_BLOCK = 0,
    MODE_ASCII_COLOR = 1,
    MODE_ASCII_MONO = 2,
    MODE_COUNT = 3
};

// Shading model for Classic Usagi
Color shadeClassicUsagi(Vec3 p_obj, Vec3 normal, Vec3 viewDir) {
    // Chiikawa authentic palette
    const Color furColor(254, 238, 175);     // Creamy butter yellow
    const Color earInnerPink(252, 182, 198); // Soft pastel pink
    const Color blushPink(252, 158, 180);    // Cheek blush
    const Color darkOutline(26, 20, 22);     // Deep brown-black lineart
    const Color whiteHighlight(255, 255, 255);
    const Color mouthPink(238, 108, 134);    // Inside open mouth
    const Color tailWhite(255, 254, 246);    // Bunny tail

    Color col = furColor;

    // Tail check (at the back)
    if (p_obj.z < -0.35f && p_obj.y < -0.15f) {
        if (distSphere(p_obj, Vec3(0.0f, -0.32f, -0.50f), 0.14f) < 0.04f) {
            col = tailWhite;
        }
    }

    // Ear interior (pink vertical oblong patch on front of each ear)
    if (p_obj.y > 0.45f && p_obj.z > -0.04f) {
        float dL = distCapsule(p_obj, Vec3(-0.16f, 0.48f, 0.02f), Vec3(-0.18f, 1.12f, 0.01f), 0.0f);
        float dR = distCapsule(p_obj, Vec3(0.16f, 0.48f, 0.02f), Vec3(0.18f, 1.12f, 0.01f), 0.0f);
        if (dL < 0.042f || dR < 0.042f) {
            col = earInnerPink;
        }
    }

    // Face features (facing front, z > 0.15)
    if (p_obj.z > 0.15f) {
        float x = p_obj.x;
        float y = p_obj.y;

        // 1. Big shiny anime eyes with double specular reflections
        for (float sign : {-1.0f, 1.0f}) {
            float eyeX = sign * 0.22f;
            float eyeY = 0.04f;
            float dx = (x - eyeX) / 0.068f;
            float dy = (y - eyeY) / 0.084f;
            if (dx * dx + dy * dy < 1.0f) {
                col = darkOutline;

                // Main upper-inner highlight
                float h1_x = (x - (eyeX - sign * 0.018f)) / 0.026f;
                float h1_y = (y - (eyeY + 0.028f)) / 0.028f;
                if (h1_x * h1_x + h1_y * h1_y < 1.0f) {
                    col = whiteHighlight;
                }

                // Secondary lower-outer highlight
                float h2_x = (x - (eyeX + sign * 0.020f)) / 0.016f;
                float h2_y = (y - (eyeY - 0.028f)) / 0.016f;
                if (h2_x * h2_x + h2_y * h2_y < 1.0f) {
                    col = whiteHighlight;
                }
            }
        }

        // 2. Rosy cheeks with 3 diagonal tick marks (///)
        for (float sign : {-1.0f, 1.0f}) {
            float chkX = sign * 0.42f;
            float chkY = -0.10f;
            float dx = (x - chkX) / 0.125f;
            float dy = (y - chkY) / 0.070f;
            if (dx * dx + dy * dy < 1.0f) {
                col = blushPink;

                // 3 diagonal slashes
                float u = (x - chkX) * 0.82f - (y - chkY) * 0.57f;
                float t1 = std::abs(u - (-0.038f));
                float t2 = std::abs(u - 0.0f);
                float t3 = std::abs(u - 0.038f);
                if (std::min({t1, t2, t3}) < 0.0075f) {
                    col = darkOutline;
                }
            }
        }

        // 3. Open smiling mouth with pink tongue
        float mx = x;
        float my = y - (-0.08f);
        float topLip = -0.028f + 0.55f * mx * mx;
        float botLip = -0.095f - 0.50f * mx * mx;

        if (mx > -0.075f && mx < 0.075f && my < topLip && my > botLip) {
            if (my < (topLip + botLip) * 0.5f + 0.01f) {
                col = mouthPink; // tongue
            } else {
                col = darkOutline;
            }
        }

        // Mouth outline
        if (std::abs(mx) < 0.080f) {
            float dTop = std::abs(my - topLip);
            float dBot = std::abs(my - botLip);
            if ((dTop < 0.011f || dBot < 0.011f) && my <= topLip + 0.005f && my >= botLip - 0.005f) {
                col = darkOutline;
            }
        }

        // Tiny cute chin crease
        if (std::abs(mx) < 0.024f && std::abs(y - (-0.21f)) < 0.007f) {
            col = darkOutline;
        }
    }

    // Cell-shading / Toon lighting
    Vec3 lightDir = Vec3(0.4f, 0.75f, 0.55f).normalize();
    float NdotL = clamp(normal.dot(lightDir), 0.0f, 1.0f);

    float shade = 1.0f;
    if (NdotL < 0.28f) shade = 0.78f;
    else if (NdotL < 0.68f) shade = 0.90f;
    else shade = 1.05f;

    // Anime silhouette contour
    float NdotV = std::abs(normal.dot(viewDir));
    if (NdotV < 0.28f) {
        col = darkOutline;
    } else {
        col = col.scale(shade);
    }

    return col;
}

// Shading model for Spider-Usagi (Spider-Man Suit)
Color shadeSpiderUsagi(Vec3 p_obj, Vec3 normal, Vec3 viewDir) {
    const Color spideyRed(215, 26, 36);      // Spider-Man scarlet red
    const Color spideyBlue(22, 68, 145);     // Spider-Man cobalt blue
    const Color darkOutline(18, 18, 22);     // Webbing & contours
    const Color eyeWhite(255, 255, 255);     // Spider mask lens

    // Base suit division: Red chest/head/ears, Blue sides/back
    Color col = spideyRed;

    // Blue flanks and underarms
    float sideFactor = std::abs(p_obj.x) / 0.55f;
    if (sideFactor > 0.72f && p_obj.y < 0.15f && p_obj.z < 0.10f) {
        col = spideyBlue;
    }
    // Blue lower torso / back
    if (p_obj.y < -0.30f && p_obj.z < -0.05f) {
        col = spideyBlue;
    }

    // Spider-Man Webbing Lines across the red suit
    if (col.r > 150) { // On red parts of suit
        // Concentric web rings expanding from face center (0, -0.02, 0.4)
        float r_web = std::sqrt(p_obj.x * p_obj.x * 1.2f + (p_obj.y + 0.02f) * (p_obj.y + 0.02f));
        float ring = std::sin(r_web * 32.0f);

        // Radial web spokes
        float angle = std::atan2(p_obj.y + 0.02f, p_obj.x);
        float spoke = std::sin(angle * 10.0f);

        if ((ring > 0.88f && r_web > 0.08f) || (std::abs(spoke) > 0.96f && r_web > 0.05f)) {
            col = darkOutline;
        }
    }

    // Spider-Man Mask Eyes (slanted angular white lenses with thick black borders)
    if (p_obj.z > 0.15f) {
        float x = p_obj.x;
        float y = p_obj.y;

        for (float sign : {-1.0f, 1.0f}) {
            float ex = (x - sign * 0.22f) * sign; // Local eye coordinate
            float ey = y - 0.04f;

            // Angular Spider-Man lens boundary
            if (ex > -0.09f && ex < 0.14f && ey > -0.09f && ey < 0.11f) {
                float dUpper = ey - (0.02f + 0.52f * ex);
                float dLower = ey - (-0.06f + 0.38f * ex * ex);

                if (dUpper < 0.025f && dLower > -0.025f) {
                    // Inside lens boundary: check if on thick black rim or white lens
                    bool isRim = (dUpper > 0.005f || dLower < -0.005f || ex < -0.07f || ex > 0.12f);
                    col = isRim ? darkOutline : eyeWhite;
                }
            }
        }

        // Chest Spider Emblem (around y = -0.28, z > 0.20)
        if (p_obj.y < -0.20f && p_obj.y > -0.38f && std::abs(x) < 0.12f) {
            float bodyDist = (x * x) / 0.0008f + ((y + 0.28f) * (y + 0.28f)) / 0.0022f;
            if (bodyDist < 1.0f) {
                col = darkOutline; // Spider body
            }
            // Spider legs
            float leg1 = std::abs((y + 0.26f) - 0.45f * std::abs(x));
            float leg2 = std::abs((y + 0.31f) + 0.45f * std::abs(x));
            if ((leg1 < 0.008f || leg2 < 0.008f) && std::abs(x) > 0.025f && std::abs(x) < 0.10f) {
                col = darkOutline;
            }
        }
    }

    // Lighting
    Vec3 lightDir = Vec3(0.4f, 0.75f, 0.55f).normalize();
    float NdotL = clamp(normal.dot(lightDir), 0.0f, 1.0f);
    float shade = 0.75f + 0.35f * NdotL;

    // Dark contour
    float NdotV = std::abs(normal.dot(viewDir));
    if (NdotV < 0.26f) {
        col = darkOutline;
    } else {
        col = col.scale(shade);
    }

    return col;
}

// Shading model for Rainbow Disco Party Usagi
Color shadePartyUsagi(Vec3 p_obj, Vec3 normal, Vec3 viewDir, float animTime) {
    // Pulsing psychedelic chromatic shifting
    float t = animTime * 2.5f + p_obj.y * 3.0f + p_obj.x * 2.0f;
    int r = int(127.0f + 127.0f * std::sin(t));
    int g = int(127.0f + 127.0f * std::sin(t + 2.094f));
    int b = int(127.0f + 127.0f * std::sin(t + 4.188f));
    Color col(r, g, b);

    // Keep Usagi's face features visible
    if (p_obj.z > 0.15f) {
        float x = p_obj.x;
        float y = p_obj.y;
        for (float sign : {-1.0f, 1.0f}) {
            float dx = (x - sign * 0.22f) / 0.068f;
            float dy = (y - 0.04f) / 0.084f;
            if (dx * dx + dy * dy < 1.0f) {
                col = Color(20, 20, 25);
            }
        }
    }

    float NdotV = std::abs(normal.dot(viewDir));
    if (NdotV < 0.26f) return Color(20, 20, 25);
    return col;
}

// -----------------------------------------------------------------------------
// Terminal Renderer
// -----------------------------------------------------------------------------

struct PixelSample {
    bool hit;
    Color color;
    float depth;
    float luminance;
};

class UsagiRenderer {
public:
    UsagiRenderer() {
        yaw = 0.0f;
        pitch = 0.08f;
        roll = 0.0f;
        spinSpeed = 1.6f; // Radians per second
        autoSpin = true;
        zoom = 1.0f;
        theme = THEME_USAGI;
        mode = MODE_HALF_BLOCK;
        showHud = true;
        shoutTimer = 0.0f;
        shoutIndex = 0;
        lastTime = std::chrono::steady_clock::now();
    }

    void handleInput() {
        char ch;
        while (read(STDIN_FILENO, &ch, 1) > 0) {
            if (ch == 'q' || ch == 'Q' || ch == 27) { // q, Q, or ESC
                // Check if it's an ANSI arrow key sequence: ESC [ A/B/C/D
                if (ch == 27) {
                    char seq[2];
                    if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                        if (seq[0] == '[') {
                            if (seq[1] == 'A') pitch = std::min(pitch + 0.15f, 1.2f);       // Up
                            else if (seq[1] == 'B') pitch = std::max(pitch - 0.15f, -1.2f); // Down
                            else if (seq[1] == 'C') yaw -= 0.20f;                           // Right
                            else if (seq[1] == 'D') yaw += 0.20f;                           // Left
                        }
                        continue;
                    } else {
                        g_running = 0;
                        return;
                    }
                }
                g_running = 0;
                return;
            } else if (ch == 3) { // Ctrl+C
                g_running = 0;
                return;
            } else if (ch == ' ') { // Toggle pause
                autoSpin = !autoSpin;
            } else if (ch == 's' || ch == 'S') { // Toggle Spider-Man
                theme = (theme == THEME_SPIDERMAN) ? THEME_USAGI : THEME_SPIDERMAN;
            } else if (ch == 'c' || ch == 'C') { // Cycle theme
                theme = static_cast<CharacterTheme>((theme + 1) % THEME_COUNT);
            } else if (ch == 'm' || ch == 'M') { // Cycle render mode
                mode = static_cast<RenderMode>((mode + 1) % MODE_COUNT);
            } else if (ch == 'a' || ch == 'A') { // Spin faster left / manual turn
                if (autoSpin) spinSpeed = std::max(-6.0f, spinSpeed - 0.5f);
                else yaw -= 0.15f;
            } else if (ch == 'd' || ch == 'D') { // Spin faster right / manual turn
                if (autoSpin) spinSpeed = std::min(6.0f, spinSpeed + 0.5f);
                else yaw += 0.15f;
            } else if (ch == 'w' || ch == 'W') { // Pitch up
                pitch = std::min(pitch + 0.15f, 1.2f);
            } else if (ch == 'x' || ch == 'X') { // Pitch down
                pitch = std::max(pitch - 0.15f, -1.2f);
            } else if (ch == '+' || ch == '=') { // Zoom in
                zoom = std::min(zoom * 1.12f, 2.8f);
            } else if (ch == '-' || ch == '_') { // Zoom out
                zoom = std::max(zoom * 0.88f, 0.45f);
            } else if (ch == 'r' || ch == 'R') { // Reset
                yaw = 0.0f;
                pitch = 0.08f;
                roll = 0.0f;
                spinSpeed = 1.6f;
                zoom = 1.0f;
            } else if (ch == 'h' || ch == 'H') { // Toggle HUD
                showHud = !showHud;
            } else if (ch == 'y' || ch == 'Y') { // Battle Cry!
                triggerShout();
            }
        }
    }

    void triggerShout() {
        shoutTimer = 2.8f; // Display shout for 2.8 seconds
        shoutIndex = (shoutIndex + 1) % 6;
    }

    void update() {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        totalTime += dt;

        if (shoutTimer > 0.0f) {
            shoutTimer -= dt;
        }

        if (autoSpin) {
            yaw += spinSpeed * dt;
            if (yaw > 2.0f * M_PI) yaw -= 2.0f * M_PI;
            if (yaw < -2.0f * M_PI) yaw += 2.0f * M_PI;
            // Cute gentle bobbing
            roll = 0.06f * std::sin(totalTime * 3.5f);
        }
    }

    void render() {
        // Query terminal dimensions
        struct winsize ws;
        int termW = 80;
        int termH = 24;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
            termW = ws.ws_col;
            termH = ws.ws_row;
        }

        // Responsive layout based on available terminal height
        int headerLines = 0;
        int footerLines = 0;
        if (showHud) {
            if (termH <= 9) {
                headerLines = 1;
                footerLines = 0;
            } else if (termH <= 14) {
                headerLines = 1;
                footerLines = 1;
            } else {
                headerLines = 2;
                footerLines = 2;
            }
        }
        int viewH = std::max(4, termH - headerLines - footerLines);
        int viewW = std::max(16, termW);

        // Precompute camera & rotation matrices
        Mat3 rotMat = Mat3::rotation(yaw, pitch, roll);
        Mat3 invRot = rotMat.transpose();

        Vec3 ro(0.0f, 0.26f, 2.40f / zoom);
        Vec3 lightDir = Vec3(0.4f, 0.75f, 0.55f).normalize();

        float fovY = 0.44f / zoom;
        float step = fovY / (float)viewH;

        std::string frameBuffer;
        frameBuffer.reserve(viewW * viewH * 32);

        // Cursor home (flicker-free rewrite)
        frameBuffer += "\033[H";

        // Header Title Bar
        if (showHud) {
            if (headerLines == 1) {
                std::string header;
                if (shoutTimer > 0.0f) {
                    header = (theme == THEME_SPIDERMAN) ?
                        " 🕷️ \"WITH GREAT POWER COMES GREAT YAHAHA!!\" 🕸️" :
                        " 🐰 \"YAHAHA!! URA URA URA!! PULLYA!! (o・∇・o)\" ★";
                } else {
                    if (theme == THEME_USAGI) {
                        header = " ★ 3D USAGI ★ [s] Spidey  [c] Theme  [m] Mode  [Space] Pause  [y] Shout  [q] Quit";
                    } else if (theme == THEME_SPIDERMAN) {
                        header = " 🕸️ SPIDER-USAGI 🕸️ [s] Usagi  [c] Theme  [m] Mode  [Space] Pause  [y] Shout  [q] Quit";
                    } else {
                        header = " 🌈 DISCO USAGI 🌈 [s] Usagi  [c] Theme  [m] Mode  [Space] Pause  [y] Shout  [q] Quit";
                    }
                }
                if ((int)header.size() < termW) header.append(termW - header.size(), ' ');
                frameBuffer += "\033[1;97;48;5;235m" + header.substr(0, termW) + "\033[0m\n";
            } else if (headerLines == 2) {
                std::string title;
                if (theme == THEME_USAGI) {
                    title = " ★ 3D SPINNING USAGI ★  (Oo-sah-gee) - Chiikawa 3D Terminal";
                } else if (theme == THEME_SPIDERMAN) {
                    title = " 🕸️ 3D SPINNING SPIDER-USAGI 🕸️  (Friendly Neighborhood Usagi!)";
                } else {
                    title = " 🌈 3D DISCO PARTY USAGI 🌈  (Ultra Yahaha Extravaganza!)";
                }
                if ((int)title.size() < termW) title.append(termW - title.size(), ' ');
                frameBuffer += "\033[1;97;48;5;235m" + title.substr(0, termW) + "\033[0m\n";

                if (shoutTimer > 0.0f) {
                    const std::string spideyShouts[] = {
                        " 🕷️ \"WITH GREAT POWER COMES GREAT YAHAHA!! (o・∇・o)\" ",
                        " 🕷️ \"FRIENDLY NEIGHBORHOOD SPIDER-USAGI ON PATROL!\" ",
                        " 🕷️ \"MY SPIDEY-SENSE IS TINGLING!! URA URA URA!!\" ",
                        " 🕷️ \"WEB-SHOOTER PULLYA BLAST!! 🕸️🕸️🕸️\" ",
                        " 🕷️ \"NOT ALL HEROES WEAR CAPES, SOME HAVE BUNNY EARS!\" ",
                        " 🕷️ \"SPIDER-USAGI INTO THE SPIDER-VERSE!!\" "
                    };
                    const std::string usagiShouts[] = {
                        " 🐰 \"YAHAHA!! YAHAHA!! (o・∇・o)\" ",
                        " 🐰 \"URA URA URA URA URA!! ⚡\" ",
                        " 🐰 \"PULLYA HA HA HA HA!! 🥕\" ",
                        " 🐰 \"FUUUTTO!! USAGI INFINITE SPIN!!\" ",
                        " 🐰 \"YAH! HAAA! EEE! (o≧▽≦)o\" ",
                        " 🐰 \"KUSHO!! MAKASERO!! YAHAHA!!\" "
                    };
                    std::string shoutText = (theme == THEME_SPIDERMAN) ? spideyShouts[shoutIndex] : usagiShouts[shoutIndex];
                    int pad = std::max(0, (termW - (int)shoutText.size()) / 2);
                    std::string shoutLine = std::string(pad, ' ') + shoutText;
                    if ((int)shoutLine.size() < termW) shoutLine.append(termW - shoutLine.size(), ' ');
                    frameBuffer += "\033[1;93;41m" + shoutLine.substr(0, termW) + "\033[0m\n";
                } else {
                    std::string sub = " Press [s] Spider-Man  [c] Theme  [m] Mode  [Space] Pause  [y] Yahaha!  [q] Quit";
                    if ((int)sub.size() < termW) sub.append(termW - sub.size(), ' ');
                    frameBuffer += "\033[38;5;246;48;5;234m" + sub.substr(0, termW) + "\033[0m\n";
                }
            }
        }

        // Raymarching lambda
        auto sampleRay = [&](float u, float v) -> PixelSample {
            PixelSample res;
            res.hit = false;
            res.depth = 1e9f;
            res.luminance = 0.0f;
            res.color = Color(0, 0, 0);

            // Ray direction
            Vec3 rd = Vec3(u, v, -1.0f).normalize();

            // Transform ray origin and direction into local object space
            Vec3 ro_obj = invRot.mul(ro);
            Vec3 rd_obj = invRot.mul(rd);

            // Generous bounding sphere check: center (0, 0.26, 0), radius 1.70
            Vec3 oc = ro_obj - Vec3(0, 0.26f, 0);
            float b = oc.dot(rd_obj);
            float c = oc.lengthSq() - 1.70f * 1.70f;
            if (b > 0.0f && c > 0.0f) return res;

            float t = 0.0f;
            const int maxSteps = 42;
            for (int s = 0; s < maxSteps; ++s) {
                Vec3 p = ro_obj + rd_obj * t;
                float d = usagiSDF(p);
                if (d < 0.0035f) {
                    res.hit = true;
                    res.depth = t;
                    Vec3 n_obj = calcNormal(p);
                    Vec3 n_world = rotMat.mul(n_obj);
                    Vec3 v_world = rd * -1.0f;

                    // Calculate color based on theme
                    if (theme == THEME_USAGI) {
                        res.color = shadeClassicUsagi(p, n_world, v_world);
                    } else if (theme == THEME_SPIDERMAN) {
                        res.color = shadeSpiderUsagi(p, n_world, v_world);
                    } else {
                        res.color = shadePartyUsagi(p, n_world, v_world, totalTime);
                    }

                    // Luminance calculation for ASCII ramps
                    float diff = clamp(n_world.dot(lightDir), 0.0f, 1.0f);
                    float rim = 1.0f - clamp(n_world.dot(v_world), 0.0f, 1.0f);
                    res.luminance = clamp(0.20f + 0.65f * diff + 0.35f * rim * rim, 0.0f, 1.0f);
                    return res;
                }
                t += d;
                if (t > 7.0f) break;
            }
            return res;
        };

        // Render viewport rows
        if (mode == MODE_HALF_BLOCK) {
            // Half-block mode: Each char row has 2 vertical subpixels
            for (int y = 0; y < viewH; ++y) {
                int lastFgR = -1, lastFgG = -1, lastFgB = -1;
                int lastBgR = -1, lastBgG = -1, lastBgB = -1;
                bool bgSet = false;

                float v_top = ((viewH * 2.0f - 1.0f) * 0.5f - (y * 2.0f)) * step;
                float v_bot = ((viewH * 2.0f - 1.0f) * 0.5f - (y * 2.0f + 1.0f)) * step;

                for (int x = 0; x < viewW; ++x) {
                    float u = (x - (viewW - 1) * 0.5f) * step;

                    PixelSample topSample = sampleRay(u, v_top);
                    PixelSample botSample = sampleRay(u, v_bot);

                    if (!topSample.hit && !botSample.hit) {
                        if (bgSet || lastFgR != -1) {
                            frameBuffer += "\033[0m";
                            lastFgR = lastFgG = lastFgB = -1;
                            lastBgR = lastBgG = lastBgB = -1;
                            bgSet = false;
                        }
                        frameBuffer += ' ';
                    } else if (topSample.hit && botSample.hit) {
                        Color fg = topSample.color;
                        Color bg = botSample.color;
                        if (fg.r != lastFgR || fg.g != lastFgG || fg.b != lastFgB) {
                            frameBuffer += "\033[38;2;" + std::to_string(fg.r) + ";" +
                                           std::to_string(fg.g) + ";" + std::to_string(fg.b) + "m";
                            lastFgR = fg.r; lastFgG = fg.g; lastFgB = fg.b;
                        }
                        if (bg.r != lastBgR || bg.g != lastBgG || bg.b != lastBgB) {
                            frameBuffer += "\033[48;2;" + std::to_string(bg.r) + ";" +
                                           std::to_string(bg.g) + ";" + std::to_string(bg.b) + "m";
                            lastBgR = bg.r; lastBgG = bg.g; lastBgB = bg.b;
                            bgSet = true;
                        }
                        frameBuffer += "\xE2\x96\x80"; // UTF-8 for '▀'
                    } else if (topSample.hit && !botSample.hit) {
                        Color fg = topSample.color;
                        if (bgSet) {
                            frameBuffer += "\033[49m"; // Reset background
                            bgSet = false;
                            lastBgR = lastBgG = lastBgB = -1;
                        }
                        if (fg.r != lastFgR || fg.g != lastFgG || fg.b != lastFgB) {
                            frameBuffer += "\033[38;2;" + std::to_string(fg.r) + ";" +
                                           std::to_string(fg.g) + ";" + std::to_string(fg.b) + "m";
                            lastFgR = fg.r; lastFgG = fg.g; lastFgB = fg.b;
                        }
                        frameBuffer += "\xE2\x96\x80"; // '▀'
                    } else { // !topSample.hit && botSample.hit
                        Color fg = botSample.color;
                        if (bgSet) {
                            frameBuffer += "\033[49m";
                            bgSet = false;
                            lastBgR = lastBgG = lastBgB = -1;
                        }
                        if (fg.r != lastFgR || fg.g != lastFgG || fg.b != lastFgB) {
                            frameBuffer += "\033[38;2;" + std::to_string(fg.r) + ";" +
                                           std::to_string(fg.g) + ";" + std::to_string(fg.b) + "m";
                            lastFgR = fg.r; lastFgG = fg.g; lastFgB = fg.b;
                        }
                        frameBuffer += "\xE2\x96\x84"; // UTF-8 for '▄'
                    }
                }
                if (y + 1 < viewH || footerLines > 0) {
                    frameBuffer += "\033[0m\n";
                } else {
                    frameBuffer += "\033[0m";
                }
            }
        } else {
            // ASCII Ramp modes (Colored or Monochrome)
            const std::string asciiRamp = " .:-=+*#%@";
            const int rampLen = (int)asciiRamp.size();

            for (int y = 0; y < viewH; ++y) {
                int lastFgR = -1, lastFgG = -1, lastFgB = -1;
                float v = ((viewH - 1) * 0.5f - y) * (2.0f * step);

                for (int x = 0; x < viewW; ++x) {
                    float u = (x - (viewW - 1) * 0.5f) * step;

                    PixelSample s = sampleRay(u, v);
                    if (!s.hit) {
                        if (lastFgR != -1) {
                            frameBuffer += "\033[0m";
                            lastFgR = lastFgG = lastFgB = -1;
                        }
                        frameBuffer += ' ';
                    } else {
                        int charIdx = std::clamp(int(s.luminance * (rampLen - 1)), 0, rampLen - 1);
                        char asciiChar = asciiRamp[charIdx];

                        if (mode == MODE_ASCII_COLOR) {
                            Color fg = s.color;
                            if (fg.r != lastFgR || fg.g != lastFgG || fg.b != lastFgB) {
                                frameBuffer += "\033[38;2;" + std::to_string(fg.r) + ";" +
                                               std::to_string(fg.g) + ";" + std::to_string(fg.b) + "m";
                                lastFgR = fg.r; lastFgG = fg.g; lastFgB = fg.b;
                            }
                        } else {
                            // Monochrome
                            if (lastFgR != 255) {
                                frameBuffer += "\033[38;5;255m";
                                lastFgR = lastFgG = lastFgB = 255;
                            }
                        }
                        frameBuffer += asciiChar;
                    }
                }
                if (y + 1 < viewH || footerLines > 0) {
                    frameBuffer += "\033[0m\n";
                } else {
                    frameBuffer += "\033[0m";
                }
            }
        }

        // Footer Dashboard Status Bar
        if (showHud && footerLines > 0) {
            std::string themeName = (theme == THEME_USAGI) ? "Usagi" :
                                    (theme == THEME_SPIDERMAN) ? "Spider-Usagi" : "Disco";
            std::string modeName = (mode == MODE_HALF_BLOCK) ? "TrueColor HD" :
                                   (mode == MODE_ASCII_COLOR) ? "Color ASCII" : "Mono ASCII";
            std::string statusState = autoSpin ? "SPIN" : "PAUSE";

            char hudBuf[256];
            int degYaw = int(yaw * 180.0f / M_PI) % 360;
            if (degYaw < 0) degYaw += 360;

            if (footerLines == 1) {
                snprintf(hudBuf, sizeof(hudBuf),
                         " [%s] Theme: %s | Mode: %s | Ang: %3d° | Spd: %+.1f | [A/D] Turn [W/X] Tilt",
                         statusState.c_str(), themeName.c_str(), modeName.c_str(),
                         degYaw, spinSpeed);
                std::string hudStr(hudBuf);
                if ((int)hudStr.size() < termW) hudStr.append(termW - hudStr.size(), ' ');
                frameBuffer += "\033[1;97;48;5;236m" + hudStr.substr(0, termW) + "\033[0m";
            } else {
                snprintf(hudBuf, sizeof(hudBuf),
                         " [%s] Theme: %-12s Mode: %-14s Angle: %3d° Speed: %+.1f Zoom: %.1fx",
                         autoSpin ? "SPINNING" : "PAUSED", themeName.c_str(), modeName.c_str(),
                         degYaw, spinSpeed, zoom);

                std::string hudStr(hudBuf);
                if ((int)hudStr.size() < termW) hudStr.append(termW - hudStr.size(), ' ');
                frameBuffer += "\033[1;97;48;5;236m" + hudStr.substr(0, termW) + "\033[0m\n";

                std::string controls = " [Left/Right/A/D] Rotate  [W/X] Pitch  [+/-] Zoom  [R] Reset  [H] Toggle HUD";
                if ((int)controls.size() < termW) controls.append(termW - controls.size(), ' ');
                frameBuffer += "\033[38;5;244;48;5;233m" + controls.substr(0, termW) + "\033[0m";
            }
        }

        // Single atomic write to stdout
        std::cout.write(frameBuffer.data(), frameBuffer.size());
        std::cout.flush();
    }

    void setTheme(CharacterTheme t) { theme = t; }
    void setMode(RenderMode m) { mode = m; }

private:
    float yaw;
    float pitch;
    float roll;
    float spinSpeed;
    bool autoSpin;
    float zoom;
    CharacterTheme theme;
    RenderMode mode;
    bool showHud;
    float shoutTimer;
    int shoutIndex;
    float totalTime = 0.0f;
    std::chrono::steady_clock::time_point lastTime;
};

// -----------------------------------------------------------------------------
// Program Entry Point & CLI Arguments
// -----------------------------------------------------------------------------

void printUsage(const char* progName) {
    std::cout << "3D Spinning Usagi & Spider-Usagi Terminal Animation\n\n"
              << "Usage: " << progName << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  --spiderman, -s     Launch directly into 3D Spider-Usagi mode\n"
              << "  --usagi, -u         Launch in Classic Usagi mode (default)\n"
              << "  --party, -p         Launch in Rainbow Disco Party mode\n"
              << "  --ascii, -a         Use 24-bit TrueColor ASCII shaded character mode\n"
              << "  --mono              Use classic monochrome ASCII mode (retro donut.c style)\n"
              << "  --help, -h          Display this help message and exit\n\n"
              << "Interactive Controls:\n"
              << "  [Space]       Pause / Resume auto-rotation\n"
              << "  [s]           Toggle Spider-Usagi / Classic Usagi theme\n"
              << "  [c]           Cycle through all themes\n"
              << "  [m]           Cycle render modes (TrueColor HD / Colored ASCII / Mono)\n"
              << "  [a] / [d]     Spin faster left / right (or rotate manually when paused)\n"
              << "  [w] / [s]     Tilt pitch up / down (or Arrow Up / Down)\n"
              << "  [+] / [-]     Zoom in / Zoom out\n"
              << "  [r]           Reset view & rotation\n"
              << "  [y]           Trigger battle cry (\"YAHAHA!!\", \"SPIDEY-SENSE!\")\n"
              << "  [h]           Toggle on-screen HUD\n"
              << "  [q] / [Esc]   Quit cleanly\n";
}

int main(int argc, char* argv[]) {
    CharacterTheme initialTheme = THEME_USAGI;
    RenderMode initialMode = MODE_HALF_BLOCK;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--spiderman" || arg == "-s" || arg == "--spider") {
            initialTheme = THEME_SPIDERMAN;
        } else if (arg == "--usagi" || arg == "-u") {
            initialTheme = THEME_USAGI;
        } else if (arg == "--party" || arg == "-p") {
            initialTheme = THEME_PARTY;
        } else if (arg == "--ascii" || arg == "-a") {
            initialMode = MODE_ASCII_COLOR;
        } else if (arg == "--mono") {
            initialMode = MODE_ASCII_MONO;
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    setupSignalHandlers();
    enableRawMode();

    UsagiRenderer renderer;
    renderer.setTheme(initialTheme);
    renderer.setMode(initialMode);

    // Target ~35-40 FPS for silky-smooth animation and responsive terminal input
    const std::chrono::milliseconds frameDuration(25);

    while (g_running) {
        auto frameStart = std::chrono::steady_clock::now();

        renderer.update();
        renderer.render();

        renderer.handleInput();
        if (!g_running) break;

        auto frameEnd = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart);
        if (elapsed < frameDuration) {
            std::this_thread::sleep_for(frameDuration - elapsed);
        }
    }

    disableRawMode();
    std::cout << "\n\033[1;32m★ Thank you for watching Usagi spin! Yahaha!! (o・∇・o) ★\033[0m\n\n";
    return 0;
}
