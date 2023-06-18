/*
Compile:

gcc -ansi banks.c -lm -lX11 -o banks

Run:

cat horizon.scene pittsburgh.scene | ./banks

Controls:

Arrow keys are the flight stick.
Enter re-centers stick left-right, but not forward-back.
PageUp, PageDn = throttle
*/

#include <stdbool.h> //required for bool
#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdint.h> //required for uint32_t
#include <stdlib.h> //required for malloc
#include <string.h> //required for memset
#include "3denv.h" //custom objects
#include "palette.h" //enum of web colors for tft display
#include <time.h> 
#include <sys/sysinfo.h>


/*define stuff from mode_flight.c*/
//#define TFT_WIDTH 640
//#define TFT_HEIGHT 480


/* Keyboard symbols we accept. Originally were -D defines on compile line. */

#define Throttle_Up XK_Page_Up
#define Throttle_Down XK_Page_Down
#define Up XK_Up
#define Down XK_Down
#define Left XK_Left
#define Right XK_Right
#define Enter XK_Return

/* Update every 0.02 seconds. */

#define dt 0.0166


double  P, 
        timeDelta = dt, 
        worldZ[999], 
        speed = 8, 
        up_down, 
        worldY[999];

double  worldX[999], 
        left_right;

double  F;

int     N, 
        num_pts; 
        
Window win;

char infoStr[52];
char flightStr[12];

GC gc; //graphics context, used by xdrawline https://tronche.com/gui/x/xlib/GC/manipulating.html

// converts 255,255,255 RGB values to xlib format
unsigned long _RGB(int r,int g, int b){
    return b + (g<<8) + (r<<16);
} 

//#define CROSSHAIR_COLOR _RGB(255,255,255) //used by crosshair draw

// draws a line to the display. this is a shim for mode_flight.c compatibility
void speedyLine(Display * disp, int16_t x0, int16_t y0, int16_t x1, int16_t y1, unsigned long c1s ){
    XSetForeground(disp, gc, c1s);
    XDrawLine(disp, win, gc, x0, y0, x1, y1);
    XSetForeground(disp, gc, _RGB(255,255,255));
}

//begin mode_flight.c stuff

/*============================================================================
 * Defines
 *==========================================================================*/

// Thruster speed, etc.
#define THRUSTER_ACCEL   2
#define THRUSTER_MAX     42 //NOTE: THRUSTER_MAX must be divisble by THRUSTER_ACCEL
#define THRUSTER_DECAY   2
#define FLIGHT_SPEED_DEC 12
#define FLIGHT_MAX_SPEED 96
#define FLIGHT_MAX_SPEED_FREE 50
#define FLIGHT_MIN_SPEED 10

#define VIEWPORT_PERSPECTIVE 600
#define VIEWPORT_DIV  4
#define OOBMUX 1.3333  //based on 48 bams/m

#define BOOLET_SPEED_DIVISOR 11

//XXX TODO: Refactor - these should probably be unified.
#define MAXRINGS 15
#define MAX_DONUTS 14
#define MAX_BEANS 69

// Target 40 FPS.
#define DEFAULT_FRAMETIME 25000
#define CROSSHAIR_COLOR _RGB(255,255,255)
#define BOOLET_COLOR 198 //Very bright yellow-orange
#define CNDRAW_BLACK 0
#define CNDRAW_WHITE _RGB(255,255,255) // was actually greenish, now modified to white again
#define PROMPT_COLOR 92
#define OTHER_PLAYER_COLOR 5
#define MAX_COLOR cTransparent

#define BOOLET_MAX_LIFETIME 8000000
#define BOOLET_HIT_DIST_SQUARED 2400 // 60x60

#define FIXEDPOINT 16
#define FIXEDPOINTD2 15

#define flightGetCourseTimeUs() ( flight->paused ? (flight->timeOfPause - flight->timeOfStart) : ((uint32_t)esp_timer_get_time() - flight->timeOfStart) )

#define m00 0
#define m01 1
#define m02 2
#define m03 3
#define m10 4
#define m11 5
#define m12 6
#define m13 7
#define m20 8
#define m21 9
#define m22 10
#define m23 11
#define m30 12
#define m31 13
#define m32 14
#define m33 15

#define TFT_WIDTH 640
#define TFT_HEIGHT 480

//this is a chip-specific thing maybe
uint32_t cndrawPerfcounter;
#define PERFHIT {cndrawPerfcounter++;}

#define speedyHash( seed ) (( seed = (seed * 1103515245) + 12345 ), seed>>16 )

const int16_t sin1024[360] =
{
    0, 18, 36, 54, 71, 89, 107, 125, 143, 160, 178, 195, 213, 230, 248, 265,
    282, 299, 316, 333, 350, 367, 384, 400, 416, 433, 449, 465, 481, 496, 512,
    527, 543, 558, 573, 587, 602, 616, 630, 644, 658, 672, 685, 698, 711, 724,
    737, 749, 761, 773, 784, 796, 807, 818, 828, 839, 849, 859, 868, 878, 887,
    896, 904, 912, 920, 928, 935, 943, 949, 956, 962, 968, 974, 979, 984, 989,
    994, 998, 1002, 1005, 1008, 1011, 1014, 1016, 1018, 1020, 1022, 1023, 1023,
    1024, 1024, 1024, 1023, 1023, 1022, 1020, 1018, 1016, 1014, 1011, 1008,
    1005, 1002, 998, 994, 989, 984, 979, 974, 968, 962, 956, 949, 943, 935, 928,
    920, 912, 904, 896, 887, 878, 868, 859, 849, 839, 828, 818, 807, 796, 784,
    773, 761, 749, 737, 724, 711, 698, 685, 672, 658, 644, 630, 616, 602, 587,
    573, 558, 543, 527, 512, 496, 481, 465, 449, 433, 416, 400, 384, 367, 350,
    333, 316, 299, 282, 265, 248, 230, 213, 195, 178, 160, 143, 125, 107, 89,
    71, 54, 36, 18, 0, -18, -36, -54, -71, -89, -107, -125, -143, -160, -178,
    -195, -213, -230, -248, -265, -282, -299, -316, -333, -350, -367, -384,
    -400, -416, -433, -449, -465, -481, -496, -512, -527, -543, -558, -573,
    -587, -602, -616, -630, -644, -658, -672, -685, -698, -711, -724, -737,
    -749, -761, -773, -784, -796, -807, -818, -828, -839, -849, -859, -868,
    -878, -887, -896, -904, -912, -920, -928, -935, -943, -949, -956, -962,
    -968, -974, -979, -984, -989, -994, -998, -1002, -1005, -1008, -1011, -1014,
    -1016, -1018, -1020, -1022, -1023, -1023, -1024, -1024, -1024, -1023, -1023,
    -1022, -1020, -1018, -1016, -1014, -1011, -1008, -1005, -1002, -998, -994,
    -989, -984, -979, -974, -968, -962, -956, -949, -943, -935, -928, -920,
    -912, -904, -896, -887, -878, -868, -859, -849, -839, -828, -818, -807,
    -796, -784, -773, -761, -749, -737, -724, -711, -698, -685, -672, -658,
    -644, -630, -616, -602, -587, -573, -558, -543, -527, -512, -496, -481,
    -465, -449, -433, -416, -400, -384, -367, -350, -333, -316, -299, -282,
    -265, -248, -230, -213, -195, -178, -160, -143, -125, -107, -89, -71, -54,
    -36, -18
};

/*============================================================================
 * Structs, Enums
 *==========================================================================*/


typedef enum
{
//    FLIGHT_MENU,
    FLIGHT_GAME,
    FLIGHT_GAME_OVER,
    FLIGHT_HIGH_SCORE_ENTRY,
    FLIGHT_SHOW_HIGH_SCORES,
    FLIGHT_FREEFLIGHT,
} flightModeScreen;

typedef struct
{
    uint16_t nrvertnums;
    uint16_t nrfaces;
    uint16_t indices_per_face;
    int16_t center[3];
    int16_t radius;
    uint16_t label;
    int16_t indices_and_vertices[1];
} tdModel;

typedef struct
{
    const tdModel * model;
    int       mrange;
} modelRangePair_t;

typedef enum
{
    FLIGHT_LED_NONE,
    FLIGHT_LED_MENU_TICK,
    FLIGHT_LED_GAME_START,
    FLIGHT_LED_BEAN,
    FLIGHT_LED_DONUT,
    FLIGHT_LED_ENDING,
    FLIGHT_LED_GOT_HIT,
    FLIGHT_LED_DIED,
    FLIGHT_GOT_KILL,
} flLEDAnimation;

//////////////////////////////////////////////////////////////////////////////
// Multiplayer

#define FSNET_CODE_SERVER 0x73534653
#define FSNET_CODE_PEER 0x66534653

#define MAX_PEERS 103 // Best if it's a prime number. Better if it's more than you will ever see, since this is used as a hashtable.
#define BOOLETSPERPLAYER 4
#define MAX_BOOLETS_FROM_HOST 96 //96 = 24(guns)*4 boolets each.
#define MAX_BOOLETS (MAX_PEERS*BOOLETSPERPLAYER+MAX_BOOLETS_FROM_HOST) 
#define MAX_NETWORK_MODELS 108 //84 + 24(guns)

typedef struct  // 32 bytes.
{
    uint32_t timeOffsetOfPeerFromNow;
    uint32_t timeOfUpdate;  // In our timestamp
    uint8_t  mac[6];

    // Not valid for server.
    int16_t posAt[3];
    int8_t  velAt[3];
    int8_t  rotAt[3];    // Right now, only HPR where R = 0
    uint8_t  basePeerFlags; // If zero, don't render.  Note flags&1 has reserved meaning locally for presence..  if flags & 2, render as dead.
    uint16_t  auxPeerFlags; // If dead, is ID of boolet which killed "me"
    uint8_t  framesDead;
    uint8_t  reqColor;
} multiplayerpeer_t;

typedef struct  // Rounds up to 16 bytes.
{
    uint32_t timeOfLaunch; // In our timestamp.
    int16_t launchLocation[3];
    int16_t launchRotation[2]; // Pitch and Yaw
    uint16_t flags;  // If 0, disabled.  Otherwise, is a randomized ID.
} boolet_t;

typedef struct
{
    uint32_t timeOfUpdate;
    uint32_t binencprop;      //Encoding starts at lsb.  First: # of bones, then the way the bones are interconnected.
    int16_t root[3];
    uint8_t  radius;
    uint8_t  reqColor;
    int8_t  velocity[3];
    int8_t  bones[0*3];  //Does not need to be all that long.

} network_model_t;


//////////////////////////////////////////////////////////////////////////////

typedef struct
{
    flightModeScreen mode;
    Display * disp;
    int frames, tframes;
    uint8_t buttonState;
    bool paused;

    int16_t planeloc[3];
    int32_t planeloc_fine[3];
    int16_t hpr[3];
    int16_t speed;
    int16_t pitchmoment;
    int16_t yawmoment;
    int8_t lastSpeed[3];
    bool perfMotion;
    bool oob;
    uint32_t lastFlightUpdate;


    int enviromodels;
    const tdModel ** environment;
    const tdModel * otherShip;

//    meleeMenu_t * menu;
//    font_t ibm;
//    font_t radiostars;
//    font_t meleeMenuFont;

    int beans;
    int ondonut;
    uint32_t timeOfStart;
    uint32_t timeGot100Percent;
    uint32_t timeAccumulatedAtPause;
    uint32_t timeOfPause;
    int wintime;
    uint16_t menuEntryForInvertY;

    flLEDAnimation ledAnimation;
    uint8_t        ledAnimationTime;

    char highScoreNameBuffer[4];
    uint8_t beangotmask[15];

//    flightSimSaveData_t savedata;
    int didFlightsimDataLoad;

    int16_t ModelviewMatrix[16];
    int16_t ProjectionMatrix[16];
    long int renderlinecolor;

    // Boolets for multiplayer.
    multiplayerpeer_t allPeers[MAX_PEERS]; //32x103 = 3296 bytes.
    multiplayerpeer_t serverPeer;
    boolet_t allBoolets[MAX_BOOLETS];  // ~8kB
    network_model_t * networkModels[MAX_NETWORK_MODELS];
    int nNetworkServerExclusiveMode; // When seeing a server resets.
    int nNetworkMode;

    boolet_t myBoolets[BOOLETSPERPLAYER];
    uint16_t booletHitHistory[BOOLETSPERPLAYER];  // For boolets that collide with us.
    int booletHitHistoryHead;
    int myBooletHead;
    int timeOfLastShot;
    int myHealth;
    uint32_t timeOfDeath;
    uint32_t lastNetUpdate;
    uint16_t killedByBooletID;

    int kills;
    int deaths;

    modelRangePair_t * mrp;

    uint8_t bgcolor;
    uint8_t was_hit_by_boolet;
} flight_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

//bool getFlightSaveData(flight_t* flightPtr);
//bool setFlightSaveData( flightSimSaveData_t * sd );
static void flightRender(int64_t elapsedUs);
static void flightBackground(Display * disp, int16_t x, int16_t y, int16_t w, int16_t h, int16_t up, int16_t upNum );
static void flightEnterMode(Display * disp);
static void flightExitMode(void);
//static void flightButtonCallback( buttonEvt_t* evt );
static void flightUpdate(void* arg __attribute__((unused)));
//static void flightMenuCb(const char* menuItem);
static void flightStartGame(flightModeScreen mode);
static void flightGameUpdate( flight_t * tflight );
static void flightUpdateLEDs(flight_t * tflight);
//static void flightLEDAnimate( flLEDAnimation anim );
int tdModelVisibilitycheck( const tdModel * m );
void tdDrawModel( Display * disp, const tdModel * m );
static int flightTimeHighScorePlace( int wintime, bool is100percent );
static void flightTimeHighScoreInsert( int insertplace, bool is100percent, char * name, int timeCentiseconds );
//static void FlightNetworkFrameCall( flight_t * tflight, Display * disp, uint32_t now, modelRangePair_t ** mrp );
//static void FlightfnEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);
//static void FlightfnEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);

static void TModOrDrawBoolet( flight_t * tflight, tdModel * tmod, int16_t * mat, boolet_t * b, uint32_t now );
static void TModOrDrawPlayer( flight_t * tflight, tdModel * tmod, int16_t * mat, multiplayerpeer_t * b, uint32_t now );
static void TModOrDrawCustomNetModel( flight_t * tflight, tdModel * tmod, int16_t * mat, network_model_t * b, uint32_t now );

static uint32_t ReadUQ( uint32_t * rin, uint32_t bits );
static uint32_t PeekUQ( uint32_t * rin, uint32_t bits );
static uint32_t ReadBitQ( uint32_t * rin );
static uint32_t ReadUEQ( uint32_t * rin );
static int WriteUQ( uint32_t * v, uint32_t number, int bits );
static int WriteUEQ( uint32_t * v, uint32_t number );


int mdlctcmp( const void * va, const void * vb )
{
    const modelRangePair_t * a = (const modelRangePair_t *)va;
    const modelRangePair_t * b = (const modelRangePair_t *)vb;
    return b->mrange - a->mrange;
}

//Forward libc declarations.
#ifndef EMU
void qsort(void *base, size_t nmemb, size_t size,
          int (*compar)(const void *, const void *));
int abs(int j);
int uprintf( const char * fmt, ... );
#else
#define uprintf printf
#endif

/*============================================================================
 * Variables
 *==========================================================================*/

static const char fl_title[]  = "Flyin Donut";
static const char fl_flight_env[] = "Atrium Course";
static const char fl_flight_invertY0_env[] = "Y Invert: Off";
static const char fl_flight_invertY1_env[] = "Y Invert: On";
static const char fl_flight_perf[] = "Free Solo/VS";
static const char fl_100_percent[] = "100% 100% 100%";
static const char fl_turn_around[] = "TURN AROUND";
static const char fl_you_win[] = "YOU   WIN!";
static const char fl_paused[] = "PAUSED";
static const char str_quit[] = "Exit";
static const char str_high_scores[] = "High Scores";

static unsigned long boot_time_in_micros = 0;

flight_t* flight;

int64_t esp_timer_get_time(void)
{
    struct timespec ts;
    if (0 != clock_gettime(CLOCK_MONOTONIC, &ts))
    {
        #define ESP_LOGE(tag,fmt,...) fprintf(stderr, tag ": " fmt "\n", ##__VA_ARGS__)
        ESP_LOGE("EMU", "Clock err");
        return 0;
    }
    return ((ts.tv_sec * 1000000) + (ts.tv_nsec / 1000)) - boot_time_in_micros;
}

//used by flightrender + setup matrix
void tdIdentity( int16_t * matrix )
{
    matrix[0] = 256; matrix[1] = 0; matrix[2] = 0; matrix[3] = 0;
    matrix[4] = 0; matrix[5] = 256; matrix[6] = 0; matrix[7] = 0;
    matrix[8] = 0; matrix[9] = 0; matrix[10] = 256; matrix[11] = 0;
    matrix[12] = 0; matrix[13] = 0; matrix[14] = 0; matrix[15] = 256;
}

//used by flightrender + setup matrix
void Perspective( int fovx, int aspect, int zNear, int zFar, int16_t * out )
{
    int16_t f = fovx;
    out[0] = f*256/aspect; out[1] = 0; out[2] = 0; out[3] = 0;
    out[4] = 0; out[5] = f; out[6] = 0; out[7] = 0;
    out[8] = 0; out[9] = 0;
    out[10] = 256*(zFar + zNear)/(zNear - zFar);
    out[11] = 2*zFar*zNear  /(zNear - zFar);
    out[12] = 0; out[13] = 0; out[14] = -256; out[15] = 0;
}

//used by flightrender
void SetupMatrix( void )
{
    tdIdentity( flight->ProjectionMatrix );
    tdIdentity( flight->ModelviewMatrix );

    Perspective( VIEWPORT_PERSPECTIVE, 256 /* 1.0 */, 50, 8192, flight->ProjectionMatrix );
}

/**
 * Integer sine function
 *
 * @param degree The degree, between 0 and 359
 * @return int16_t The sine of the degree, between -1024 and 1024
 */
int16_t getSin1024(int16_t degree)
{
    degree = ( (degree % 360) + 360 ) % 360;
    return sin1024[degree];
}

/**
 * Integer cosine function
 *
 * @param degree The degree, between 0 and 359
 * @return int16_t The cosine of the degree, between -1024 and 1024
 */
int16_t getCos1024(int16_t degree)
{
    // cos is just sin offset by 90 degrees
    degree = ( (degree % 360) + 450 ) % 360;
    return sin1024[degree];
}

void tdRotateNoMulEA( int16_t * f, int16_t x, int16_t y, int16_t z )
{
    //x,y,z must be negated for some reason
    const int16_t * stable = sin1024;
    int16_t cx = stable[((x>=270)?(x-270):(x+90))]>>2;
    int16_t sx = stable[x]>>2;
    int16_t cy = stable[((y>=270)?(y-270):(y+90))]>>2;
    int16_t sy = stable[y]>>2;
    int16_t cz = stable[((z>=270)?(z-270):(z+90))]>>2;
    int16_t sz = stable[z]>>2;

    //Row major
    //manually transposed
    f[m00] = (cy*cz)>>8;
    f[m10] = ((((sx*sy)>>8)*cz)-(cx*sz))>>8;
    f[m20] = ((((cx*sy)>>8)*cz)+(sx*sz))>>8;
    f[m30] = 0;

    f[m01] = (cy*sz)>>8;
    f[m11] = ((((sx*sy)>>8)*sz)+(cx*cz))>>8;
    f[m21] = ((((cx*sy)>>8)*sz)-(sx*cz))>>8;
    f[m31] = 0;

    f[m02] = -sy;
    f[m12] = (sx*cy)>>8;
    f[m22] = (cx*cy)>>8;
    f[m32] = 0;

    f[m03] = 0;
    f[m13] = 0;
    f[m23] = 0;
    f[m33] = 1;
}

void tdMultiply( int16_t * fin1, int16_t * fin2, int16_t * fout )
{
    int16_t fotmp[16];

    fotmp[m00] = ((int32_t)fin1[m00] * (int32_t)fin2[m00] + (int32_t)fin1[m01] * (int32_t)fin2[m10] + (int32_t)fin1[m02] * (int32_t)fin2[m20] + (int32_t)fin1[m03] * (int32_t)fin2[m30])>>8;
    fotmp[m01] = ((int32_t)fin1[m00] * (int32_t)fin2[m01] + (int32_t)fin1[m01] * (int32_t)fin2[m11] + (int32_t)fin1[m02] * (int32_t)fin2[m21] + (int32_t)fin1[m03] * (int32_t)fin2[m31])>>8;
    fotmp[m02] = ((int32_t)fin1[m00] * (int32_t)fin2[m02] + (int32_t)fin1[m01] * (int32_t)fin2[m12] + (int32_t)fin1[m02] * (int32_t)fin2[m22] + (int32_t)fin1[m03] * (int32_t)fin2[m32])>>8;
    fotmp[m03] = ((int32_t)fin1[m00] * (int32_t)fin2[m03] + (int32_t)fin1[m01] * (int32_t)fin2[m13] + (int32_t)fin1[m02] * (int32_t)fin2[m23] + (int32_t)fin1[m03] * (int32_t)fin2[m33])>>8;

    fotmp[m10] = ((int32_t)fin1[m10] * (int32_t)fin2[m00] + (int32_t)fin1[m11] * (int32_t)fin2[m10] + (int32_t)fin1[m12] * (int32_t)fin2[m20] + (int32_t)fin1[m13] * (int32_t)fin2[m30])>>8;
    fotmp[m11] = ((int32_t)fin1[m10] * (int32_t)fin2[m01] + (int32_t)fin1[m11] * (int32_t)fin2[m11] + (int32_t)fin1[m12] * (int32_t)fin2[m21] + (int32_t)fin1[m13] * (int32_t)fin2[m31])>>8;
    fotmp[m12] = ((int32_t)fin1[m10] * (int32_t)fin2[m02] + (int32_t)fin1[m11] * (int32_t)fin2[m12] + (int32_t)fin1[m12] * (int32_t)fin2[m22] + (int32_t)fin1[m13] * (int32_t)fin2[m32])>>8;
    fotmp[m13] = ((int32_t)fin1[m10] * (int32_t)fin2[m03] + (int32_t)fin1[m11] * (int32_t)fin2[m13] + (int32_t)fin1[m12] * (int32_t)fin2[m23] + (int32_t)fin1[m13] * (int32_t)fin2[m33])>>8;

    fotmp[m20] = ((int32_t)fin1[m20] * (int32_t)fin2[m00] + (int32_t)fin1[m21] * (int32_t)fin2[m10] + (int32_t)fin1[m22] * (int32_t)fin2[m20] + (int32_t)fin1[m23] * (int32_t)fin2[m30])>>8;
    fotmp[m21] = ((int32_t)fin1[m20] * (int32_t)fin2[m01] + (int32_t)fin1[m21] * (int32_t)fin2[m11] + (int32_t)fin1[m22] * (int32_t)fin2[m21] + (int32_t)fin1[m23] * (int32_t)fin2[m31])>>8;
    fotmp[m22] = ((int32_t)fin1[m20] * (int32_t)fin2[m02] + (int32_t)fin1[m21] * (int32_t)fin2[m12] + (int32_t)fin1[m22] * (int32_t)fin2[m22] + (int32_t)fin1[m23] * (int32_t)fin2[m32])>>8;
    fotmp[m23] = ((int32_t)fin1[m20] * (int32_t)fin2[m03] + (int32_t)fin1[m21] * (int32_t)fin2[m13] + (int32_t)fin1[m22] * (int32_t)fin2[m23] + (int32_t)fin1[m23] * (int32_t)fin2[m33])>>8;

    fotmp[m30] = ((int32_t)fin1[m30] * (int32_t)fin2[m00] + (int32_t)fin1[m31] * (int32_t)fin2[m10] + (int32_t)fin1[m32] * (int32_t)fin2[m20] + (int32_t)fin1[m33] * (int32_t)fin2[m30])>>8;
    fotmp[m31] = ((int32_t)fin1[m30] * (int32_t)fin2[m01] + (int32_t)fin1[m31] * (int32_t)fin2[m11] + (int32_t)fin1[m32] * (int32_t)fin2[m21] + (int32_t)fin1[m33] * (int32_t)fin2[m31])>>8;
    fotmp[m32] = ((int32_t)fin1[m30] * (int32_t)fin2[m02] + (int32_t)fin1[m31] * (int32_t)fin2[m12] + (int32_t)fin1[m32] * (int32_t)fin2[m22] + (int32_t)fin1[m33] * (int32_t)fin2[m32])>>8;
    fotmp[m33] = ((int32_t)fin1[m30] * (int32_t)fin2[m03] + (int32_t)fin1[m31] * (int32_t)fin2[m13] + (int32_t)fin1[m32] * (int32_t)fin2[m23] + (int32_t)fin1[m33] * (int32_t)fin2[m33])>>8;

    memcpy( fout, fotmp, sizeof( fotmp ) );
}

void tdRotateEA( int16_t * f, int16_t x, int16_t y, int16_t z )
{
    int16_t ftmp[16];
    tdRotateNoMulEA( ftmp, x, y, z );
    tdMultiply( f, ftmp, f );
}

void tdTranslate( int16_t * f, int16_t x, int16_t y, int16_t z )
{
//    int16_t ftmp[16];
//    tdIdentity(ftmp);
    f[m03] += x;
    f[m13] += y;
    f[m23] += z;
//    tdMultiply( f, ftmp, f );
}

void tdPtTransform( int16_t * pout, const int16_t * restrict f, const int16_t * restrict pin )
{
    pout[0] = (pin[0] * f[m00] + pin[1] * f[m01] + pin[2] * f[m02] + 256 * f[m03])>>8;
    pout[1] = (pin[0] * f[m10] + pin[1] * f[m11] + pin[2] * f[m12] + 256 * f[m13])>>8;
    pout[2] = (pin[0] * f[m20] + pin[1] * f[m21] + pin[2] * f[m22] + 256 * f[m23])>>8;
    pout[3] = (pin[0] * f[m30] + pin[1] * f[m31] + pin[2] * f[m32] + 256 * f[m33])>>8;
}

void td4Transform( int16_t * pout, const int16_t * restrict f, const int16_t * pin )
{
    int16_t ptmp[3];
    ptmp[0] = (pin[0] * f[m00] + pin[1] * f[m01] + pin[2] * f[m02] + pin[3] * f[m03])>>8;
    ptmp[1] = (pin[0] * f[m10] + pin[1] * f[m11] + pin[2] * f[m12] + pin[3] * f[m13])>>8;
    ptmp[2] = (pin[0] * f[m20] + pin[1] * f[m21] + pin[2] * f[m22] + pin[3] * f[m23])>>8;
    pout[3] = (pin[0] * f[m30] + pin[1] * f[m31] + pin[2] * f[m32] + pin[3] * f[m33])>>8;
    pout[0] = ptmp[0];
    pout[1] = ptmp[1];
    pout[2] = ptmp[2];
}

// Only needs center and radius.
int tdModelVisibilitycheck( const tdModel * m )
{

    //For computing visibility check
    int16_t tmppt[4];
    tdPtTransform( tmppt, flight->ModelviewMatrix, m->center );
    td4Transform( tmppt, flight->ProjectionMatrix, tmppt );
    if( tmppt[3] < -2 )
    {
        int scx = ((256 * tmppt[0] / tmppt[3])/VIEWPORT_DIV+(TFT_WIDTH/2));
        int scy = ((256 * tmppt[1] / tmppt[3])/VIEWPORT_DIV+(TFT_HEIGHT/2));
       // int scz = ((65536 * tmppt[2] / tmppt[3]));
        int scd = ((-256 * 2 * m->radius / tmppt[3])/VIEWPORT_DIV);
        scd += 3; //Slack
        if( scx < -scd || scy < -scd || scx >= TFT_WIDTH + scd || scy >= TFT_HEIGHT + scd )
        {
            return -1;
        }
        else
        {
            return -tmppt[3];
        }
    }
    else
    {
        return -2;
    }
}

int LocalToScreenspace( const int16_t * coords_3v, int16_t * o1, int16_t * o2 )
{
    int16_t tmppt[4];
    tdPtTransform( tmppt, flight->ModelviewMatrix, coords_3v );
    td4Transform( tmppt, flight->ProjectionMatrix, tmppt );
    if( tmppt[3] >= -4 ) { return -1; }
    int calcx = ((256 * tmppt[0] / tmppt[3])/VIEWPORT_DIV+(TFT_WIDTH/2));
    int calcy = ((256 * tmppt[1] / tmppt[3])/VIEWPORT_DIV+(TFT_HEIGHT/2));
    if( calcx < -16000 || calcx > 16000 || calcy < -16000 || calcy > 16000 ) return -2;
    *o1 = calcx;
    *o2 = calcy;
    return 0;
}

static void TModOrDrawBoolet( flight_t * tflight, tdModel * tmod, int16_t * mat, boolet_t * b, uint32_t now )
{
    int32_t delta = now - b->timeOfLaunch;
    int16_t * pa = b->launchLocation;
    int16_t * lr = b->launchRotation;
    int yawDivisor = getCos1024( lr[1]/11 );
    int deltaWS = delta>>BOOLET_SPEED_DIVISOR;

    int32_t direction[3];
    direction[0] = (( getSin1024( lr[0]/11 ) * yawDivisor ) >> 10);
    direction[2] = (( getCos1024( lr[0]/11 ) * yawDivisor ) >> 10);
    direction[1] = -getSin1024( lr[1]/11 );

    int32_t deltas[3];
    deltas[0] = (deltaWS * direction[0] ) >> 10;
    deltas[2] = (deltaWS * direction[2] ) >> 10;
    deltas[1] = (deltaWS * direction[1] ) >> 10;

    int16_t cstuff[3];

    int16_t * center = tmod?tmod->center:cstuff;
    center[0] = pa[0]+deltas[0];
    center[2] = pa[2]+deltas[2];
    center[1] = pa[1]+deltas[1];

    if( tmod )
    {
        tmod->radius = 50;
        return;
    }
    else
    {
        // Control length of boolet line.
        direction[0]>>=3;
        direction[1]>>=3;
        direction[2]>>=3;

        // Actually "Draw" the boolet.
        int16_t end[3];
        int16_t start[3];
        end[0] = center[0] + (direction[0]);
        end[1] = center[1] + (direction[1]);
        end[2] = center[2] + (direction[2]);

        start[0] = center[0];
        start[1] = center[1];
        start[2] = center[2];

        int16_t sx, sy, ex, ey;
        // We now have "start" and "end"

        if( LocalToScreenspace( start, &sx, &sy ) < 0 ) return;
        if( LocalToScreenspace( end, &ex, &ey ) < 0 ) return;

        speedyLine( tflight->disp, sx, sy+1, ex, ey-1, BOOLET_COLOR ); // draw boolet
    }
}

void tdPt3Transform( int16_t * pout, const int16_t * restrict f, const int16_t * restrict pin )
{
    pout[0] = (pin[0] * f[m00] + pin[1] * f[m01] + pin[2] * f[m02] + 256 * f[m03])>>8;
    pout[1] = (pin[0] * f[m10] + pin[1] * f[m11] + pin[2] * f[m12] + 256 * f[m13])>>8;
    pout[2] = (pin[0] * f[m20] + pin[1] * f[m21] + pin[2] * f[m22] + 256 * f[m23])>>8;
}

// A technique to turbo time set pixels (not yet in use) used by 'outlineTriangle'
#define SETUP_FOR_TURBO( disp ) \
    register uint32_t dispWidth = DisplayWidth(disp, DefaultScreen(disp)); \ //TODO: find downstream instances of this and convert it back to 'dispWidth' starting on 864. THis line is fine but dwnstram is bad 
    register uint32_t dispHeight = DisplayHeight(disp, DefaultScreen(disp)); \ //TODO: same but 'dispHeight' #ref main/display/display.h This line is fine too bug downstream is also bad
    register uint32_t dispPx = (uint32_t)disp->pxFb; \ //TODO: go over struct display on line 64 of display.h it is referencing 'paletteColor_t' which is that wacky web safe color thing
// comments on above lines fuck up the \ forward slash line breaks.

/**
 * @brief Optimized method to draw a triangle with outline.
 *
 * @param x2, x1, x0 Column of display, 0 is at the left
 * @param y2, y1, y0 Row of the display, 0 is at the top
 * @param colorA filled area color
 * @param colorB outline color
 *
 */
void outlineTriangle( Display * disp, int16_t v0x, int16_t v0y, int16_t v1x, int16_t v1y,
                                        int16_t v2x, int16_t v2y, paletteColor_t colorA, paletteColor_t colorB )
{
	SETUP_FOR_TURBO( disp );
	
    int16_t i16tmp;

    //Sort triangle such that v0 is the top-most vertex.
    //v0->v1 is LEFT edge.
    //v0->v2 is RIGHT edge.

    if( v0y > v1y )
    {
        i16tmp = v0x;
        v0x = v1x;
        v1x = i16tmp;
        i16tmp = v0y;
        v0y = v1y;
        v1y = i16tmp;
    }
    if( v0y > v2y )
    {
        i16tmp = v0x;
        v0x = v2x;
        v2x = i16tmp;
        i16tmp = v0y;
        v0y = v2y;
        v2y = i16tmp;
    }

    //v0 is now top-most vertex.  Now orient 2 and 3.
    //Tricky: Use slopes!  Otherwise, we could get it wrong.
    {
        int slope02;
        if( v2y - v0y )
        {
            slope02 = ((v2x - v0x) << FIXEDPOINT) / (v2y - v0y);
        }
        else
        {
            slope02 = ((v2x - v0x) > 0) ? 0x7fffff : -0x800000;
        }

        int slope01;
        if( v1y - v0y )
        {
            slope01 = ((v1x - v0x) << FIXEDPOINT) / (v1y - v0y);
        }
        else
        {
            slope01 = ((v1x - v0x) > 0) ? 0x7fffff : -0x800000;
        }

        if( slope02 < slope01 )
        {
            i16tmp = v1x;
            v1x = v2x;
            v2x = i16tmp;
            i16tmp = v1y;
            v1y = v2y;
            v2y = i16tmp;
        }
    }

    //We now have a fully oriented triangle.
    int16_t x0A = v0x;
    int16_t y0A = v0y;
    int16_t x0B = v0x;
    //int16_t y0B = v0y;

    //A is to the LEFT of B.
    int dxA = (v1x - v0x);
    int dyA = (v1y - v0y);
    int dxB = (v2x - v0x);
    int dyB = (v2y - v0y);
    int sdxA = (dxA > 0) ? 1 : -1;
    int sdyA = (dyA > 0) ? 1 : -1;
    int sdxB = (dxB > 0) ? 1 : -1;
    int sdyB = (dyB > 0) ? 1 : -1;
    int xerrdivA = ( dyA * sdyA );  //dx, but always positive.
    int xerrdivB = ( dyB * sdyB );  //dx, but always positive.
    int xerrnumeratorA = 0;
    int xerrnumeratorB = 0;

    if( xerrdivA )
    {
        xerrnumeratorA = (((dxA * sdxA) << FIXEDPOINT) + xerrdivA / 2 ) / xerrdivA;
    }
    else
    {
        xerrnumeratorA = 0x7fffff;
    }

    if( xerrdivB )
    {
        xerrnumeratorB = (((dxB * sdxB) << FIXEDPOINT) + xerrdivB / 2 ) / xerrdivB;
    }
    else
    {
        xerrnumeratorB = 0x7fffff;
    }

    //X-clipping is handled on a per-scanline basis.
    //Y-clipping must be handled upfront.

    /*
        //Optimization BUT! Can't do this here, as we would need to be smarter about it.
        //If we do this, and the second triangle is above y=0, we'll get the wrong answer.
        if( y0A < 0 )
        {
            delta = 0 - y0A;
            y0A = 0;
            y0B = 0;
            x0A += (((xerrnumeratorA*delta)) * sdxA) >> FIXEDPOINT; //Could try rounding.
            x0B += (((xerrnumeratorB*delta)) * sdxB) >> FIXEDPOINT;
        }
    */

	PERFHIT

    {
        //Section 1 only.
        int yend = (v1y < v2y) ? v1y : v2y;
        int errA = 1 << FIXEDPOINTD2;
        int errB = 1 << FIXEDPOINTD2;
        int y;

        //Going between x0A and x0B
        for( y = y0A; y < yend; y++ )
        {
            int x = x0A;
            int endx = x0B;
            int suppress = 1;
			
			PERFHIT

            if( y >= 0 && y < (int)dispHeight ) //TODO: don't hard code to display 0 snum = DefaultScreen(dpy)
            {
                suppress = 0;
                if( x < 0 )
                {
                    x = 0;
                }
                if( endx > (int)dispWidth)
                {
                    endx = (int)dispWidth;
                }
				
				// Draw left line
                if( x0A >= 0  && x0A < (int)dispWidth )
                {
                    TURBO_SET_PIXEL( disp, x0A, y, colorB );
                    x++;
                }
				
				// Draw body
                for( ; x < endx; x++ )
                {
					PERFHIT
                    TURBO_SET_PIXEL( disp, x, y, colorA );
                }
				
				// Draw right line
                if( x0B < (int)dispWidth && x0B >= 0 )
                {
                    TURBO_SET_PIXEL( disp, x0B, y, colorB );
                }
            }

            //Now, advance the start/end X's.
            errA += xerrnumeratorA;
            errB += xerrnumeratorB;
            while( errA >= (1 << FIXEDPOINT) && x0A != v1x )
            {
				PERFHIT
                x0A += sdxA;
                //if( x0A < 0 || x0A > (dispWidth-1) ) break;
                if( x0A >= 0 && x0A < (int)dispWidth && !suppress )
                {
                    TURBO_SET_PIXEL( disp, x0A, y, colorB );
                }
                errA -= 1 << FIXEDPOINT;
            }
            while( errB >= (1 << FIXEDPOINT) && x0B != v2x )
            {
				PERFHIT
                x0B += sdxB;
                //if( x0B < 0 || x0B > (dispWidth-1) ) break;
                if( x0B >= 0 && x0B < (int)dispWidth && !suppress )
                {
                    TURBO_SET_PIXEL( disp, x0B, y, colorB );
                }
                errB -= 1 << FIXEDPOINT;
            }
        }

        //We've come to the end of section 1.  Now, we need to figure

        //Now, yend is the highest possible hit on the triangle.

        //v1 is LEFT OF v2
        // A is LEFT OF B
        if( v1y < v2y )
        {
            //V1 has terminated, move to V1->V2 but keep V0->V2[B] segment
            yend = v2y;
            dxA = (v2x - v1x);
            dyA = (v2y - v1y);
            sdxA = (dxA > 0) ? 1 : -1;
            sdyA = (dyA > 0) ? 1 : -1;
            xerrdivA = ( dyA * sdyA );  //dx, but always positive.
            if( xerrdivA )
            {
                xerrnumeratorA = (((dxA * sdxA) << FIXEDPOINT) + xerrdivA / 2 ) / xerrdivA;
            }
            else
            {
                xerrnumeratorA = 0x7fffff;
            }
            x0A = v1x;
            errA = 1 << FIXEDPOINTD2;
        }
        else
        {
            //V2 has terminated, move to V2->V1 but keep V0->V1[A] segment
            yend = v1y;
            dxB = (v1x - v2x);
            dyB = (v1y - v2y);
            sdxB = (dxB > 0) ? 1 : -1;
            sdyB = (dyB > 0) ? 1 : -1;
            xerrdivB = ( dyB * sdyB );  //dx, but always positive.
            if( xerrdivB )
            {
                xerrnumeratorB = (((dxB * sdxB) << FIXEDPOINT) + xerrdivB / 2 ) / xerrdivB;
            }
            else
            {
                xerrnumeratorB = 0x7fffff;
            }
            x0B = v2x;
            errB = 1 << FIXEDPOINTD2;
        }

        if( yend > (int)(dispHeight - 1) )
        {
            yend = (int)dispHeight - 1;
        }



        if( xerrnumeratorA > 1000000 || xerrnumeratorB > 1000000 )
        {
            if( x0A < x0B )
            {
                sdxA = 1;
                sdxB = -1;
            }
            if( x0A > x0B )
            {
                sdxA = -1;
                sdxB = 1;
            }
            if( x0A == x0B )
            {
                if( x0A >= 0 && x0A < (int)dispWidth) && y >= 0 && y < (int)dispHeight )
                {
                    TURBO_SET_PIXEL( disp, x0A, y, colorB );
                }
                return;
            }
        }

        for( ; y <= yend; y++ )
        {
            int x = x0A;
            int endx = x0B;
            int suppress = 1;
			PERFHIT

            if( y >= 0 && y <= (int)(dispHeight - 1) )
            {
                suppress = 0;
                if( x < 0 )
                {
                    x = 0;
                }
                if( endx >= (int)(dispWidth) )
                {
                    endx = (dispWidth);
                }
				
				// Draw left line
                if( x0A >= 0  && x0A < (int)(dispWidth) )
                {
                    TURBO_SET_PIXEL( disp, x0A, y, colorB );
                    x++;
                }
				
				// Draw body
                for( ; x < endx; x++ )
                {
					PERFHIT
                    TURBO_SET_PIXEL( disp, x, y, colorA );
                }
				
				// Draw right line
                if( x0B < (int)(dispWidth) && x0B >= 0 )
                {
                    TURBO_SET_PIXEL( disp, x0B, y, colorB );
                }
            }

            //Now, advance the start/end X's.
            errA += xerrnumeratorA;
            errB += xerrnumeratorB;
            while( errA >= (1 << FIXEDPOINT) )
            {
				PERFHIT
                x0A += sdxA;
                //if( x0A < 0 || x0A > (dispWidth-1) ) break;
                if( x0A >= 0 && x0A < (int)(dispWidth) && !suppress )
                {
                    TURBO_SET_PIXEL( disp, x0A, y, colorB );
                }
                errA -= 1 << FIXEDPOINT;
                if( x0A == x0B  )
                {
                    return;
                }
            }
            while( errB >= (1 << FIXEDPOINT) )
            {
				PERFHIT
                x0B += sdxB;
                if( x0B >= 0 && x0B < (int)(dispWidth) && !suppress )
                {
                    TURBO_SET_PIXEL( disp, x0B, y, colorB );
                }
                errB -= 1 << FIXEDPOINT;
                if( x0A == x0B  )
                {
                    return;
                }
            }

        }
    }
}

void tdDrawModel( Display * disp, const tdModel * m )
{
    int i;

    int nrv = m->nrvertnums;
    int nri = m->nrfaces*m->indices_per_face;
    const int16_t * verticesmark = (const int16_t*)&m->indices_and_vertices[nri];

#if 0
    // By the time we get here, we're sure we want to render.
    if( tdModelVisibilitycheck( m ) < 0 )
    {
        return;
    }
#endif

    //This looks a little odd, but what we're doing is caching our vertex computations
    //so we don't have to re-compute every time round.
    //f( "%d\n", nrv );
    int16_t cached_verts[nrv];

    for( i = 0; i < nrv; i+=3 )
    {
        int16_t * cv1 = &cached_verts[i];
        if( LocalToScreenspace( &verticesmark[i], cv1, cv1+1 ) )
            cv1[2] = 2;
        else
            cv1[2] = 1;
    }

    if( m->indices_per_face == 2 )
    {
        for( i = 0; i < nri; i+=2 )
        {
            int i1 = m->indices_and_vertices[i];
            int i2 = m->indices_and_vertices[i+1];
            int16_t * cv1 = &cached_verts[i1];
            int16_t * cv2 = &cached_verts[i2];

            if( cv1[2] != 2 && cv2[2] != 2 )
            {
                speedyLine( disp, cv1[0], cv1[1], cv2[0], cv2[1], flight->renderlinecolor ); //draw model?
            }
        }
    }
    else if( m->indices_per_face == 3 )
    {
        for( i = 0; i < nri; i+=3 )
        {
            int i1 = m->indices_and_vertices[i];
            int i2 = m->indices_and_vertices[i+1];
            int i3 = m->indices_and_vertices[i+2];
            int16_t * cv1 = &cached_verts[i1];
            int16_t * cv2 = &cached_verts[i2];
            int16_t * cv3 = &cached_verts[i3];
            //printf( "%d/%d/%d  %d %d %d\n", i1, i2, i3, cv1[2], cv2[2], cv3[2] );

            if( cv1[2] != 2 && cv2[2] != 2 && cv3[2] != 2 )
            {

                //Perform screen-space cross product to determine if we're looking at a backface.
                int Ux = cv3[0] - cv1[0];
                int Uy = cv3[1] - cv1[1];
                int Vx = cv2[0] - cv1[0];
                int Vy = cv2[1] - cv1[1];
                if( Ux*Vy-Uy*Vx >= 0 )
                    outlineTriangle( disp, cv1[0], cv1[1], cv2[0], cv2[1], cv3[0], cv3[1], flight->bgcolor, flight->renderlinecolor );
            }
        }
    }
}

static void TModOrDrawPlayer( flight_t * tflight, tdModel * tmod, int16_t * mat, multiplayerpeer_t * p, uint32_t now )
{
    int32_t deltaTime = (now - p->timeOfUpdate);
    // This isn't "exactly" right since we assume the 
    int16_t * pa = p->posAt;
    int8_t * va = p->velAt;
    if( tmod )
    {
        tmod->center[0] = pa[0] + ((va[0] * deltaTime)>>16);
        tmod->center[1] = pa[1] + ((va[1] * deltaTime)>>16);
        tmod->center[2] = pa[2] + ((va[2] * deltaTime)>>16);
        tmod->radius = tflight->otherShip->radius;
        return;
    }

    const tdModel * s = tflight->otherShip;
    int nri = s->nrfaces*s->indices_per_face;
    int size_of_header_and_indices = 16 + nri*2;

    tdModel * m = alloca( size_of_header_and_indices + s->nrvertnums*6 );

    memcpy( m, s, size_of_header_and_indices ); // Copy header + indices.
    int16_t * mverticesmark = (int16_t*)&m->indices_and_vertices[nri];
    const int16_t * sverticesmark = (const int16_t*)&s->indices_and_vertices[nri];

    int16_t LocalXForm[16];
    {
        uint8_t * ra = (uint8_t*)p->rotAt;  // Tricky: Math works easier on signed numbers.
        tdRotateNoMulEA( LocalXForm, 0, 359-((ra[0]*360)>>8), 0 );  // Perform pitchbefore yaw
        tdRotateEA( LocalXForm, 359-((ra[1]*360)>>8), 0, (ra[2]*360)>>8 );
    }

    LocalXForm[m03] = pa[0] + ((va[0] * deltaTime)>>16);
    LocalXForm[m13] = pa[1] + ((va[1] * deltaTime)>>16);
    LocalXForm[m23] = pa[2] + ((va[2] * deltaTime)>>16);

    tdPt3Transform( m->center, LocalXForm, s->center );    

    int i;
    int lv = s->nrvertnums;

    int fd = p->framesDead;
    int hashseed = p->auxPeerFlags;
    if( fd )
    {
        int nri = s->nrfaces*s->indices_per_face;

        // if dead, blow ship apart.

        // This is a custom function so that we can de-weld the vertices.
        // If they remained welded, then it could not blow apart.
        for( i = 0; i < nri; i+=2 )
        {
            Display * disp = tflight->disp;
            int16_t npos[3];
            int16_t ntmp[3];
            int i1 = s->indices_and_vertices[i];
            int i2 = s->indices_and_vertices[i+1];

            const int16_t * origs = sverticesmark + i1;
            int16_t sx1, sy1, sx2, sy2;

            npos[0] = (origs)[0] + ((((int16_t)speedyHash( hashseed )) * fd)>>14);
            npos[1] = (origs)[1] + ((((int16_t)speedyHash( hashseed )) * fd)>>14);
            npos[2] = (origs)[2] + ((((int16_t)speedyHash( hashseed )) * fd)>>14);
            tdPt3Transform( ntmp, LocalXForm, npos );
            int oktorender = !LocalToScreenspace( ntmp, &sx1, &sy1 );

            origs = sverticesmark + i2;
            npos[0] = (origs)[0] + ((((int16_t)speedyHash( hashseed )) * fd)>>14);
            npos[1] = (origs)[1] + ((((int16_t)speedyHash( hashseed )) * fd)>>14);
            npos[2] = (origs)[2] + ((((int16_t)speedyHash( hashseed )) * fd)>>14);
            tdPt3Transform( ntmp, LocalXForm, npos );
            oktorender &= !LocalToScreenspace( ntmp, &sx2, &sy2 );

            if( oktorender )
                speedyLine( disp, sx1, sy1, sx2, sy2, 180 ); //Override color of explosions to red. draw explosion
        }
        if( fd < 255 ) p->framesDead = fd + 1;
    }
    else
    {
        for( i = 0; i < lv; i+=3 )
        {
            tdPt3Transform( mverticesmark + i, LocalXForm, sverticesmark + i );
        }
        int backupColor = flight->renderlinecolor;
        flight->renderlinecolor = p->reqColor;
        tdDrawModel( tflight->disp, m );
        flight->renderlinecolor = backupColor;
    }
}

void flightc_shim(Display * disp){
    XDrawString(disp, win, gc, 20, 450, flightStr, 17);
    speedyLine(disp, 8, 7, 210, 220, _RGB(0,255,0));

    /**
     * Initializer for flight
     */
    void flightEnterMode(Display * disp)
    {
        // Alloc and clear everything
        flight = malloc(sizeof(flight_t));
        memset(flight, 0, sizeof(flight_t));

        // Hmm this seems not to be obeyed, at least not well?
        //setFrameRateUs( DEFAULT_FRAMETIME );

        flight->mode = 0;
        flight->disp = disp;
        flight->renderlinecolor = CNDRAW_WHITE;

        // load obj data from 3denv.h
        const uint16_t * data = model3d;//(uint16_t*)getAsset( "3denv.obj", &retlen );
        data+=2; //header
        flight->enviromodels = *(data++);
        flight->environment = malloc( sizeof(const tdModel *) * flight->enviromodels );
        
        flight->mrp = malloc( sizeof(modelRangePair_t) * ( flight->enviromodels+MAX_PEERS+MAX_BOOLETS+MAX_NETWORK_MODELS ) );

        int i;
        for( i = 0; i < flight->enviromodels; i++ )
        {
            const tdModel * m = flight->environment[i] = (const tdModel*)data;
            data += 8 + m->nrvertnums + m->nrfaces * m->indices_per_face;
        }

        flight->otherShip = (const tdModel * )(ship3d + 3);  //+ header(3)

        //skip flightMenuCb
        flight->nNetworkMode = 0;
        

        speedyLine(disp, 8, 7, 210, 230, _RGB(0,5,255));
    }

    void flightStartGame( flightModeScreen mode )
    {
        flight->mode = mode;
        flight->frames = 0;

        flight->paused = false;

        flight->ondonut = 0; //Set to 14 to b-line it to the end for testing.
        flight->beans = 0; //Set to MAX_BEANS for 100% instant.
        flight->timeOfStart = (uint32_t)esp_timer_get_time();//-1000000*190; // (Do this to force extra coursetime)
        flight->timeGot100Percent = 0;
        flight->timeOfPause = 0;
        flight->wintime = 0;
        flight->speed = 0;

        //Starting location/orientation
        if( mode == FLIGHT_FREEFLIGHT )
        {
            srand( flight->timeOfStart );
            flight->planeloc[0] = (int16_t)(((rand()%1500)-200)*OOBMUX);
            flight->planeloc[1] = (int16_t)(((rand()%500)+500)*OOBMUX);
            flight->planeloc[2] = (int16_t)(((rand()%900)+2000)*OOBMUX);
            flight->hpr[0] = 2061;
            flight->hpr[1] = 190;
            flight->hpr[2] = 0;
            flight->myHealth = 100;
            flight->killedByBooletID = 0;
        }
        else
        {
            flight->planeloc[0] = (int16_t)(24*48*OOBMUX);
            flight->planeloc[1] = (int16_t)(18*48*OOBMUX); //Start pos * 48 since 48 is the fixed scale.
            flight->planeloc[2] = (int16_t)(60*48*OOBMUX);    
            flight->hpr[0] = 2061;
            flight->hpr[1] = 190;
            flight->hpr[2] = 0;
            flight->myHealth = 0; // Not used in regular mode.
        }
        flight->planeloc_fine[0] = flight->planeloc[0] << FLIGHT_SPEED_DEC;
        flight->planeloc_fine[1] = flight->planeloc[1] << FLIGHT_SPEED_DEC;
        flight->planeloc_fine[2] = flight->planeloc[2] << FLIGHT_SPEED_DEC;

        flight->pitchmoment = 0;
        flight->yawmoment = 0;

        memset(flight->beangotmask, 0, sizeof( flight->beangotmask) );

        speedyLine(disp, 203, 207, 410, 430, _RGB(128,95,255));
    }

    void flightUpdate(void* arg __attribute__((unused)))
    {
        Display * disp = flight->disp;
        static const char* const EnglishNumberSuffix[] = { "st", "nd", "rd", "th" };
    }

    void flightRender(int64_t elapsedUs __attribute__((unused)))
    {
        flightUpdate( 0 ); // just draws the melee menu, skip
        flight_t * tflight = flight;
        Display * disp = tflight->disp;
        tflight->tframes++;
        if( tflight->mode != FLIGHT_GAME && tflight->mode != FLIGHT_GAME_OVER && tflight->mode != FLIGHT_FREEFLIGHT ) return;

        SetupMatrix();

        uint32_t now = esp_timer_get_time();

        tdRotateEA( flight->ProjectionMatrix, tflight->hpr[1]/11, tflight->hpr[0]/11, 0 );
        tdTranslate( flight->ModelviewMatrix, -tflight->planeloc[0], -tflight->planeloc[1], -tflight->planeloc[2] );

        modelRangePair_t * mrp = tflight->mrp;
        modelRangePair_t * mrptr = mrp;

/////////////////////////////////////////////////////////////////////////////////////////
////GAME LOGIC GOES HERE (FOR COLLISIONS/////////////////////////////////////////////////

        int i;
        for( i = 0; i < tflight->enviromodels;i++ )
        {
            const tdModel * m = tflight->environment[i];

            int label = m->label;
            int draw = 1;

        // skipping a bunch of useless donut collision logic

        if( draw == 0 ) continue;

            int r = tdModelVisibilitycheck( m );
            if( r < 0 ) continue;
            mrptr->model = m;
            mrptr->mrange = r;
            mrptr++;
        }

        //this is never called
        //if( tflight->nNetworkMode )
        //    FlightNetworkFrameCall( tflight, disp, now, &mrptr );

        int mdlct = mrptr - mrp;

        qsort( mrp, mdlct, sizeof( modelRangePair_t ), mdlctcmp );

        for( i = 0; i < mdlct; i++ )
        {
        const tdModel * m = mrp[i].model;

        // everything after this point should get thrown into it's own library as it's mostly just boilerplate matrix math
        if( (intptr_t)m < 0x3000 )
        {
            // It's a special thing.  Don't draw the normal way.
            //0x0000 - 0x1000 = Boolets.
            if( (intptr_t)m < 0x800 ) TModOrDrawBoolet( tflight, 0, 0, &tflight->allBoolets[(intptr_t)m], now );
            else if( (intptr_t)m < 0x1000 ) TModOrDrawBoolet( tflight, 0, 0, &tflight->myBoolets[(intptr_t)m-0x800], now );
            else if( (intptr_t)m < 0x2000 ) TModOrDrawPlayer( tflight, 0, 0, &tflight->allPeers[((intptr_t)m)-0x1000], now );
            else TModOrDrawCustomNetModel( tflight, 0, 0, tflight->networkModels[((intptr_t)m) - 0x2000], now );
        }
        //skip a bunch of ELSE code about flashing donut bullshit

        if( flight->mode == FLIGHT_GAME || tflight->mode == FLIGHT_FREEFLIGHT )
        {
            char framesStr[32] = {0};
            int16_t width;
        }
        }
        speedyLine(disp, 403, 407, 210, 230, _RGB(198,195,8));
    }


    flightEnterMode(disp);
    flightStartGame(FLIGHT_FREEFLIGHT);
    flightRender(0);
}

void main(){
    /* X windows set up. */
    Display *disp = XOpenDisplay(0);
    win = RootWindow(disp, 0);
    gc = XCreateGC(disp, win, 0, 0);
    XSetForeground(disp, gc, _RGB(255,255,255));
    win = XCreateSimpleWindow(disp, win, 0, 0, TFT_WIDTH, TFT_HEIGHT, 0, 0, _RGB(25.5,51,76.5));
    XSelectInput(disp, win, KeyPressMask);
    XMapWindow(disp, win);

    /*Load map files from stdin into arrays. */
    for (; scanf("%lf%lf%lf", worldX + num_pts, worldY + num_pts, worldZ + num_pts) + 1;
         num_pts++);

    /* Infinite loop, though plane can drop and ruin the calculations. */
    for (;;)
    {
        /* Sleep */

        /*dt is 0.02. timeval is in bits/time.h and gives secs and usecs.
        This becomes 0.02 * 1000000 = 20000 usecs = 0.02 seconds.
        Need it here to reset it since the select call wipes it out. */

        struct timeval sleeptime = {0, dt * 1e6};
        select(0, 0, 0, 0, &sleeptime);
       
        F += timeDelta * P;
        
        /* Update the display. */

        XClearWindow(disp, win);

        /*Loop over points and draw lines.*/
        XDrawLine(disp, win, gc, 30, 30, 100, 100); //https://github.com/QMonkey/Xlib-demo/blob/master/src/color-drawing.c
        XDrawString(disp, win, gc, 20, 460, infoStr, 17);
        speedyLine(disp, 9, 10, 110, 120, _RGB(255,0,0));
        
        // Draw crosshairs.
        int centerx = TFT_WIDTH/2;
        int centery = TFT_HEIGHT/2;
        speedyLine(disp, centerx-4, centery, centerx+4, centery, CROSSHAIR_COLOR ); //draw crosshair
        speedyLine(disp, centerx, centery-4, centerx, centery+4, CROSSHAIR_COLOR ); //draw crosshair

        // future flight.c code goes here

        flightc_shim(disp);

        /*Get key press.*/

        for (; XPending(disp);)

        {

            XEvent z;
            XNextEvent(disp, &z);

            N = XLookupKeysym(&z.xkey, 0);
            switch (N)

            {
            case Up:
                ++up_down;
                break;

            case Left:
                ++left_right;
                break;

            case Throttle_Up:
                ++speed;
                break;

            case Right:
                --left_right;
                break;

            case Throttle_Down:
                --speed;
                break;

            case Down:
                --up_down;
                break;

            case Enter:
                left_right = 0;
                break; /* re-center from turning */
            }
        }

       sprintf(infoStr, "flightsim");
       sprintf(flightStr, "flightStr");

    }
}