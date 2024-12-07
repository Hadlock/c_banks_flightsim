/* De-obfuscating the Banks flight simulator from IOCCC 1998. Not finished,
I haven’t tracked down all the math though author has mentioned he used
orthogonal matrices.

Stdin is lists of 3d point coordinates. Each list is an object
drawn connect-a-dot and terminated with 0 0 0. Z upward is negative.

Compile:

gcc -ansi banks.c -lm -lX11 -o banks

Run:

cat horizon.scene pittsburgh.scene | ./banks

Get the .scene data files from the IOCCC site.

http://www0.us.ioccc.org/years.html#1998

Controls:

Arrow keys are the flight stick.
Enter re-centers stick left-right, but not forward-back.
PageUp, PageDn = throttle

HUD on bottom-left:
speed, heading (0 = North), altitude

Math note:

Angles
There are 3 angles in the math. Your compass heading, your
front-back tilt, and your sideways tilt = Tait-Bryan
angles typical in aerospace math. Also called yaw, pitch, and roll.
The Z axis is negative upward. That’s called left-handed coordinates.
The rotation matrix assumes that.

The rotation matrix is not shown in final form in the wiki article I cite,
so let’s derive it:

cx = cos(x) and so on. x=sideTilt, y=forwardTilt, z=compass

1 0 0 cy 0 -sy cz sz 0

0 cx sx * 0 1 0 * -sz cz 0

0 -sx cx sy 0 cy 0 0 1

cy 0 -sy cz sz 0

sx*sy cx sx*cy * -sz cz 0

cx*sy -sx cx*cy 0 0 1

cy*cz cy*sz -sy

sx*sy*cz-cx*sz sx*sy*sz+cx*cz sx*cy

cx*sy*cz+sx*sz cx*sy*sz-sx*cz cx*cy


It is shown in section 2.2.1 “Euler Angles” of banks1
references

wiki1 = en.wikipedia.org/wiki/Perspective_transform#Perspective_projection
wiki2 = https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
banks1 = “A DISCUSSION OF METHODS OF REAL-TIME AIRPLANE FLIGHT SIMULATION”
http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.510.7499&rep=rep1&type=pdf

Obfuscation notes.

The original program has some tricky syntax.

Forms like –*(DN -N? N-DT ?N== RT?&u: & W:&h:&J )
are nested x ? y :z that return the address of a variable that
gets de-referenced by the * and finally decremented.

A comma like in c += (I = M / l, l * H + I * M + a * X) * _;

makes a sequence of statements. The last one is the value of the

(…) list.

*/

#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

/* Keyboard symbols we accept. Originally were -D defines on compile line. */

#define Throttle_Up XK_Page_Up
#define Throttle_Down XK_Page_Down
#define Up XK_Up
#define Down XK_Down
#define Left XK_Left
#define Right XK_Right
#define Enter XK_Return

/* Update every 0.02 seconds. */

#define dt 0.02

/* Variables renamed from original program.

_ -> timeDelta

A -> gravityAccel
a -> R32, a still used in another case.
B -> sin_forwardTilt
C -> y and integerConvert.
c -> airplaneY.
CS -> Enter
D -> worldX_rel and cos_sideTilt and Dz, D still used for other cases.
DN -> Down
DT -> Throttle_Down
E -> worldY_rel and sin_sideTilt, E still used for other cases.
H -> R12, H still used in another case.
h -> speed.
I -> R22, I still used in another case.
i -> airplaneZ.
IT -> Throttle_Up
J -> up_down.
j -> forwardTiltRadians. Airplane’s front-back tilt angle
in radians. 0 if plane is level, increases negative if dive or
positive if climb.

K -> accel and cos_forwardTilt and Dx.
L -> airplaneX.
l -> speedFeet.
LT -> Left.
m -> R11, m still used in another case.
N -> prevX, N still used for other cases.
n -> worldX.
O -> compassRadians
o -> sideTiltRadians. Airplane’s sideways tilt angle in radians.
It’s 0 if plane is level, increases positive if plane spins right
or negative if spin left. Keeps growing beyond 2pi if plane keeps
spinning.

P -> R21, P still used in another case.
p -> idx and speedKnots.
q -> x.
r -> R23.
RT -> Right
s -> worldZ.
T -> worldZ_rel and sin_compass, T still used for other cases.
t -> R31, t still used in another case.
U -> prevY.
u -> left_right.
UP -> Up
W -> cos_compass and Dy, W still used for other cases.
w -> worldY.
y -> num_pts.
Z -> R33.

*/

/* Global variables for the simulation */
double  airplaneX, 
        sideTiltRadians, 
        P, 
        timeDelta = dt, 
        T, Z, 
        D = 1, 
        d, 
        worldZ[999], 
        E, 
        speed = 8, 
        I, 
        up_down, 
        accel, 
        worldY[999], 
        M, m, 
        compassRadians;

double  worldX[999], 
        forwardTiltRadians = 33e-3, 
        airplaneZ = 1E3, 
        t, 
        left_right, 
        v, W, 
        S = 74.5, 
        speedFeet = 221, 
        X = 7.26;

double  a, 
        gravityAccel = 32.2 /*ft/sec^2*/,
        airplaneY, 
        F, H;

double  cos_compass, 
        cos_forwardTilt, 
        cos_sideTilt = 1, 
        Dx, Dy, Dz, 
        R11, R12, R13, R21, 
        R22, R23, R31, R32, 
        R33, 
        worldX_rel, 
        worldY_rel,
        worldZ_rel, 
        sin_compass, 
        sin_forwardTilt, 
        sin_sideTilt;

int     N, x, 
        integerConvert, 
        num_pts, 
        speedKnots, 
        idx;

int     prevX, 
        prevY, 
        y;

Window win;

char infoStr[52];

GC gc;

/* Function to set up X Windows */
void setupXWindows(Display **disp, Window *win, GC *gc) {
    *disp = XOpenDisplay(0);
    *win = RootWindow(*disp, 0);
    *gc = XCreateGC(*disp, *win, 0, 0);
    XSetForeground(*disp, *gc, BlackPixel(*disp, 0));
    *win = XCreateSimpleWindow(*disp, *win, 0, 0, 400, 400, 0, 0, WhitePixel(*disp, 0));
    XSelectInput(*disp, *win, KeyPressMask);
    XMapWindow(*disp, *win);
}

/* Function to load map files from stdin into arrays */
void loadMapFiles() {
    /*Load map files from stdin into arrays. */
    for (; scanf("%lf%lf%lf", worldX + num_pts, worldY + num_pts, worldZ + num_pts) + 1; num_pts++);
}

/* Function to sleep for a specified interval */
void sleepForInterval() {
    /* Sleep */

    /*dt is 0.02. timeval is in bits/time.h and gives secs and usecs.
    This becomes 0.02 * 1000000 = 20000 usecs = 0.02 seconds.
    Need it here to reset it since the select call wipes it out. */
    struct timeval sleeptime = {0, dt * 1e6};
    select(0, 0, 0, 0, &sleeptime);
}



/* Function to calculate angles and update rotation matrix */
void calculateAngles() {

    /* Function to calculate cosines and sines of angles */
    void calculateTrigonometricValues() {
        /* Angle calculations. */
        cos_forwardTilt = cos(forwardTiltRadians);
        sin_forwardTilt = sin(forwardTiltRadians);
        cos_compass = cos(compassRadians);
        cos_sideTilt = cos(sideTiltRadians);
        sin_sideTilt = sin(sideTiltRadians);
        sin_compass = sin(compassRadians);
    }

    void updateF() {
        F += timeDelta * P;
    }

    void updateCompassRadians() {
        compassRadians += cos_sideTilt * timeDelta * F / cos_forwardTilt + d / cos_forwardTilt * sin_sideTilt * timeDelta;
    }

    void updateForwardTiltRadians() {
        forwardTiltRadians += d * timeDelta * cos_sideTilt - timeDelta * F * sin_sideTilt;
    }

    void updateSideTiltRadians() {
        sideTiltRadians += (sin_sideTilt * d / cos_forwardTilt * sin_forwardTilt + v + sin_forwardTilt / cos_forwardTilt * F * cos_sideTilt) * timeDelta;
    }

    void updateRotationMatrix() {
        /* Next 9 values make up a rotation matrix for a camera transform. See wiki1. */
        R11 = cos_forwardTilt * cos_compass;
        R12 = cos_forwardTilt * sin_compass;
        R13 = -sin_forwardTilt; /* Original code didn’t put this in a variable. */

        R21 = cos_compass * sin_sideTilt * sin_forwardTilt - sin_compass * cos_sideTilt;
        R22 = cos_sideTilt * cos_compass + sin_sideTilt * sin_compass * sin_forwardTilt;
        R23 = sin_sideTilt * cos_forwardTilt;

        R31 = sin_compass * sin_sideTilt + cos_sideTilt * sin_forwardTilt * cos_compass;
        R32 = sin_forwardTilt * sin_compass * cos_sideTilt - sin_sideTilt * cos_compass;
        R33 = cos_sideTilt * cos_forwardTilt;
    }

    calculateTrigonometricValues();
    updateF();
    updateCompassRadians();
    updateForwardTiltRadians();
    updateSideTiltRadians();
    updateRotationMatrix();
}

/* Function to update the display */
void updateDisplay(Display *disp, Window win, GC gc) {
    XClearWindow(disp, win);

    /*Loop over points and draw lines.*/
    for (idx = 0, prevX = 1E4; idx < num_pts;) {
        /*The world point must be moved so the airplane is the 0,0,0 origin.
        Then the point must be rotated by all 3 angles.
        
        Finally the 3D point must be projected onto the 2D plane of the
        display. All this is the camera transform in

        en.wikipedia.org/wiki/Perspective_transform#Perspective_projection.*/

        /*Shift world object vertex x,y,z relative to airplane as origin.
        The Z line uses + because airplaneZ is upward positive. It has to
        be negated because world Z is upward negative:
        worldZ – -airplaneZ = worldZ + airplaneZ. */

        worldX_rel = worldX[idx] - airplaneX;
        worldY_rel = worldY[idx] - airplaneY;
        worldZ_rel = worldZ[idx] + airplaneZ;

        /* Apply the 3 angle rotation matrix. */

        Dx = R11 * worldX_rel + R12 * worldY_rel + R13 * worldZ_rel;
        Dy = R21 * worldX_rel + R22 * worldY_rel + R23 * worldZ_rel;
        Dz = R31 * worldX_rel + R32 * worldY_rel + R33 * worldZ_rel;

        /* Point D is now a shifted and rotated world vertex.
        We are looking along the Dx axis, I think. */

        /*0,0,0 signals end of an object. Dy or Dz larger than Dx means point
        is out of range of view (assuming a square display). */
        if (worldX[idx] + worldY[idx] + worldZ[idx] == 0 || Dx < fabs(Dy) || Dx < fabs(Dz)) {
            /* Don’t draw this point and set flag to not draw it next time
            through loop. */
            prevX = 1e4;
        } else {

            /* Project 3D point onto 2D plane to be displayed. This will
            make distant objects look smaller. The rotation has us
            looking along the Dx axis, I think. So the farther out Dx
            is, the smaller Dy and Dz become. The wiki1 article has
            Dz as the denominator. Why? Is the article wrong? This
            code is working. */

            /* Window is 400 x 400. */

            x = Dy / Dx * 4E2 + 2E2;
            y = Dz / Dx * 4E2 + 2E2;

            /* 1E4 flag prevents drawing first point since we don’t have a line
            until 2nd point is read. It also skips points that fall out of
            range of view. */

            if (prevX - 1E4)
                /* Draw line from (prevX, prevY) to (x, y).
                Flickers since we’re not using double buffering. */
                XDrawLine(disp, win, gc, prevX, prevY, x, y);

            prevX = x;
            prevY = y;
        }

        ++idx;
    }
    /*HUD. infoStr = 3 values: speed in knots, heading 0=N 90=E 180=S 270=W,
    altimeter in feet.*/
    XDrawString(disp, win, gc, 20, 380, infoStr, 17);
}

/* Function to handle key press events */
void handleKeyPress(Display *disp) {
    XEvent event;
    while (XPending(disp)) {
        /*Get key press.*/
        XNextEvent(disp, &event);
        KeySym key = XLookupKeysym(&event.xkey, 0);
        switch (key) {
            case Up:
                ++up_down;
                break;
            case Down:
                --up_down;
                break;
            case Left:
                ++left_right;
                break;
            case Right:
                --left_right;
                break;
            case Throttle_Up:
                ++speed;
                break;
            case Throttle_Down:
                --speed;
                break;
            case Enter:
                left_right = 0;
                break; /* re-center from turning */
            default:
                break;
        }
    }
}



/* Function to update the position and physics of the airplane */
void updatePhysics() {

    void updateMomentum() {
        M += H * timeDelta;
    }

    void calculateInertia() {
        I = M / speedFeet;
    }

    void updateAirplanePosition() {
        airplaneX += (R11 * speedFeet + R21 * M + R31 * X) * timeDelta;
        airplaneY += (R12 * speedFeet + I * M + R32 * X) * timeDelta;
        /* airplaneZ is positive upward, rotation matrix is negative upward Z. */
        airplaneZ += (-R13 * speedFeet - R23 * M - R33 * X) * timeDelta;
    }

    void calculateIntermediateValues() {
        m = 15 * F / speedFeet;
        E = 0.1 + X * 4.9 / speedFeet;
        T = X * X + speedFeet * speedFeet + M * M;
        t = T * m / 32 - I * T / 24;
    }

    void calculateH() {
        H = gravityAccel * R23 + v * X - F * speedFeet + t / S;
    }

    void calculateAcceleration() {
        accel = F * M + (speed * 1e4 / speedFeet - (T + E * 5 * T * E) / 3e2) / S - X * d - sin_forwardTilt * gravityAccel;
    }

    void updateSpeed() {
        speedFeet += accel * timeDelta;
        speedKnots = speedFeet / 1.7;
    }

    void updateInfoString() {
        /*infoStr = 3 values: speed in knots, heading 0=N 90=E 180=S 270=W,
        altimeter in feet.*/
        sprintf(infoStr, "% 5d % 3d % 7d", speedKnots, (int)(compassRadians * 57.3) % 360, (int)airplaneZ);
    }

    /* Function to calculate intermediate values for the next step */
    void calculateNextStepValues() {
        a = 2.63 / speedFeet * d;
        X += (d * speedFeet - T / S * (0.19 * E + a * 0.64 + up_down / 1e3) - M * v + gravityAccel * R33) * timeDelta;
        W = d;
        d += T * (0.45 - 14 / speedFeet * X - a * 130 - up_down * 0.14) * timeDelta / 125e2 + F * timeDelta * v;
        D = v / speedFeet * 15;
    }

    void calculateP() {
        P = (T * (47 * I - m * 52 + E * 94 * D - t * 0.38 + left_right * 0.21 * E) / 1e2 + W * 179 * v) / 2312;
    }

    void updateV() {
        v -= (W * F - T * (0.63 * m - I * 0.086 + m * E * 19 - D * 25 - 0.11 * left_right) / 107e2) * timeDelta;
    }


    updateMomentum();
    calculateInertia();
    updateAirplanePosition();
    calculateIntermediateValues();
    calculateH();
    calculateAcceleration();
    updateSpeed();
    updateInfoString();
    calculateNextStepValues();
    calculateP();
    updateV();
}

/* Main function */
main() {
    Display *disp;
    Window win;
    GC gc;

    /* Set up X Windows */
    setupXWindows(&disp, &win, &gc);

    /* Load map files from stdin */
    loadMapFiles();

    /* Infinite loop to update the simulation */
    for (;;) {
        sleepForInterval();
        calculateAngles();
        updateDisplay(disp, win, gc);
        handleKeyPress(disp);
        updatePhysics();
    }
}