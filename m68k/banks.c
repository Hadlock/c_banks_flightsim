#include <QuickDraw.h>
#include <Events.h>
#include <Fonts.h>
#include <ToolUtils.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Sound.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*
We define our own gray pattern to avoid referencing qd.gray,
which triggers an undefined reference to `qd`.
*/
static const Pattern myGray = {
    0xAAAA, 0x5555,
    0xAAAA, 0x5555,
    0xAAAA, 0x5555,
    0xAAAA, 0x5555
};

#define c2pstr(cstr) ((StringPtr)(unsigned char *)cstr)

WindowPtr mainWin;
Rect screenRect = {50, 50, 434, 434};
GrafPtr oldPort;

#define dt 0.02
#define MAX_PTS 3000

short worldX[MAX_PTS], worldY[MAX_PTS], worldZ[MAX_PTS];
short num_pts = 0;

short prevX = 1e4, prevY = 0;
double airplaneX = 0, airplaneY = 0, airplaneZ = 1000;
double compassRadians = 0, forwardTilt = 0.0, sideTilt = 0.0;
double speed = 8;
char infoStr[32];

// Rotation matrix
float R11, R12, R13;
float R21, R22, R23;
float R31, R32, R33;

/* Simple beep for effect */
void playBeep() {
    SysBeep(10);
}

/* We use a local gray pattern to avoid referencing qd.gray. */
void fadeInWindow() {
    for (short i = 0; i < 16; ++i) {
        PenPat(&myGray);
        PaintRect(&mainWin->portRect);
        Delay(4, NULL);
    }
    EraseRect(&mainWin->portRect);
}

/* Draw a loading screen with a Wingdings airplane & instructions */
void showLoadingScreen() {
    SetPort(mainWin);
    EraseRect(&mainWin->portRect);

    fadeInWindow();
    playBeep();

    TextFont(23); // Wingdings
    TextSize(48);
    MoveTo(150, 180);
    DrawChar(81); // Airplane symbol

    TextFont(0); // Geneva
    TextSize(12);
    MoveTo(130, 220);
    DrawString("\pLoading scene files...");

    MoveTo(110, 240);
    DrawString("\pWASD = Control | +/- = Throttle | Esc = Quit");

    Delay(90, NULL); // ~1.5 seconds
}

/* Load horizon.scene & pittsburgh.scene from the working directory */
void loadSceneFiles() {
    showLoadingScreen();

    FILE *f = fopen("horizon.scene", "r");
    if (!f) ExitToShell();
    while (fscanf(f, "%hd %hd %hd", &worldX[num_pts], &worldY[num_pts], &worldZ[num_pts]) == 3) {
        if (++num_pts >= MAX_PTS) break;
    }
    fclose(f);

    f = fopen("pittsburgh.scene", "r");
    if (!f) ExitToShell();
    while (fscanf(f, "%hd %hd %hd", &worldX[num_pts], &worldY[num_pts], &worldZ[num_pts]) == 3) {
        if (++num_pts >= MAX_PTS) break;
    }
    fclose(f);
}

/* Precompute rotation matrix from sideTilt, forwardTilt, compassRadians */
void updateRotationMatrix() {
    float cx = cos(sideTilt), sx = sin(sideTilt);
    float cy = cos(forwardTilt), sy = sin(forwardTilt);
    float cz = cos(compassRadians), sz = sin(compassRadians);

    R11 = cy * cz;
    R12 = cy * sz;
    R13 = -sy;

    R21 = sx * sy * cz - cx * sz;
    R22 = sx * sy * sz + cx * cz;
    R23 = sx * cy;

    R31 = cx * sy * cz + sx * sz;
    R32 = cx * sy * sz - sx * cz;
    R33 = cx * cy;
}

/* Simple line drawing wrapper */
void drawLine(short x1, short y1, short x2, short y2) {
    MoveTo(x1, y1);
    LineTo(x2, y2);
}

/* Draw simple HUD text in bottom-left corner */
void drawHUD() {
    MoveTo(20, 420);
    DrawString(c2pstr(infoStr));
}

/* Clear the entire window’s rect */
void clearWindow() {
    EraseRect(&mainWin->portRect);
}

/* Apply camera transforms (R11..R33) and project 3D lines to 2D */
void projectAndDrawScene() {
    short i;
    short x, y;
    for (i = 0; i < num_pts; ++i) {
        float dx = worldX[i] - airplaneX;
        float dy = worldY[i] - airplaneY;
        float dz = worldZ[i] + airplaneZ;  // note: Z is negative up in classic Mac coords

        float X = R11 * dx + R12 * dy + R13 * dz;
        float Y = R21 * dx + R22 * dy + R23 * dz;
        float Z = R31 * dx + R32 * dy + R33 * dz;

        if (X == 0) continue;

        x = (short)(Y / X * 150 + 192);
        y = (short)(Z / X * 150 + 192);

        // End of object or out of view
        if (worldX[i] + worldY[i] + worldZ[i] == 0 || fabs(Y) > fabs(X) || fabs(Z) > fabs(X)) {
            prevX = 1e4;
        } else {
            if (prevX < 10000) {
                drawLine(prevX, prevY, x, y);
            }
            prevX = x;
            prevY = y;
        }
    }
    drawHUD();
}

/* Simple physics integration: move airplane forward in heading, update infoStr */
void updatePhysics() {
    airplaneX += cos(compassRadians) * speed * dt;
    airplaneY += sin(compassRadians) * speed * dt;

    sprintf(infoStr, "% 5.0f % 3.0f % 7.0f", speed * 1.7, compassRadians * 57.3, airplaneZ);
}

/* Process user input for altitude, heading, speed */
void handleKey(char c) {
    switch (c) {
        case 'w': airplaneZ += 10; break;
        case 's': airplaneZ -= 10; break;
        case 'a': compassRadians -= 0.05; break;
        case 'd': compassRadians += 0.05; break;
        case '+': speed += 1;       break;
        case '-': speed -= 1;       break;
    }
}

/* Main event & render loop */
void mainLoop() {
    EventRecord evt;
    while (1) {
        if (WaitNextEvent(everyEvent, &evt, 2, NULL)) {
            if (evt.what == keyDown || evt.what == autoKey) {
                char c = evt.message & charCodeMask;
                if (c == 0x1B) ExitToShell(); // Esc
                handleKey(c);
            }
        }
        clearWindow();
        updateRotationMatrix();
        projectAndDrawScene();
        updatePhysics();
        Delay(2, NULL); // ~ 1/30th second
    }
}

/* Main entry point, avoids referencing `qd` or `thePort` */
int main() {
    GrafPtr systemPort = NULL;
    InitGraf(&systemPort);   // Initialize QuickDraw globally
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    mainWin = NewWindow(NULL, &screenRect, "\pFlightSim", true, documentProc, (WindowPtr)-1L, false, 0);
    SetPort(mainWin);

    loadSceneFiles();
    mainLoop();
    return 0;
}