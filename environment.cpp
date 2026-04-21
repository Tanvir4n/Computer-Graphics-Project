#include <windows.h>
#include <GL/glut.h>
#include <math.h>

// ─────────────────────────────────────────────────────────
// Basic Circle (used for trees & building)
// ─────────────────────────────────────────────────────────
void circle(float cx, float cy, float r, int R, int G, int B) {
    glColor3ub(R, G, B);
    glBegin(GL_POLYGON);
    for (int i = 0; i < 100; i++) {
        float angle = 2 * 3.1416 * i / 100;
        float x = r * cos(angle);
        float y = r * sin(angle);
        glVertex2f(cx + x, cy + y);
    }
    glEnd();
}

// ─────────────────────────────────────────────────────────
// RECTANGLE
// ─────────────────────────────────────────────────────────
void rect(float x1, float y1, float x2, float y2, int R, int G, int B) {
    glColor3ub(R, G, B);
    glBegin(GL_POLYGON);
    glVertex2f(x1, y1);
    glVertex2f(x1, y2);
    glVertex2f(x2, y2);
    glVertex2f(x2, y1);
    glEnd();
}

// ─────────────────────────────────────────────────────────
// TREE
// ─────────────────────────────────────────────────────────
void circleTree(float tx, float ty) {
    glLineWidth(10);
    glBegin(GL_LINES);
    glColor3ub(153, 51, 51);
    glVertex2f(tx, ty);
    glVertex2f(tx, ty + 0.2f);
    glEnd();

    circle(tx, ty + 0.3f, 0.1f, 0, 153, 51);
}
