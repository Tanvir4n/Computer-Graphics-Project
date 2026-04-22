#include <GL/glut.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

float carX[5] = {-1, -0.7, -0.3, 0.2, 0.6};
float cloud[4] = {-0.9, -0.3, 0.2, 0.7};
float birdX = -1.0;
float personX = -0.8;

bool rain = false;
bool night = false;

struct Rain {
    float x;
    float y;
};

Rain drop[300];
float starX[100];
float starY[100];

// =============================================
// WINDOW TO VIEWPORT CONVERSION
// OpenGL uses [-1,1] coordinate range (viewport: 1000x700)
// We convert NDC coords to pixel coords for raster algorithms,
// then draw pixel-by-pixel using glVertex2f back in NDC
// =============================================

// NDC [-1,1] to pixel [0, windowW/H]
void ndcToPixel(float nx, float ny, int &px, int &py) {
    px = (int)((nx + 1.0f) * 0.5f * 1000);
    py = (int)((ny + 1.0f) * 0.5f * 700);
}

// Pixel back to NDC
void pixelToNDC(int px, int py, float &nx, float &ny) {
    nx = (px / 500.0f) - 1.0f;
    ny = (py / 350.0f) - 1.0f;
}

// Draw a single pixel in NDC space
void drawPixel(int px, int py) {
    float nx, ny;
    pixelToNDC(px, py, nx, ny);
    glBegin(GL_POINTS);
    glVertex2f(nx, ny);
    glEnd();
}

// =============================================
// DDA LINE ALGORITHM
// Used for: street light poles, tree trunks,
//           bird wings, rain drops
// =============================================
void ddaLine(float x1, float y1, float x2, float y2) {
    int px1, py1, px2, py2;
    ndcToPixel(x1, y1, px1, py1);
    ndcToPixel(x2, y2, px2, py2);

    int dx = px2 - px1;
    int dy = py2 - py1;
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);

    if (steps == 0) {
        drawPixel(px1, py1);
        return;
    }

    float xInc = dx / (float)steps;
    float yInc = dy / (float)steps;

    float x = px1;
    float y = py1;

    for (int i = 0; i <= steps; i++) {
        drawPixel((int)round(x), (int)round(y));
        x += xInc;
        y += yInc;
    }
}

// =============================================
// BRESENHAM'S LINE ALGORITHM
// Used for: road dashes, zebra crossing lines,
//           building window frames, car outlines
// =============================================
void bresenhamLine(float x1, float y1, float x2, float y2) {
    int px1, py1, px2, py2;
    ndcToPixel(x1, y1, px1, py1);
    ndcToPixel(x2, y2, px2, py2);

    int dx = abs(px2 - px1);
    int dy = abs(py2 - py1);
    int sx = (px1 < px2) ? 1 : -1;
    int sy = (py1 < py2) ? 1 : -1;
    int err = dx - dy;

    int x = px1, y = py1;

    while (true) {
        drawPixel(x, y);
        if (x == px2 && y == py2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
    }
}

// =============================================
// MIDPOINT CIRCLE DRAWING ALGORITHM
// Used for: wheels, lights, sun/moon,
//           tree foliage circles, cloud circles
// =============================================
void midpointCircle(float cx, float cy, float r) {
    int pcx, pcy;
    ndcToPixel(cx, cy, pcx, pcy);

    // Convert radius from NDC to pixels (use x-axis scale: 500 pixels per unit)
    int pr = (int)(r * 500);
    if (pr <= 0) pr = 1;

    int x = 0;
    int y = pr;
    int d = 1 - pr;

    // Lambda-style helper to plot all 8 octants
    auto plot8 = [&](int ox, int oy) {
        drawPixel(pcx + ox, pcy + oy);
        drawPixel(pcx - ox, pcy + oy);
        drawPixel(pcx + ox, pcy - oy);
        drawPixel(pcx - ox, pcy - oy);
        drawPixel(pcx + oy, pcy + ox);
        drawPixel(pcx - oy, pcy + ox);
        drawPixel(pcx + oy, pcy - ox);
        drawPixel(pcx - oy, pcy - ox);
    };

    plot8(x, y);

    while (x < y) {
        x++;
        if (d < 0) {
            d += 2 * x + 1;
        } else {
            y--;
            d += 2 * (x - y) + 1;
        }
        plot8(x, y);
    }
}

// =============================================
// FILLED CIRCLE (used for large solid areas like
// sun body, cloud puffs, leaf clusters)
// Uses midpoint circle scanline fill
// =============================================
void midpointCircleFilled(float cx, float cy, float r) {
    int pcx, pcy;
    ndcToPixel(cx, cy, pcx, pcy);
    int pr = (int)(r * 500);
    if (pr <= 0) { drawPixel(pcx, pcy); return; }

    int x = 0, y = pr;
    int d = 1 - pr;

    auto hline = [&](int x0, int x1, int py) {
        for (int i = x0; i <= x1; i++) drawPixel(i, py);
    };

    hline(pcx - y, pcx + y, pcy);

    while (x < y) {
        x++;
        if (d < 0) {
            d += 2 * x + 1;
        } else {
            y--;
            d += 2 * (x - y) + 1;
        }
        hline(pcx - x, pcx + x, pcy + y);
        hline(pcx - x, pcx + x, pcy - y);
        hline(pcx - y, pcx + y, pcy + x);
        hline(pcx - y, pcx + y, pcy - x);
    }
}

// =============================================
// ORIGINAL POLYGON-BASED CIRCLE (kept for
// compatibility with complex polygon bodies)
// =============================================
void circle(float cx, float cy, float r) {
    glBegin(GL_POLYGON);
    for (int i = 0; i < 360; i++) {
        float t = i * 3.1416f / 180.0f;
        glVertex2f(cx + r * cos(t), cy + r * sin(t));
    }
    glEnd();
}

// =============================================
// GRADIENT SKY
// =============================================
void gradientSky() {
    glBegin(GL_QUADS);
    if (night) glColor3f(0.02f, 0.02f, 0.15f);
    else       glColor3f(0.2f, 0.45f, 0.9f);
    glVertex2f(-1, 1); glVertex2f(1, 1);
    if (night) glColor3f(0.05f, 0.05f, 0.2f);
    else       glColor3f(0.75f, 0.85f, 1.0f);
    glVertex2f(1, -1); glVertex2f(-1, -1);
    glEnd();
}

// =============================================
// SUN — body uses midpointCircleFilled,
//        rays use DDA lines
// =============================================
void sun() {
    // Sun body: filled using Midpoint Circle Algorithm
    glColor3f(1.0f, 0.9f, 0.2f);
    glPointSize(1.5f);
    midpointCircleFilled(0.8f, 0.75f, 0.07f);

    // Sun rays: drawn using DDA Line Algorithm
    glColor3f(1.0f, 0.7f, 0.0f);
    glPointSize(1.0f);
    for (int i = 0; i < 16; i++) {
        float a = i * 22.5f * 3.1416f / 180.0f;
        float x1 = 0.8f + 0.09f * cos(a);
        float y1 = 0.75f + 0.09f * sin(a);
        float x2 = 0.8f + 0.16f * cos(a);
        float y2 = 0.75f + 0.16f * sin(a);
        ddaLine(x1, y1, x2, y2);
    }
}

// =============================================
// MOON — outline using Midpoint Circle Algorithm
// =============================================
void moon() {
    glColor3f(1.0f, 1.0f, 1.0f);
    glPointSize(2.0f);
    midpointCircleFilled(0.8f, 0.75f, 0.06f);
}

void stars() {
    glColor3f(1, 1, 1);
    glBegin(GL_POINTS);
    for (int i = 0; i < 100; i++)
        glVertex2f(starX[i], starY[i]);
    glEnd();
}

// =============================================
// CLOUD — puffs drawn with midpointCircleFilled
// =============================================
void cloudDraw(float x, float y) {
    glPointSize(1.5f);

    if (rain) glColor3f(0.45f, 0.45f, 0.45f);
    else      glColor3f(0.85f, 0.85f, 0.85f);
    midpointCircleFilled(x + 0.03f, y - 0.02f, 0.05f);
    midpointCircleFilled(x + 0.09f, y - 0.02f, 0.05f);
    midpointCircleFilled(x + 0.15f, y - 0.02f, 0.05f);

    if (rain) glColor3f(0.55f, 0.55f, 0.55f);
    else      glColor3f(1.0f, 1.0f, 1.0f);
    midpointCircleFilled(x,         y,         0.05f);
    midpointCircleFilled(x + 0.05f, y + 0.02f, 0.06f);
    midpointCircleFilled(x + 0.10f, y + 0.03f, 0.06f);
    midpointCircleFilled(x + 0.15f, y + 0.01f, 0.05f);
    midpointCircleFilled(x + 0.04f, y + 0.05f, 0.04f);
    midpointCircleFilled(x + 0.10f, y + 0.06f, 0.04f);
}

// =============================================
// BIRD — wings drawn with DDA Line Algorithm
// =============================================
void bird(float x, float y) {
    glColor3f(0, 0, 0);
    glPointSize(1.0f);

    // Left wing — DDA
    ddaLine(x,          y,          x + 0.015f, y + 0.015f);
    ddaLine(x + 0.015f, y + 0.015f, x + 0.03f,  y);

    // Right wing — DDA
    ddaLine(x + 0.03f,  y,          x + 0.045f, y + 0.015f);
    ddaLine(x + 0.045f, y + 0.015f, x + 0.06f,  y);

    // Body dot
    glPointSize(3);
    glBegin(GL_POINTS);
    glVertex2f(x + 0.03f, y);
    glEnd();
    glPointSize(1.0f);
}

// =============================================
// GRASS
// =============================================
void grass() {
    glColor3f(0.35f, 0.75f, 0.35f);
    glBegin(GL_QUADS);
    glVertex2f(-1, -0.6f); glVertex2f(1, -0.6f);
    glVertex2f(1, -0.3f);  glVertex2f(-1, -0.3f);
    glEnd();
}

// =============================================
// ROAD — body drawn with GL_QUADS,
//         dashes drawn with Bresenham's Line Algorithm
// =============================================
void road() {
    glColor3f(0.4f, 0.4f, 0.4f);
    glBegin(GL_QUADS);
    glVertex2f(-1, -0.6f); glVertex2f(1, -0.6f);
    glVertex2f(1, -0.9f);  glVertex2f(-1, -0.9f);
    glEnd();

    // Road center dashes — Bresenham's Line Algorithm
    glColor3f(1, 1, 1);
    glPointSize(3.0f);
    for (float i = -0.9f; i < 0.9f; i += 0.25f) {
        // Each dash is a thick horizontal line drawn with Bresenham
        for (int row = 0; row < 4; row++) {
            float yOff = -0.755f + row * 0.002f;
            bresenhamLine(i, yOff, i + 0.12f, yOff);
        }
    }
    glPointSize(1.0f);
}

// =============================================
// ZEBRA CROSSING — stripes via Bresenham's Line
// =============================================
void zebra() {
    glColor3f(1, 1, 1);
    glPointSize(3.0f);
    for (float y = -0.62f; y > -0.88f; y -= 0.07f) {
        for (int row = 0; row < 5; row++) {
            float yOff = y - row * 0.004f;
            bresenhamLine(0.50f, yOff, 0.70f, yOff);
        }
    }
    glPointSize(1.0f);
}

// =============================================
// BUILDING
// =============================================
void building(float x, float h) {
    if      (x < -0.8f) glColor3f(0.6f,  0.75f, 0.95f);
    else if (x < -0.6f) glColor3f(0.85f, 0.7f,  0.9f);
    else if (x <  0.3f) glColor3f(0.7f,  0.85f, 0.7f);
    else                glColor3f(0.95f, 0.75f, 0.6f);

    glBegin(GL_QUADS);
    glVertex2f(x, -0.45f); glVertex2f(x + 0.2f, -0.45f);
    glVertex2f(x + 0.2f, h); glVertex2f(x, h);
    glEnd();

    if (night) glColor3f(1.0f, 0.9f, 0.4f);
    else       glColor3f(0.25f, 0.45f, 0.75f);

    for (float yw = -0.40f; yw < h; yw += 0.1f)
        for (float iw = x + 0.03f; iw < x + 0.17f; iw += 0.06f) {
            glBegin(GL_QUADS);
            glVertex2f(iw, yw); glVertex2f(iw + 0.035f, yw);
            glVertex2f(iw + 0.035f, yw + 0.05f); glVertex2f(iw, yw + 0.05f);
            glEnd();
        }
}

// =============================================
// SCHOOL
// =============================================
void school() {
    float s = -0.75f;

    glColor3f(0.90f, 0.40f, 0.35f);
    glBegin(GL_QUADS);
    glVertex2f(-0.05f+s,-0.45f); glVertex2f(0.23f+s,-0.45f);
    glVertex2f(0.23f+s, 0.28f);  glVertex2f(-0.05f+s, 0.28f);
    glEnd();

    glColor3f(0.92f, 0.55f, 0.45f);
    glBegin(GL_QUADS);
    glVertex2f(-0.28f+s,-0.45f); glVertex2f(-0.05f+s,-0.45f);
    glVertex2f(-0.05f+s, 0.18f); glVertex2f(-0.28f+s, 0.18f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(0.23f+s,-0.45f); glVertex2f(0.46f+s,-0.45f);
    glVertex2f(0.46f+s, 0.18f); glVertex2f(0.23f+s, 0.18f);
    glEnd();

    glColor3f(0.85f, 0.85f, 0.85f);
    glBegin(GL_QUADS);
    glVertex2f(-0.30f+s, 0.18f); glVertex2f(0.48f+s, 0.18f);
    glVertex2f(0.43f+s,  0.24f); glVertex2f(-0.25f+s, 0.24f);
    glEnd();

    glColor3f(0.95f, 0.95f, 0.95f);
    glBegin(GL_QUADS);
    glVertex2f(0.03f+s,-0.45f); glVertex2f(0.15f+s,-0.45f);
    glVertex2f(0.15f+s, 0.10f); glVertex2f(0.03f+s, 0.10f);
    glEnd();

    glColor3f(0.92f, 0.92f, 0.92f);
    for (float x = 0.05f; x <= 0.13f; x += 0.04f) {
        glBegin(GL_QUADS);
        glVertex2f(x+s,-0.45f); glVertex2f(x+s+0.015f,-0.45f);
        glVertex2f(x+s+0.015f, 0.10f); glVertex2f(x+s, 0.10f);
        glEnd();
    }

    glColor3f(0.20f, 0.45f, 0.85f);
    glBegin(GL_QUADS);
    glVertex2f(0.08f+s,-0.45f); glVertex2f(0.11f+s,-0.45f);
    glVertex2f(0.11f+s,-0.20f); glVertex2f(0.08f+s,-0.20f);
    glEnd();

    if (night) glColor3f(1.0f, 0.9f, 0.4f);
    else       glColor3f(0.45f, 0.80f, 0.98f);

    for (float y = -0.02f; y <= 0.10f; y += 0.12f)
        for (float x = -0.25f; x <= 0.36f; x += 0.18f) {
            if (x > -0.02f && x < 0.20f) continue;
            glBegin(GL_QUADS);
            glVertex2f(x+s,y); glVertex2f(x+s+0.08f,y);
            glVertex2f(x+s+0.08f,y+0.08f); glVertex2f(x+s,y+0.08f);
            glEnd();
        }

    glColor3f(0.95f, 0.95f, 0.95f);
    for (float y = -0.02f; y <= 0.10f; y += 0.12f)
        for (float x = -0.25f; x <= 0.36f; x += 0.18f) {
            if (x > -0.02f && x < 0.20f) continue;
            glBegin(GL_LINE_LOOP);
            glVertex2f(x+s,y); glVertex2f(x+s+0.08f,y);
            glVertex2f(x+s+0.08f,y+0.08f); glVertex2f(x+s,y+0.08f);
            glEnd();
        }

    glColor3f(0.75f, 0.10f, 0.10f);
    glBegin(GL_QUADS);
    glVertex2f(0.02f+s,0.24f); glVertex2f(0.16f+s,0.24f);
    glVertex2f(0.16f+s,0.30f); glVertex2f(0.02f+s,0.30f);
    glEnd();

    glColor3f(1,1,1);
    glRasterPos2f(0.04f+s, 0.262f);
    char text[] = "SCHOOL";
    for (int i = 0; i < 6; i++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, text[i]);
}

// =============================================
// BANK
// =============================================
void bank() {
    float s = -0.05f;

    glColor3f(0.72f, 0.12f, 0.22f);
    glBegin(GL_QUADS);
    glVertex2f(-0.08f+s,-0.45f); glVertex2f(0.20f+s,-0.45f);
    glVertex2f(0.20f+s, 0.30f);  glVertex2f(-0.08f+s, 0.30f);
    glEnd();

    glColor3f(0.55f, 0.05f, 0.15f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.10f+s,0.30f); glVertex2f(0.22f+s,0.30f); glVertex2f(0.06f+s,0.40f);
    glEnd();

    glColor3f(0.60f, 0.08f, 0.18f);
    glBegin(GL_QUADS);
    glVertex2f(-0.10f+s,0.28f); glVertex2f(0.22f+s,0.28f);
    glVertex2f(0.20f+s, 0.30f); glVertex2f(-0.08f+s,0.30f);
    glEnd();

    glColor3f(0.90f, 0.75f, 0.80f);
    glBegin(GL_QUADS);
    glVertex2f(0.00f+s,-0.45f); glVertex2f(0.12f+s,-0.45f);
    glVertex2f(0.12f+s, 0.10f); glVertex2f(0.00f+s, 0.10f);
    glEnd();

    glColor3f(0.95f, 0.85f, 0.88f);
    for (float x = 0.01f; x <= 0.10f; x += 0.03f) {
        glBegin(GL_QUADS);
        glVertex2f(x+s,-0.45f); glVertex2f(x+s+0.015f,-0.45f);
        glVertex2f(x+s+0.015f,0.10f); glVertex2f(x+s,0.10f);
        glEnd();
    }

    glColor3f(0.35f, 0.05f, 0.10f);
    glBegin(GL_QUADS);
    glVertex2f(0.045f+s,-0.45f); glVertex2f(0.075f+s,-0.45f);
    glVertex2f(0.075f+s,-0.20f); glVertex2f(0.045f+s,-0.20f);
    glEnd();

    glColor3f(0.45f, 0.02f, 0.10f);
    glBegin(GL_QUADS);
    glVertex2f(-0.02f+s,0.22f); glVertex2f(0.14f+s,0.22f);
    glVertex2f(0.14f+s, 0.27f); glVertex2f(-0.02f+s,0.27f);
    glEnd();

    glColor3f(1,1,1);
    glRasterPos2f(0.015f+s, 0.235f);
    char text[] = "BANK";
    for (int i = 0; i < 4; i++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, text[i]);
}

// =============================================
// POLICE STATION
// =============================================
void policeStation() {
    float s = 0.55f;

    glColor3f(0.20f, 0.30f, 0.45f);
    glBegin(GL_QUADS);
    glVertex2f(-0.05f+s,-0.45f); glVertex2f(0.23f+s,-0.45f);
    glVertex2f(0.23f+s, 0.28f);  glVertex2f(-0.05f+s, 0.28f);
    glEnd();

    glColor3f(0.15f, 0.25f, 0.40f);
    glBegin(GL_QUADS);
    glVertex2f(-0.25f+s,-0.45f); glVertex2f(-0.05f+s,-0.45f);
    glVertex2f(-0.05f+s, 0.15f); glVertex2f(-0.25f+s, 0.15f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(0.23f+s,-0.45f); glVertex2f(0.43f+s,-0.45f);
    glVertex2f(0.43f+s, 0.15f); glVertex2f(0.23f+s, 0.15f);
    glEnd();

    glColor3f(0.10f, 0.15f, 0.25f);
    glBegin(GL_QUADS);
    glVertex2f(-0.27f+s,0.15f); glVertex2f(0.45f+s,0.15f);
    glVertex2f(0.40f+s, 0.22f); glVertex2f(-0.22f+s,0.22f);
    glEnd();

    glColor3f(0.25f, 0.35f, 0.55f);
    glBegin(GL_QUADS);
    glVertex2f(0.06f+s,0.28f); glVertex2f(0.12f+s,0.28f);
    glVertex2f(0.12f+s,0.45f); glVertex2f(0.06f+s,0.45f);
    glEnd();

    // Tower top light — Midpoint Circle Algorithm
    glColor3f(1, 0, 0);
    glPointSize(2.0f);
    midpointCircleFilled(0.09f+s, 0.47f, 0.015f);

    glColor3f(0.85f, 0.85f, 0.9f);
    glBegin(GL_QUADS);
    glVertex2f(0.05f+s,-0.45f); glVertex2f(0.13f+s,-0.45f);
    glVertex2f(0.13f+s, 0.08f); glVertex2f(0.05f+s, 0.08f);
    glEnd();

    glColor3f(0.10f, 0.20f, 0.45f);
    glBegin(GL_QUADS);
    glVertex2f(0.075f+s,-0.45f); glVertex2f(0.105f+s,-0.45f);
    glVertex2f(0.105f+s,-0.18f); glVertex2f(0.075f+s,-0.18f);
    glEnd();

    if (night) glColor3f(1.0f, 0.9f, 0.4f);
    else       glColor3f(0.35f, 0.70f, 0.95f);

    for (float y = -0.05f; y <= 0.07f; y += 0.12f)
        for (float x = -0.23f; x <= 0.35f; x += 0.18f) {
            if (x > -0.01f && x < 0.19f) continue;
            glBegin(GL_QUADS);
            glVertex2f(x+s,y); glVertex2f(x+s+0.07f,y);
            glVertex2f(x+s+0.07f,y+0.07f); glVertex2f(x+s,y+0.07f);
            glEnd();
        }

    glColor3f(1,1,1);
    for (float y = -0.05f; y <= 0.07f; y += 0.12f)
        for (float x = -0.23f; x <= 0.35f; x += 0.18f) {
            if (x > -0.01f && x < 0.19f) continue;
            glBegin(GL_LINE_LOOP);
            glVertex2f(x+s,y); glVertex2f(x+s+0.07f,y);
            glVertex2f(x+s+0.07f,y+0.07f); glVertex2f(x+s,y+0.07f);
            glEnd();
        }

    glColor3f(0.05f, 0.10f, 0.35f);
    glBegin(GL_QUADS);
    glVertex2f(0.02f+s,0.24f); glVertex2f(0.17f+s,0.24f);
    glVertex2f(0.17f+s,0.29f); glVertex2f(0.02f+s,0.29f);
    glEnd();

    glColor3f(1,1,1);
    glRasterPos2f(0.04f+s, 0.255f);
    char text[] = "POLICE";
    for (int i = 0; i < 6; i++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, text[i]);
}

// =============================================
// TREE — trunk: DDA Line, leaves: midpointCircleFilled
// =============================================
void tree(float x) {
    // Trunk — DDA Line Algorithm (thick trunk = 3 parallel DDA lines)
    glColor3f(0.35f, 0.18f, 0.05f);
    glPointSize(3.0f);
    ddaLine(x + 0.000f, -0.60f, x + 0.000f, -0.48f);
    ddaLine(x + 0.010f, -0.60f, x + 0.010f, -0.48f);
    ddaLine(x + 0.020f, -0.60f, x + 0.020f, -0.48f);
    ddaLine(x + 0.030f, -0.60f, x + 0.030f, -0.48f);

    // Leaf layers — Midpoint Circle Algorithm (filled)
    glPointSize(1.5f);
    glColor3f(0.1f, 0.6f, 0.2f);
    midpointCircleFilled(x + 0.015f, -0.45f, 0.06f);

    glColor3f(0.1f, 0.7f, 0.2f);
    midpointCircleFilled(x + 0.015f, -0.40f, 0.05f);

    glColor3f(0.1f, 0.8f, 0.2f);
    midpointCircleFilled(x + 0.015f, -0.35f, 0.04f);

    glPointSize(1.0f);
}

// =============================================
// CAR — wheels: midpointCircle outlines
// =============================================
void car(float x, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_POLYGON);
    glVertex2f(x,        -0.72f); glVertex2f(x+0.15f, -0.72f);
    glVertex2f(x+0.17f, -0.69f); glVertex2f(x+0.14f, -0.66f);
    glVertex2f(x+0.05f, -0.66f); glVertex2f(x+0.02f, -0.69f);
    glEnd();

    glColor3f(0.7f, 0.9f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x+0.06f,-0.66f); glVertex2f(x+0.12f,-0.66f);
    glVertex2f(x+0.11f,-0.64f); glVertex2f(x+0.07f,-0.64f);
    glEnd();

    // Front/back lights — Midpoint Circle Algorithm
    glColor3f(1,1,0.6f);
    glPointSize(2.0f);
    midpointCircleFilled(x+0.165f, -0.69f, 0.01f);

    glColor3f(1, 0.2f, 0.2f);
    midpointCircleFilled(x+0.01f, -0.69f, 0.01f);

    // Wheels — Midpoint Circle Algorithm (outline + rim)
    glColor3f(0, 0, 0);
    midpointCircle(x+0.04f, -0.73f, 0.02f);
    midpointCircle(x+0.13f, -0.73f, 0.02f);

    glColor3f(0.7f, 0.7f, 0.7f);
    midpointCircle(x+0.04f, -0.73f, 0.01f);
    midpointCircle(x+0.13f, -0.73f, 0.01f);

    glPointSize(1.0f);
}

// =============================================
// POLICE CAR — wheels: midpointCircle
// =============================================
void policeCar(float x) {
    glColor3f(0.05f, 0.05f, 0.10f);
    glBegin(GL_POLYGON);
    glVertex2f(x,       -0.58f); glVertex2f(x+0.16f,-0.58f);
    glVertex2f(x+0.18f,-0.55f); glVertex2f(x+0.15f,-0.52f);
    glVertex2f(x+0.05f,-0.52f); glVertex2f(x+0.02f,-0.55f);
    glEnd();

    glColor3f(1,1,1);
    glBegin(GL_QUADS);
    glVertex2f(x+0.05f,-0.57f); glVertex2f(x+0.13f,-0.57f);
    glVertex2f(x+0.13f,-0.53f); glVertex2f(x+0.05f,-0.53f);
    glEnd();

    glColor3f(0.6f, 0.85f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x+0.06f,-0.52f); glVertex2f(x+0.12f,-0.52f);
    glVertex2f(x+0.11f,-0.50f); glVertex2f(x+0.07f,-0.50f);
    glEnd();

    // Police light bar — Midpoint Circle Algorithm
    glColor3f(1, 0, 0);
    glPointSize(2.0f);
    midpointCircleFilled(x+0.08f, -0.49f, 0.008f);
    glColor3f(0, 0, 1);
    midpointCircleFilled(x+0.10f, -0.49f, 0.008f);

    glColor3f(1, 1, 0.7f);
    midpointCircleFilled(x+0.175f, -0.55f, 0.01f);
    glColor3f(1, 0.2f, 0.2f);
    midpointCircleFilled(x+0.01f, -0.55f, 0.01f);

    // Wheels — Midpoint Circle Algorithm
    glColor3f(0, 0, 0);
    midpointCircle(x+0.05f, -0.60f, 0.02f);
    midpointCircle(x+0.13f, -0.60f, 0.02f);
    glColor3f(0.7f, 0.7f, 0.7f);
    midpointCircle(x+0.05f, -0.60f, 0.01f);
    midpointCircle(x+0.13f, -0.60f, 0.01f);

    glPointSize(1.0f);

    glColor3f(0, 0, 0);
    glRasterPos2f(x+0.065f, -0.555f);
    char text[] = "POLICE";
    for (int i = 0; i < 6; i++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, text[i]);
}

// =============================================
// SCHOOL BUS — headlight: midpointCircleFilled
// =============================================
void schoolBus(float x) {
    glColor3f(1.0f, 0.8f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(x,      -0.60f); glVertex2f(x+0.32f,-0.60f);
    glVertex2f(x+0.32f,-0.50f); glVertex2f(x,      -0.50f);
    glEnd();
    glBegin(GL_POLYGON);
    glVertex2f(x+0.32f,-0.60f); glVertex2f(x+0.37f,-0.57f);
    glVertex2f(x+0.37f,-0.53f); glVertex2f(x+0.32f,-0.50f);
    glEnd();

    glColor3f(0.2f, 0.3f, 0.5f);
    for (float i = 0.03f; i < 0.30f; i += 0.06f) {
        glBegin(GL_QUADS);
        glVertex2f(x+i,       -0.51f); glVertex2f(x+i+0.04f,-0.51f);
        glVertex2f(x+i+0.04f,-0.55f); glVertex2f(x+i,       -0.55f);
        glEnd();
    }

    glColor3f(0.15f, 0.15f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(x+0.27f,-0.60f); glVertex2f(x+0.31f,-0.60f);
    glVertex2f(x+0.31f,-0.50f); glVertex2f(x+0.27f,-0.50f);
    glEnd();

    // Wheels — Midpoint Circle Algorithm
    glColor3f(0, 0, 0);
    glPointSize(2.0f);
    midpointCircle(x+0.08f, -0.62f, 0.022f);
    midpointCircle(x+0.28f, -0.62f, 0.022f);
    glColor3f(0.8f, 0.8f, 0.8f);
    midpointCircle(x+0.08f, -0.62f, 0.010f);
    midpointCircle(x+0.28f, -0.62f, 0.010f);

    // Headlight — Midpoint Circle Algorithm
    glColor3f(1, 1, 0.7f);
    midpointCircleFilled(x+0.36f, -0.56f, 0.01f);

    glPointSize(1.0f);

    glColor3f(0, 0, 0);
    glRasterPos2f(x+0.10f, -0.585f);
    char text[] = "SCHOOL BUS";
    for (int i = 0; i < 10; i++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, text[i]);
}

// =============================================
// TRAFFIC LIGHT — colored lights: midpointCircleFilled
// =============================================
void trafficLight() {
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(0.60f,-0.6f); glVertex2f(0.62f,-0.6f);
    glVertex2f(0.62f,-0.4f); glVertex2f(0.60f,-0.4f);
    glEnd();

    // Traffic light circles — Midpoint Circle Algorithm
    glPointSize(2.0f);
    glColor3f(1, 0, 0);
    midpointCircleFilled(0.61f, -0.42f, 0.015f);

    glColor3f(1, 1, 0);
    midpointCircleFilled(0.61f, -0.47f, 0.015f);

    glColor3f(0, 1, 0);
    midpointCircleFilled(0.61f, -0.52f, 0.015f);

    glPointSize(1.0f);
}

// =============================================
// STREET LIGHT — pole: DDA Line Algorithm,
//                bulb: midpointCircleFilled
// =============================================
void streetLight(float x) {
    // Pole — DDA Line Algorithm (doubled for thickness)
    glColor3f(0.3f, 0.3f, 0.3f);
    glPointSize(3.0f);
    ddaLine(x,         -0.6f, x,         -0.30f);
    ddaLine(x + 0.01f, -0.6f, x + 0.01f, -0.30f);
    ddaLine(x + 0.02f, -0.6f, x + 0.02f, -0.30f);

    // Arm — Bresenham's Line Algorithm
    glPointSize(2.0f);
    bresenhamLine(x + 0.02f, -0.31f, x + 0.08f, -0.31f);
    bresenhamLine(x + 0.02f, -0.30f, x + 0.08f, -0.30f);

    // Bulb — Midpoint Circle Algorithm
    if (night) glColor3f(1.0f, 0.95f, 0.6f);
    else       glColor3f(0.6f, 0.6f, 0.6f);
    midpointCircleFilled(x + 0.08f, -0.33f, 0.015f);

    glPointSize(1.0f);
}

// =============================================
// RAIN
// =============================================
void rainDraw() {
    // Rain drops — DDA Line Algorithm
    glColor3f(0.7f, 0.7f, 1.0f);
    glPointSize(1.5f);
    for (int i = 0; i < 300; i++) {
        ddaLine(drop[i].x, drop[i].y, drop[i].x + 0.02f, drop[i].y - 0.06f);
    }
    glPointSize(1.0f);
}

void rainSplash() {
    glColor3f(0.8f, 0.8f, 1.0f);
    for (int i = 0; i < 80; i++) {
        float x = (rand() % 200 - 100) / 100.0f;
        float y = -0.88f + (rand() % 10) / 100.0f;
        glBegin(GL_LINES);
        glVertex2f(x, y);     glVertex2f(x - 0.01f, y + 0.01f);
        glVertex2f(x, y);     glVertex2f(x + 0.01f, y + 0.01f);
        glEnd();
    }
}


float pedestrianX    = -0.95f; // Starting position (screen left theke suru)
float pedestrianWalk = 0.0f;   // Walk cycle angle (radian) — leg/arm swing er jonno
bool  showPedestrian = true;   // Pedestrian show/hide toggle

// pedestrian() — Stick figure manush draw kore
// x         : manusher horizontal position (NDC)
// walkAngle : leg/arm swing angle, update() theke ase
//
// Body parts (sob DDA Line Algorithm diye anka):
//   HEAD    : midpointCircleFilled (circle)
//   BODY    : vertical DDA line (spine)
//   LEFT ARM: DDA line, walkAngle diye forward-back swing
//   RIGHT ARM: DDA line, opposite direction swing
//   LEFT LEG : DDA line, walkAngle diye forward-back swing
//   RIGHT LEG: DDA line, opposite direction swing
// -----------------------------------------------
void pedestrian(float x, float walkAngle) {

    float baseY = -0.62f; // Manush er paer niche (footpath level)

    // ---- HEAD (Matha) ----
    // Midpoint Circle Algorithm diye filled circle
    glColor3f(0.95f, 0.75f, 0.55f); // Skin color
    glPointSize(1.5f);
    midpointCircleFilled(x, baseY + 0.14f, 0.022f);

    // ---- BODY (Sharir) ----
    // DDA Line Algorithm — spine er moto vertical line
    glColor3f(0.2f, 0.4f, 0.8f); // Blue shirt
    glPointSize(2.5f);
    ddaLine(x, baseY + 0.12f, x, baseY + 0.06f); // Upper body
    ddaLine(x, baseY + 0.06f, x, baseY + 0.00f); // Lower body

    // ---- ARMS (Haat) ----
    // DDA Line Algorithm diye, walkAngle use kore alternate swing
    // Left arm: sin(walkAngle) diye forward
    // Right arm: -sin(walkAngle) diye backward (opposite)
    glColor3f(0.2f, 0.4f, 0.8f); // Same shirt color
    glPointSize(2.0f);

    float armSwing = 0.04f * sin(walkAngle);

    // Left arm — DDA
    ddaLine(x,                   baseY + 0.10f,
            x - 0.025f + armSwing, baseY + 0.05f);

    // Right arm — DDA (opposite swing)
    ddaLine(x,                   baseY + 0.10f,
            x + 0.025f - armSwing, baseY + 0.05f);

    // ---- LEGS (Pa) ----
    // DDA Line Algorithm diye, walkAngle use kore alternate swing
    // Leg swing: left forward hoile right backward
    glColor3f(0.15f, 0.15f, 0.40f); // Dark pants color
    glPointSize(2.5f);

    float legSwing = 0.05f * sin(walkAngle);

    // Left leg — DDA
    ddaLine(x, baseY + 0.00f,
            x - 0.02f + legSwing, baseY - 0.05f);

    // Right leg — DDA (opposite)
    ddaLine(x, baseY + 0.00f,
            x + 0.02f - legSwing, baseY - 0.05f);

    // ---- FEET (Juta) ----
    // Small horizontal DDA lines for feet
    glColor3f(0.1f, 0.1f, 0.1f); // Black shoes
    ddaLine(x - 0.02f + legSwing,  baseY - 0.05f,
            x - 0.04f + legSwing,  baseY - 0.05f);
    ddaLine(x + 0.02f - legSwing,  baseY - 0.05f,
            x + 0.04f - legSwing,  baseY - 0.05f);

    glPointSize(1.0f);
}


// =============================================
// DISPLAY
// =============================================
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    gradientSky();
    grass();
    road();
    zebra();
    school();
    bank();
    policeStation();
    policeCar(0.75f);
    schoolBus(-0.95f);

    tree(-0.78f);
    tree(-0.53f);
    tree( 0.05f);
    tree( 0.32f);
    tree( 0.58f);

    streetLight(-0.85f);
    streetLight(-0.40f);
    streetLight( 0.10f);
    streetLight( 0.45f);

    trafficLight();

    if (night) { moon(); stars(); }
    else       { sun(); }

    if (!night) {
        bird(birdX,        0.80f);
        bird(birdX + 0.18f, 0.76f);
        bird(birdX + 0.36f, 0.82f);
        bird(birdX + 0.55f, 0.78f);
    }

    for (int i = 0; i < 4; i++)
        cloudDraw(cloud[i], 0.8f - i * 0.05f);

    for (int i = 0; i < 5; i++)
        car(carX[i], 0.2f * i, 0.4f, 1.0f - 0.2f * i);

    if (showPedestrian)
        pedestrian(pedestrianX, pedestrianWalk);


    if (rain) { rainDraw(); rainSplash(); }

    glutSwapBuffers();
}

// =============================================
// UPDATE / TIMER
// =============================================
void update(int v) {
    for (int i = 0; i < 5; i++) {
        carX[i] += 0.0007f + (i * 0.0003f);
        if (carX[i] > 1) carX[i] = -1;
    }
    for (int i = 0; i < 4; i++) {
        cloud[i] += 0.001f;
        if (cloud[i] > 1) cloud[i] = -1;
    }
    birdX += 0.002f;
    if (birdX > 1.2f) birdX = -1.2f;

    if (rain) {
        for (int i = 0; i < 300; i++) {
            drop[i].y -= 0.03f;
            drop[i].x += 0.01f;
            if (drop[i].y < -0.6f) {
                drop[i].y = 1.0f;
                drop[i].x = (rand() % 200 - 100) / 100.0f;
            }
        }
    }

    // ---- FEATURE 1 UPDATE: pedestrian er position o walk cycle update ----
    if (showPedestrian) {
        pedestrianX    += 0.0012f; // Pedestrian er speed (car er cheye slow)
        pedestrianWalk += 0.15f;   // Walk cycle angle — leg/arm swing speed
        if (pedestrianX > 1.1f)
            pedestrianX = -1.1f;   // Screen wrap — baame fire ashe
    }

    // ---- FEATURE 2 UPDATE: ambulance er position o flash update ----
    if (showAmbulance) {
        ambulanceX    += 0.004f;   // Ambulance er speed (police car theke fast)
        ambulanceFlash++;
        if (ambulanceFlash >= 8) { // Protiti 8 frame e light toggle hobe
            ambulanceFlash = 0;
            flashState     = !flashState; // Red to white, white to red
        }
        if (ambulanceX > 1.2f)
            ambulanceX = -1.2f;    // Screen wrap
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

// =============================================
// KEYBOARD
// =============================================
void keyboard(unsigned char key, int x, int y) {
    if (key == 'r') rain  = true;
    if (key == 's') rain  = false;
    if (key == 'n') night = true;
    if (key == 'd') night = false;
    if (key == 'p') showPedestrian = true;  // Pedestrian ON
    if (key == 'o') showPedestrian = false; // Pedestrian OFF

}

// =============================================
// INIT
// =============================================
void init() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-1, 1, -1, 1);

    for (int i = 0; i < 100; i++) {
        starX[i] = (rand() % 200 - 100) / 100.0f;
        starY[i] = 0.45f + (rand() % 55) / 100.0f;
    }
    for (int i = 0; i < 300; i++) {
        drop[i].x = (rand() % 200 - 100) / 100.0f;
        drop[i].y = (rand() % 200) / 100.0f;
    }
    srand(time(0));
}

// =============================================
// MAIN
// =============================================
int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(1000, 700);
    glutCreateWindow("Smart Primary School Environment");
    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, update, 0);
    glutMainLoop();
    return 0;
}