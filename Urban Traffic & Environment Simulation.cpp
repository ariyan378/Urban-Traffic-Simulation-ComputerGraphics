git init#include <GL/glut.h>
#include <cmath>
#include <cstdlib>

bool showWelcome = true; // welcome screen
// ---------------------------------------------------------------------
// LANE LAYOUT
// Each traffic group gets its own lane (own y value) so cars never
// occupy the same vertical space. Same-direction cars in a lane also
// move at the SAME speed and wrap using the SAME range, so the gap
// between them never changes (no catching up / passing through).
// ---------------------------------------------------------------------
const float LANE_MIN = -1300.0f;
const float LANE_MAX = 1300.0f;
const float LANE_RANGE = LANE_MAX - LANE_MIN; // 2600

const float LANE1_Y = -140.0f; // rightward traffic (car1, car3)
const float LANE2_Y = -230.0f; // leftward traffic (car2)
const float LANE3_Y = -320.0f; // player-controlled car

const float TRAFFIC_SPEED = 3.0f;              // shared by all lane-1 vehicles to keep spacing fixed
const float LANE1_SPACING = LANE_RANGE / 3.0f; // even 3-way spacing between car1, car3, bus

float car1 = LANE_MIN;
float car2 = 900.0f;
float car3 = LANE_MIN + LANE1_SPACING;

float rainY = 0;

int signal = 0;
// 0 = Green
// 1 = Yellow
// 2 = Red
float cloudMove = -1000;
bool dayMode = true;
bool rainMode = false;
bool carMove = true;

float zoom = 1.0f;
float carPosition = -800.0f;

// Precomputed so they don't flicker every frame
float buildingHeights[10];
float starPositions[120][2];

// Each building gets its own color, window tint, and rooftop detail
float buildingColors[10][3];
float buildingWindowColors[10][3];
int buildingRoofType[10]; // 0 = plain, 1 = antenna, 2 = water tank
bool blinkOn = true;      // drives the antenna warning-light blink

// ---------------------------------------------------------------------
// REDESIGN FEATURE 1: distinct building archetypes for skyline variety
// ---------------------------------------------------------------------
enum BuildingType {
    BLD_SKYSCRAPER = 0,
    BLD_OFFICE_TOWER,
    BLD_APARTMENT,
    BLD_HOTEL,
    BLD_HOSPITAL,
    BLD_SCHOOL,
    BLD_MALL,
    BLD_BANK,
    BLD_LIBRARY,
    BLD_POLICE
};
int buildingType[10]; // one archetype per slot, assigned in init()

float personX = -900;
float busX = LANE_MIN + 2 * LANE1_SPACING; // third synced vehicle in lane 1

// ---------------------------------------------------------------------
// NEW: PEDESTRIAN CROSSING (this pass)
// The user's actual point: the stop line only matters if it leaves the
// crosswalk clear for people to use. Now that cars/bus reliably stop
// short of the stripes, add pedestrians who actually walk across on
// them -- but only start a crossing once the light is RED (traffic
// stopped). If a crossing is already underway when the light changes,
// they're allowed to finish it rather than being stranded mid-road.
// ---------------------------------------------------------------------
enum CrossState { CROSS_WAIT_NEAR = 0, CROSS_GOING_FAR, CROSS_WAIT_FAR, CROSS_GOING_NEAR };
float crossPedY[2]     = { -70.0f, -355.0f }; // start on opposite sides for variety
int   crossPedState[2] = { CROSS_WAIT_NEAR, CROSS_WAIT_FAR };
const float CROSS_NEAR_Y = -70.0f;  // sidewalk side
const float CROSS_FAR_Y  = -355.0f; // far side of the road, against the embankment
const float CROSS_SPEED  = 1.4f;
float birdX = -1000;
float planeX = -1500;

// Lake / boat - placed in the strip below the road (y from -500 to -360)
// which was previously blank/unrendered background (sky) color.
// A stone embankment wall + railing separates the road from the lake.
const float WALL_TOP = -360.0f;    // meets the bottom of the road
const float WALL_BOTTOM = -388.0f; // wall is 28 units tall
const float LAKE_TOP = WALL_BOTTOM; // lake starts right under the wall
const float LAKE_BOTTOM = -500.0f;
const float LAKE_CY = (LAKE_TOP + LAKE_BOTTOM) * 0.5f;
float waterPhase = 0.0f; // drives ripple animation and the fountain jets

// ---------------------------------------------------------------------
// NEW: MARKET AREA (Feature 1 - original numbering)
// ---------------------------------------------------------------------
const int   MARKET_SHOP_COUNT = 4;
const float MARKET_START_X   = 300.0f;   // first shop's left edge
const float MARKET_SHOP_GAP  = 130.0f;   // spacing between shops
const float MARKET_Y         = 120.0f;   // sits on the grass baseline

float marketShopColors[MARKET_SHOP_COUNT][3] = {
    {0.85f, 0.35f, 0.35f},
    {0.35f, 0.55f, 0.85f},
    {0.90f, 0.65f, 0.20f},
    {0.45f, 0.75f, 0.45f}
};

const int MARKET_CUSTOMER_COUNT = 5;
float marketCustomerX[MARKET_CUSTOMER_COUNT];
float marketCustomerDir[MARKET_CUSTOMER_COUNT];
bool  marketLightsOn = true;
int   marketBlinkCount = 0;

// ---------------------------------------------------------------------
// NEW: BOAT (Feature 2 - original numbering)
// ---------------------------------------------------------------------
float boatX = -200.0f;
float boatDir = 1.0f;
float rowPhase = 0.0f; // বৈঠা বাওয়ার animation phase
const float BOAT_MIN_X = -450.0f; // stays clear of the fountain, within the lake
const float BOAT_MAX_X = 450.0f;

// ---------------------------------------------------------------------
// NEW: SUN ANIMATION (Feature 3 - original numbering)
// ---------------------------------------------------------------------
float sunAngle = 0.0f;

// ---------------------------------------------------------------------
// NEW: PEDESTRIAN WALK ANIMATION (Feature 6 - original numbering)
// ---------------------------------------------------------------------
float walkPhase = 0.0f;

// ---------------------------------------------------------------------
// NEW: TRAFFIC LIGHT STOP ZONES (Feature 7 & 8 - original numbering)
// ---------------------------------------------------------------------
const float CROSSWALK_A_X = -350.0f;
const float CROSSWALK_B_X = 350.0f;
const float STOP_ZONE = 55.0f; // legacy constant, kept for compatibility (unused by new logic below)

// ---------------------------------------------------------------------
// FEATURE 1 (this pass): Smart Traffic Control System
// ---------------------------------------------------------------------
// Proper GREEN=10s / YELLOW=3s / RED=10s cycle, timed off the existing
// 16ms glutTimerFunc tick, plus true stop-line distances (crosswalk
// half-width + vehicle half-length + margin) so vehicles brake to a
// smooth stop BEFORE the zebra crossing, never on top of it.
const int SIGNAL_GREEN_FRAMES  = 625;  // 10s @ 16ms/frame
const int SIGNAL_YELLOW_FRAMES = 187;  // 3s
const int SIGNAL_RED_FRAMES    = 625;  // 10s
int signalFrameCount = 0;

// Lane 1 carries a bus (half-length 90); since car1/car3/busX always
// move together at identical speed, using the bus's length for the
// WHOLE group's stop zone keeps every member safely clear of the
// crosswalk without ever breaking their fixed relative spacing.
const float STOP_ZONE_LANE1 = 260.0f; // braking begins this far out, well clear of the bus's own clamp boundary
const float STOP_ZONE_LANE2 = 220.0f; // same idea for lane 2 (car2)

// BUG FIX: this used to be a 4-unit margin, which put the vehicle's
// front bumper close enough to the zebra stripes that it visually
// looked like a collision (basically touching the crossing). Bumped
// up to a clearly visible real-world-style gap so cars/bus always
// stop with daylight between their front and the crosswalk.
const float STOP_LINE_MARGIN = 26.0f;

// Eased 0..1 speed multipliers, shared per lane so LANE1_SPACING never changes
float lane1SpeedFactor = 1.0f;
float lane2SpeedFactor = 1.0f;

// ---------------------------------------------------------------------
// NEW: RIVER FISH JUMP (Feature 12 - original numbering)
// ---------------------------------------------------------------------
float fishTimer = 0.0f;
float fishX = -80.0f;
bool  fishJumping = false;

void display();
void keyboard(unsigned char key, int x, int y);
void mouse(int button, int state, int x, int y);
void timer(int);

// forward declarations for new functions
void circle(float x, float y, float r);
void initMarket();
void drawMarket();
void updateMarket();
void drawBoat();
void updateBoat();
void drawSun(float x, float y, float r);
void updateSun();
void drawFootpathBoundary();
void drawDustbin(float x, float y);
void drawRoadSign(float x, float y);
void drawBusStop(float x, float y);
void drawBikeStand(float x, float y);
void drawFireHydrant(float x, float y);
void drawMailbox(float x, float y);
void updateWalkAnimation();
void updateTraffic();
void drawDecorations();
void updateDecorations();
void drawFish();
void updateFish();
void drawCrossingPedestrians();
void updateCrossingPedestrians();

void init()
{
    glClearColor(0.53f, 0.81f, 0.98f, 1.0f);

    // VISUAL POLISH: needed so drawGroundShadow()'s translucent alpha
    // actually blends instead of drawing solid black ellipses.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-1000, 1000, -500, 500);

    // A varied palette of attractive city-building colors
    // (kept for reference; per-archetype colors below are what's actually used)
    float palette[8][3] = {
        {0.35f, 0.40f, 0.55f}, // slate blue
        {0.55f, 0.35f, 0.40f}, // brick plum
        {0.30f, 0.45f, 0.48f}, // teal charcoal
        {0.50f, 0.45f, 0.35f}, // warm sandstone
        {0.40f, 0.40f, 0.45f}, // cool gray
        {0.45f, 0.30f, 0.50f}, // muted violet
        {0.35f, 0.50f, 0.45f}, // sage green
        {0.55f, 0.42f, 0.30f}, // terracotta
    };
    (void)palette; // silence unused-variable warning; kept as a reference palette

    // Deterministic archetype order gives a believable skyline silhouette:
    // tallest commercial core in the middle, civic buildings toward the edges.
    int typeOrder[10] = {
        BLD_APARTMENT, BLD_SCHOOL, BLD_OFFICE_TOWER, BLD_SKYSCRAPER, BLD_HOTEL,
        BLD_HOSPITAL,  BLD_BANK,   BLD_MALL,          BLD_LIBRARY,   BLD_POLICE
    };

    // Base height per archetype (jittered slightly so no two look identical)
    float typeBaseHeight[10] = {
    220, 170, 320, 200, 200,
    240, 210, 150, 190, 160
};

    // Signature color per archetype (glass/stone/brick tones, not random)
    float typeColor[10][3] = {
        {0.55f, 0.42f, 0.34f}, // apartment - warm brick
        {0.72f, 0.35f, 0.28f}, // school - brick red
        {0.30f, 0.42f, 0.55f}, // office tower - cool glass blue
        {0.25f, 0.35f, 0.50f}, // skyscraper - deep glass blue
        {0.45f, 0.32f, 0.48f}, // hotel - muted violet glass
        {0.92f, 0.92f, 0.88f}, // hospital - clean white/cream
        {0.85f, 0.55f, 0.20f}, // mall - bright amber front
        {0.42f, 0.42f, 0.46f}, // bank - stone grey
        {0.70f, 0.58f, 0.38f}, // library - sandstone
        {0.30f, 0.38f, 0.50f}, // police - blue-grey
    };

    for (int i = 0; i < 10; i++)
    {
        buildingType[i] = typeOrder[i];
        buildingHeights[i] = typeBaseHeight[i] + (rand() % 30); // small natural jitter

        buildingColors[i][0] = typeColor[i][0];
        buildingColors[i][1] = typeColor[i][1];
        buildingColors[i][2] = typeColor[i][2];

        // Glass-heavy archetypes (tower/skyscraper/hotel/bank) glow cool cyan
        // at night; brick/civic buildings glow warm yellow.
        bool coolGlass = (buildingType[i] == BLD_OFFICE_TOWER ||
                           buildingType[i] == BLD_SKYSCRAPER   ||
                           buildingType[i] == BLD_HOTEL        ||
                           buildingType[i] == BLD_BANK);
        if (coolGlass)
        {
            buildingWindowColors[i][0] = 0.5f;
            buildingWindowColors[i][1] = 0.9f;
            buildingWindowColors[i][2] = 1.0f;
        }
        else
        {
            buildingWindowColors[i][0] = 1.0f;
            buildingWindowColors[i][1] = 1.0f;
            buildingWindowColors[i][2] = 0.4f;
        }

        buildingRoofType[i] = rand() % 3; // still used by skyscraper roof beacon
    }

    for (int i = 0; i < 120; i++)
    {
        starPositions[i][0] = (float)(rand() % 2000 - 1000);
        starPositions[i][1] = (float)(rand() % 300 + 150);
    }

    initMarket(); // NEW: set up market customer starting positions
}
void drawSkyGradient()
{
    float topR, topG, topB, botR, botG, botB;

    if (rainMode && dayMode)
    {
        // - dull sky(rain-time)
        topR = 0.30f; topG = 0.33f; topB = 0.38f;
        botR = 0.55f; botG = 0.58f; botB = 0.62f;
    }
    else if (dayMode)
    {
        // Deep sky blue up high, softening toward a pale horizon
        topR = 0.30f; topG = 0.58f; topB = 0.90f;
        botR = 0.75f; botG = 0.90f; botB = 0.99f;
    }
    else
    {
        // Deep night navy up high, faint indigo glow near the horizon
        topR = 0.02f; topG = 0.02f; topB = 0.10f;
        botR = 0.10f; botG = 0.10f; botB = 0.24f;
    }

    glBegin(GL_QUADS);
    glColor3f(topR, topG, topB);
    glVertex2f(-1000, 500);
    glVertex2f(1000, 500);
    glColor3f(botR, botG, botB);
    glVertex2f(1000, 120);
    glVertex2f(-1000, 120);
    glEnd();
}
void drawBackground()
{
    if (rainMode)
    {
        if (dayMode)
            glClearColor(0.35f, 0.4f, 0.45f, 1);
        else
            glClearColor(0.02f, 0.02f, 0.06f, 1);  // dark sky
    }
    else if (dayMode)
        glClearColor(0.53f, 0.81f, 0.98f, 1);
    else
        glClearColor(0.03f, 0.03f, 0.12f, 1);

    glClear(GL_COLOR_BUFFER_BIT);
}


// ---------------------------------------------------------------------
// VISUAL POLISH: smooth sky gradient (replaces the flat single-color
// sky). Drawn as one big quad with per-vertex colors so OpenGL
// interpolates it for us -- cheap, and it immediately makes the scene
// look far less "flat shapes on a solid background".
// ---------------------------------------------------------------------

// ---------------------------------------------------------------------
// VISUAL POLISH: soft ground-contact shadow. A flattened, semi-dark
// ellipse drawn at an object's base gives it visual weight and grounds
// it to the scene instead of looking like it's floating on flat grass.
// ---------------------------------------------------------------------
void drawGroundShadow(float x, float y, float rx, float ry)
{
    glColor4f(0, 0, 0, 0.18f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i = 0; i <= 24; i++)
    {
        float a = 2 * 3.1416f * i / 24;
        glVertex2f(x + rx * cos(a), y + ry * sin(a));
    }
    glEnd();
}

void circle(float x, float y, float r)
{
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);

    for (int i = 0; i <= 100; i++)
    {
        float angle = 2 * 3.1416f * i / 100;
        glVertex2f(x + r * cos(angle), y + r * sin(angle));
    }

    glEnd();
}

void drawGrass()
{
    glColor3f(0.1f, 0.6f, 0.1f);

    glBegin(GL_QUADS);
    glVertex2f(-1000, -100);
    glVertex2f(1000, -100);
    glVertex2f(1000, 120);
    glVertex2f(-1000, 120);
    glEnd();
}

// ---------------------------------------------------------------------
// REDESIGN FEATURE 1: shared window-grid helper + per-archetype details
// ---------------------------------------------------------------------
void drawWindowGrid(float x, float top, float h, int cols, float colSpacing,
                     float winW, float winH, float rowSpacing,
                     float wr, float wg, float wb)
{
    glColor3f(wr, wg, wb);
    int maxRows = (int)((h - 45.0f) / rowSpacing) + 1;
    if (maxRows > 7) maxRows = 7;
    if (maxRows < 1) maxRows = 1;

    for (int r = 0; r < maxRows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            float wx = x + 15 + c * colSpacing;
            float wy = 150 + r * rowSpacing;
            if (wy + winH > top - 6) continue;
            glBegin(GL_QUADS);
            glVertex2f(wx, wy);
            glVertex2f(wx + winW, wy);
            glVertex2f(wx + winW, wy + winH);
            glVertex2f(wx, wy + winH);
            glEnd();
        }
    }
}

// Tall glass tower, flat roof, blinking antenna beacon
void drawSkyscraperBuilding(float x, float h, float cr, float cg, float cb,
                             float wr, float wg, float wb, int roofType)
{
    float top = 120 + h;
    glColor3f(cr, cg, cb);
    glBegin(GL_QUADS);
    glVertex2f(x, 120); glVertex2f(x + 120, 120);
    glVertex2f(x + 120, top); glVertex2f(x, top);
    glEnd();

    // Vertical glass-panel seams for a curtain-wall look
    glColor3f(cr * 0.6f, cg * 0.6f, cb * 0.6f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (float lx = x + 20; lx < x + 120; lx += 20)
    { glVertex2f(lx, 120); glVertex2f(lx, top); }
    glEnd();

    drawWindowGrid(x, top, h, 4, 25, 14, 12, 32, wr, wg, wb);

    // Antenna with blinking beacon (reuses your existing blinkOn variable)
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_LINES);
    glVertex2f(x + 60, top); glVertex2f(x + 60, top + 45);
    glEnd();
    if (!dayMode && blinkOn) { glColor3f(1, 0.1f, 0.1f); circle(x + 60, top + 47, 4); }
}

// Mid-rise office tower, flat roof, small rooftop AC units
void drawOfficeTowerBuilding(float x, float h, float cr, float cg, float cb,
                              float wr, float wg, float wb)
{
    float top = 120 + h;
    glColor3f(cr, cg, cb);
    glBegin(GL_QUADS);
    glVertex2f(x, 120); glVertex2f(x + 120, 120);
    glVertex2f(x + 120, top); glVertex2f(x, top);
    glEnd();

    drawWindowGrid(x, top, h, 3, 32, 16, 14, 34, wr, wg, wb);

    // Rooftop AC/mechanical units
    glColor3f(0.35f, 0.35f, 0.35f);
    glBegin(GL_QUADS);
    glVertex2f(x + 20, top); glVertex2f(x + 45, top);
    glVertex2f(x + 45, top + 12); glVertex2f(x + 20, top + 12);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(x + 70, top); glVertex2f(x + 95, top);
    glVertex2f(x + 95, top + 12); glVertex2f(x + 70, top + 12);
    glEnd();
}

// Apartment block: warm brick, a balcony ledge on every floor
void drawApartmentBuilding(float x, float h, float cr, float cg, float cb,
                            float wr, float wg, float wb)
{
    float top = 120 + h;
    glColor3f(cr, cg, cb);
    glBegin(GL_QUADS);
    glVertex2f(x, 120); glVertex2f(x + 120, 120);
    glVertex2f(x + 120, top); glVertex2f(x, top);
    glEnd();

    drawWindowGrid(x, top, h, 3, 30, 15, 13, 36, wr, wg, wb);

    // Balcony ledges under each window row
    glColor3f(cr * 0.75f, cg * 0.75f, cb * 0.75f);
    int rows = (int)((h - 45.0f) / 36.0f) + 1;
    if (rows > 7) rows = 7;
    for (int r = 0; r < rows; r++)
    {
        float wy = 150 + r * 36 - 4;
        if (wy + 4 > top - 6) continue;
        glBegin(GL_QUADS);
        glVertex2f(x + 10, wy); glVertex2f(x + 110, wy);
        glVertex2f(x + 110, wy + 3); glVertex2f(x + 10, wy + 3);
        glEnd();
    }
}

// Hotel: glass tower + glowing rooftop sign box (blank, no text)
void drawHotelBuilding(float x, float h, float cr, float cg, float cb,
                        float wr, float wg, float wb)
{
    float top = 120 + h;
    glColor3f(cr, cg, cb);
    glBegin(GL_QUADS);
    glVertex2f(x, 120); glVertex2f(x + 120, 120);
    glVertex2f(x + 120, top); glVertex2f(x, top);
    glEnd();

    drawWindowGrid(x, top, h, 4, 25, 14, 12, 32, wr, wg, wb);

    // Rooftop sign box - lit at night, blank by day
    if (!dayMode) glColor3f(1.0f, 0.75f, 0.3f);
    else glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(x + 25, top + 4); glVertex2f(x + 95, top + 4);
    glVertex2f(x + 95, top + 20); glVertex2f(x + 25, top + 20);
    glEnd();
}

// Hospital: low/cream, flat roof, red cross sign
void drawHospitalBuilding(float x, float h, float cr, float cg, float cb)
{
    float top = 120 + h;
    glColor3f(cr, cg, cb);
    glBegin(GL_QUADS);
    glVertex2f(x, 120); glVertex2f(x + 120, 120);
    glVertex2f(x + 120, top); glVertex2f(x, top);
    glEnd();

    // Blue-tinted, evenly spaced clinical windows
    drawWindowGrid(x, top, h, 3, 32, 16, 14, 34, 0.55f, 0.8f, 0.95f);

    // Red cross, lit red at night
    float rr = dayMode ? 0.85f : 1.0f;
    glColor3f(rr, 0.1f, 0.1f);
    glBegin(GL_QUADS);
    glVertex2f(x + 52, top + 8); glVertex2f(x + 68, top + 8);
    glVertex2f(x + 68, top + 28); glVertex2f(x + 52, top + 28);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(x + 44, top + 14); glVertex2f(x + 76, top + 14);
    glVertex2f(x + 76, top + 22); glVertex2f(x + 44, top + 22);
    glEnd();
}

// School: low brick building, flagpole with small flag on the roof
void drawSchoolBuilding(float x, float h, float cr, float cg, float cb,
                         float wr, float wg, float wb)
{
    float top = 120 + h;
    glColor3f(cr, cg, cb);
    glBegin(GL_QUADS);
    glVertex2f(x, 120); glVertex2f(x + 120, 120);
    glVertex2f(x + 120, top); glVertex2f(x, top);
    glEnd();

    drawWindowGrid(x, top, h, 4, 25, 15, 15, 30, wr, wg, wb);

    // Flagpole + flag
    // Flagpole + Bangladesh Flag
glColor3f(0.3f, 0.3f, 0.3f);
glBegin(GL_LINES);
glVertex2f(x + 60, top); glVertex2f(x + 60, top + 30);
glEnd();

// Green Flag
glColor3f(0.0f, 0.4f, 0.0f);
glBegin(GL_QUADS);
glVertex2f(x + 60, top + 30);
glVertex2f(x + 80, top + 29);
glVertex2f(x + 80, top + 19);
glVertex2f(x + 60, top + 20);
glEnd();

glColor3f(0.85f, 0.05f, 0.1f);
circle(x + 68.0f, top + 24.5f, 3.5f);
}

// Mall: low & wide-reading front with a bright striped awning
void drawMallBuilding(float x, float h, float cr, float cg, float cb)
{
    float top = 120 + h;
    glColor3f(0.85f, 0.85f, 0.88f); // neutral body so the awning pops
    glBegin(GL_QUADS);
    glVertex2f(x, 120); glVertex2f(x + 120, 120);
    glVertex2f(x + 120, top); glVertex2f(x, top);
    glEnd();

    // Large storefront glass band at street level
    glColor3f(0.55f, 0.75f, 0.9f);
    glBegin(GL_QUADS);
    glVertex2f(x + 5, 122); glVertex2f(x + 115, 122);
    glVertex2f(x + 115, 155); glVertex2f(x + 5, 155);
    glEnd();

    drawWindowGrid(x, top, h, 4, 25, 14, 12, 32, 1.0f, 1.0f, 0.5f);

    // Striped awning over the entrance
    for (int i = 0; i < 5; i++)
    {
        if (i % 2 == 0) glColor3f(cr, cg, cb); else glColor3f(1, 1, 1);
        glBegin(GL_TRIANGLES);
        glVertex2f(x + 20 + i * 16.0f, 122);
        glVertex2f(x + 28 + i * 16.0f, 122);
        glVertex2f(x + 24 + i * 16.0f, 110);
        glEnd();
    }
}

// Bank: stone grey with entrance columns
void drawBankBuilding(float x, float h, float cr, float cg, float cb,
                       float wr, float wg, float wb)
{
    float top = 120 + h;
    glColor3f(cr, cg, cb);
    glBegin(GL_QUADS);
    glVertex2f(x, 120); glVertex2f(x + 120, 120);
    glVertex2f(x + 120, top); glVertex2f(x, top);
    glEnd();

    drawWindowGrid(x, top, h, 3, 32, 16, 14, 34, wr, wg, wb);

    // Entrance columns
    glColor3f(0.85f, 0.85f, 0.82f);
    for (int i = 0; i < 4; i++)
    {
        float cx = x + 20 + i * 22.0f;
        glBegin(GL_QUADS);
        glVertex2f(cx, 120); glVertex2f(cx + 6, 120);
        glVertex2f(cx + 6, 150); glVertex2f(cx, 150);
        glEnd();
    }
}

// Library: sandstone with arched top windows (approximated fan shape)
void drawLibraryBuilding(float x, float h, float cr, float cg, float cb)
{
    float top = 120 + h;
    glColor3f(cr, cg, cb);
    glBegin(GL_QUADS);
    glVertex2f(x, 120); glVertex2f(x + 120, 120);
    glVertex2f(x + 120, top); glVertex2f(x, top);
    glEnd();

    // Arched windows: a rectangle capped with a fan (arch) top
    glColor3f(1.0f, 0.95f, 0.75f);
    for (int i = 0; i < 3; i++)
    {
        float wx = x + 20 + i * 32.0f;
        float wy = 150.0f;
        glBegin(GL_QUADS);
        glVertex2f(wx, wy); glVertex2f(wx + 18, wy);
        glVertex2f(wx + 18, wy + 30); glVertex2f(wx, wy + 30);
        glEnd();
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(wx + 9, wy + 30);
        for (int a = 0; a <= 10; a++)
        {
            float ang = 3.1416f * a / 10.0f;
            glVertex2f(wx + 9 - 9 * cos(ang), wy + 30 + 9 * sin(ang));
        }
        glEnd();
    }
}

// Police station: low, blue-grey, rooftop light bar blinking blue/red
void drawPoliceBuilding(float x, float h, float cr, float cg, float cb,
                         float wr, float wg, float wb)
{
    float top = 120 + h;
    glColor3f(cr, cg, cb);
    glBegin(GL_QUADS);
    glVertex2f(x, 120); glVertex2f(x + 120, 120);
    glVertex2f(x + 120, top); glVertex2f(x, top);
    glEnd();

    drawWindowGrid(x, top, h, 3, 32, 16, 14, 34, wr, wg, wb);

    // Rooftop light bar, alternates blue/red using existing blinkOn
    if (!dayMode)
    {
        glColor3f(blinkOn ? 0.2f : 0.9f, 0.2f, blinkOn ? 0.9f : 0.2f);
        glBegin(GL_QUADS);
        glVertex2f(x + 45, top + 2); glVertex2f(x + 75, top + 2);
        glVertex2f(x + 75, top + 10); glVertex2f(x + 45, top + 10);
        glEnd();
    }
}

void drawBuildings()
{
    float x = -900;

    for (int i = 0; i < 10; i++)
    {
        float h = buildingHeights[i];
        float cr = buildingColors[i][0], cg = buildingColors[i][1], cb = buildingColors[i][2];
        float wr = buildingWindowColors[i][0], wg = buildingWindowColors[i][1], wb = buildingWindowColors[i][2];

        switch (buildingType[i])
        {
        case BLD_SKYSCRAPER:   drawSkyscraperBuilding(x, h, cr, cg, cb, wr, wg, wb, buildingRoofType[i]); break;
        case BLD_OFFICE_TOWER: drawOfficeTowerBuilding(x, h, cr, cg, cb, wr, wg, wb); break;
        case BLD_APARTMENT:    drawApartmentBuilding(x, h, cr, cg, cb, wr, wg, wb); break;
        case BLD_HOTEL:        drawHotelBuilding(x, h, cr, cg, cb, wr, wg, wb); break;
        case BLD_HOSPITAL:     drawHospitalBuilding(x, h, cr, cg, cb); break;
        case BLD_SCHOOL:       drawSchoolBuilding(x, h, cr, cg, cb, wr, wg, wb); break;
        case BLD_MALL:         drawMallBuilding(x, h, cr, cg, cb); break;
        case BLD_BANK:         drawBankBuilding(x, h, cr, cg, cb, wr, wg, wb); break;
        case BLD_LIBRARY:      drawLibraryBuilding(x, h, cr, cg, cb); break;
        case BLD_POLICE:       drawPoliceBuilding(x, h, cr, cg, cb, wr, wg, wb); break;
        }

        x += 180;
    }
}

void drawTree(float x, float y)
{
    glColor3f(0.5f, 0.25f, 0);

    glBegin(GL_QUADS);
    glVertex2f(x - 8, y);
    glVertex2f(x + 8, y);
    glVertex2f(x + 8, y + 50);
    glVertex2f(x - 8, y + 50);
    glEnd();

    glColor3f(0, 0.6f, 0);
    circle(x, y + 75, 30);
    circle(x - 20, y + 60, 25);
    circle(x + 20, y + 60, 25);
}

void drawCloud(float x, float y)
{
    if (rainMode) return; // no cloud in rain

    glColor3f(1, 1, 1);
    circle(x, y, 25);
    circle(x + 25, y + 10, 30);
    circle(x + 55, y, 25);
    circle(x + 20, y - 10, 22);
}

// ---------------------------------------------------------------------
// (Feature 3, original numbering): sun with rotating rays. Disc + rays
// drawn behind it. Called from drawSkyObjects() below instead of a
// plain circle().
// ---------------------------------------------------------------------
void drawSun(float x, float y, float r)
{
    glColor3f(1.0f, 0.85f, 0.2f);
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(sunAngle, 0, 0, 1);

    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 12; i++)
    {
        float a1 = i * (360.0f / 12.0f) * 3.1416f / 180.0f;
        float a2 = a1 + 8.0f * 3.1416f / 180.0f;
        glVertex2f((r + 5) * cos(a1), (r + 5) * sin(a1));
        glVertex2f((r + 25) * cos((a1 + a2) / 2), (r + 25) * sin((a1 + a2) / 2));
        glVertex2f((r + 5) * cos(a2), (r + 5) * sin(a2));
    }
    glEnd();
    glPopMatrix();

    glColor3f(1, 0.9f, 0);
    circle(x, y, r);
}

void drawSkyObjects()
{
    // Sun/moon should be hidden behind rain clouds while it's raining.
    if (rainMode)
        return;

    if (dayMode)
    {
        // NEW: soft glow halo behind the sun for a warmer look
        glColor4f(1.0f, 0.9f, 0.5f, 0.25f);
        circle(750, 410, 65);

        drawSun(750, 410, 45); // মেঘের ব্যান্ড (280-340) থেকে স্পষ্ট উপরে, সংঘর্ষ এড়ানো
    }
    else
    {
        // Crescent moon: bright yellow full circle, then an offset
        // circle in the night-sky color drawn on top to "eat away"
        // part of it, leaving a crescent sliver. No halo behind it.
        glColor3f(1.0f, 0.85f, 0.2f);
        circle(750, 410, 40);

        // Cutting circle - same color as the night sky background,
        // shifted up-right so the crescent opens toward the lower-left
        glColor3f(0.03f, 0.03f, 0.12f);
        circle(762, 420, 36);
    }
}
void drawTrafficLight(float x, float y)
{
    // Pole
    glColor3f(0.2f, 0.2f, 0.2f);

    glBegin(GL_QUADS);
    glVertex2f(x - 5, y);
    glVertex2f(x + 5, y);
    glVertex2f(x + 5, y + 120);
    glVertex2f(x - 5, y + 120);
    glEnd();

    // Box
    glColor3f(0.1f, 0.1f, 0.1f);

    glBegin(GL_QUADS);
    glVertex2f(x - 20, y + 120);
    glVertex2f(x + 20, y + 120);
    glVertex2f(x + 20, y + 190);
    glVertex2f(x - 20, y + 190);
    glEnd();

    // RED
    if (signal == 2) glColor3f(1, 0, 0);
    else glColor3f(0.3f, 0, 0);
    circle(x, y + 175, 8);

    // YELLOW
    if (signal == 1) glColor3f(1, 1, 0);
    else glColor3f(0.3f, 0.3f, 0);
    circle(x, y + 155, 8);

    // GREEN
    if (signal == 0) glColor3f(0, 1, 0);
    else glColor3f(0, 0.3f, 0);
    circle(x, y + 135, 8);
}

void drawCar(float x, float y, float r, float g, float b)
{
    glPushMatrix();
    glTranslatef(x, y, 0);

    // Body
    glColor3f(r, g, b);

    glBegin(GL_QUADS);
    glVertex2f(-50, 0);
    glVertex2f(50, 0);
    glVertex2f(50, 35);
    glVertex2f(-50, 35);
    glEnd();

    // Roof
    glBegin(GL_POLYGON);
    glVertex2f(-30, 35);
    glVertex2f(25, 35);
    glVertex2f(10, 60);
    glVertex2f(-20, 60);
    glEnd();

    // Windows
    glColor3f(0.7f, 0.9f, 1);

    glBegin(GL_QUADS);
    glVertex2f(-18, 38);
    glVertex2f(5, 38);
    glVertex2f(0, 55);
    glVertex2f(-15, 55);
    glEnd();

    // Wheels
    glColor3f(0, 0, 0);
    circle(-30, -5, 10);
    circle(30, -5, 10);

    glPopMatrix();
}

void drawRain()
{
    if (!rainMode)
        return;

    glColor3f(0.8f, 0.9f, 1.0f);
    glLineWidth(1.0f);

    glBegin(GL_LINES);

    for (int x = -1000; x < 1000; x += 55)
    {
        for (int y = -500; y < 545; y += 70)
        {
            // rainY (0..70) shifts each drop straight DOWN each frame,
            // wrapping seamlessly since it matches the 70px cell spacing.
            // A small x-offset gives a gentle wind-blown slant.
            float dropY = y - rainY;
            float windX = x - 5;

            glVertex2f(x, dropY);
            glVertex2f(windX, dropY - 12);
        }
    }

    glEnd();
}

void drawStreetLight(float x, float y)
{
    // Pole
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(x - 3, y);
    glVertex2f(x + 3, y);
    glVertex2f(x + 3, y + 120);
    glVertex2f(x - 3, y + 120);
    glEnd();

    // Curved arm reaching toward the road
    glColor3f(0.15f, 0.15f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(x, y + 120);
    glVertex2f(x + 25, y + 120);
    glVertex2f(x + 25, y + 110);
    glVertex2f(x, y + 110);
    glEnd();

    // NEW: bulb housing - drawn in both day and night now, so the lamp
    // reads as a real fixture even when unlit, not just a bare pole
    glColor3f(0.35f, 0.35f, 0.38f);
    glBegin(GL_QUADS);
    glVertex2f(x + 18, y + 100);
    glVertex2f(x + 32, y + 100);
    glVertex2f(x + 32, y + 110);
    glVertex2f(x + 18, y + 110);
    glEnd();

    if (!dayMode)
    {
        // Lit at night - warm glow
        glColor3f(1, 1, 0.6f);
        circle(x + 25, y + 100, 10);
    }
    else
    {
        // Unlit by day - dull glass/silver bulb, still visible as a fixture
        glColor3f(0.75f, 0.78f, 0.8f);
        circle(x + 25, y + 100, 7);
    }
}

// ---------------------------------------------------------------------
// (Feature 6, original numbering): pedestrian body + walking animation.
// ---------------------------------------------------------------------
void drawPerson(float x, float y)
{
    if (rainMode) return;
    float swing = 6.0f * sin(walkPhase);

    // Head
    glColor3f(1, 0.8f, 0.6f);
    circle(x, y + 46, 7);

    // Torso
    glColor3f(0.2f, 0.35f, 0.8f);
    glBegin(GL_POLYGON);
    glVertex2f(x - 7, y + 20);
    glVertex2f(x + 7, y + 20);
    glVertex2f(x + 6, y + 39);
    glVertex2f(x - 6, y + 39);
    glEnd();

    // Arms (swing opposite to legs)
    glColor3f(1, 0.8f, 0.6f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    glVertex2f(x - 6, y + 36);
    glVertex2f(x - 6 - swing * 0.5f, y + 22);
    glVertex2f(x + 6, y + 36);
    glVertex2f(x + 6 + swing * 0.5f, y + 22);
    glEnd();

    // Legs (walking swing)
    glColor3f(0.25f, 0.25f, 0.3f);
    glBegin(GL_LINES);
    glVertex2f(x, y + 20);
    glVertex2f(x - swing, y);
    glVertex2f(x, y + 20);
    glVertex2f(x + swing, y);
    glEnd();
    glLineWidth(1.0f);
}

void drawBus(float x, float y)
{
    glPushMatrix();
    glTranslatef(x, y, 0);

    glColor3f(1, 0.8f, 0);

    glBegin(GL_QUADS);
    glVertex2f(-90, 0);
    glVertex2f(90, 0);
    glVertex2f(90, 55);
    glVertex2f(-90, 55);
    glEnd();

    glColor3f(0.7f, 0.9f, 1);

    for (int i = -70; i <= 50; i += 30)
    {
        glBegin(GL_QUADS);
        glVertex2f(i, 25);
        glVertex2f(i + 20, 25);
        glVertex2f(i + 20, 45);
        glVertex2f(i, 45);
        glEnd();
    }

    glColor3f(0, 0, 0);
    circle(-55, -5, 12);
    circle(55, -5, 12);

    glPopMatrix();
}

void drawStars()
{
    if (dayMode || rainMode)
        return;

    glColor3f(1, 1, 1);
    glPointSize(2);

    glBegin(GL_POINTS);
    for (int i = 0; i < 120; i++)
        glVertex2f(starPositions[i][0], starPositions[i][1]);
    glEnd();
}

void drawMountains()
{
    glColor3f(0.35f, 0.35f, 0.35f);

    glBegin(GL_TRIANGLES);
    glVertex2f(-1000, 120);
    glVertex2f(-700, 420);
    glVertex2f(-400, 120);

    glVertex2f(-500, 120);
    glVertex2f(-150, 450);
    glVertex2f(200, 120);

    glVertex2f(100, 120);
    glVertex2f(500, 430);
    glVertex2f(900, 120);
    glEnd();
}

void drawBird()
{
    glPushMatrix();
    glTranslatef(birdX, 320, 0);

    glColor3f(0, 0, 0);

    glBegin(GL_LINE_STRIP);
    glVertex2f(0, 0);
    glVertex2f(10, 10);
    glVertex2f(20, 0);
    glVertex2f(30, 10);
    glVertex2f(40, 0);
    glEnd();

    glPopMatrix();
}

void drawPlane()
{
    // Classic side-view airplane silhouette. Nose points right
    // since planeX increases (flies left -> right).
    glPushMatrix();
    glTranslatef(planeX, 470, 0);

    // Fuselage: elongated oval body
    glColor3f(0.9f, 0.9f, 0.92f);
    glBegin(GL_POLYGON);
    for (int i = 0; i <= 40; i++)
    {
        float t = 2 * 3.1416f * i / 40;
        glVertex2f(75.0f * cos(t), 9.0f * sin(t));
    }
    glEnd();

    // Nose tip highlight
    glColor3f(0.8f, 0.8f, 0.85f);
    glBegin(GL_TRIANGLES);
    glVertex2f(60, 5);
    glVertex2f(75, 0);
    glVertex2f(60, -5);
    glEnd();

    // Main wing: swept back, angled down from mid-fuselage
    glColor3f(0.75f, 0.75f, 0.8f);
    glBegin(GL_TRIANGLES);
    glVertex2f(15, -2);
    glVertex2f(-15, -2);
    glVertex2f(-55, -42);
    glEnd();

    // Tail fin: vertical stabilizer at the rear, pointing up
    glBegin(GL_TRIANGLES);
    glVertex2f(-58, 6);
    glVertex2f(-75, 6);
    glVertex2f(-75, 32);
    glEnd();

    // Horizontal tail stabilizer, angled down at the rear
    glBegin(GL_TRIANGLES);
    glVertex2f(-55, 1);
    glVertex2f(-72, 1);
    glVertex2f(-90, -14);
    glEnd();

    // Cabin window strip
    glColor3f(0.35f, 0.6f, 0.85f);
    glBegin(GL_QUADS);
    glVertex2f(-30, 3);
    glVertex2f(40, 3);
    glVertex2f(40, 6);
    glVertex2f(-30, 6);
    glEnd();

    glPopMatrix();
}

void drawBench(float x, float y)
{
    glColor3f(0.55f, 0.3f, 0.1f);

    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + 50, y);
    glVertex2f(x + 50, y + 8);
    glVertex2f(x, y + 8);
    glEnd();

    glBegin(GL_LINES);
    glVertex2f(x + 5, y);
    glVertex2f(x + 5, y - 20);

    glVertex2f(x + 45, y);
    glVertex2f(x + 45, y - 20);
    glEnd();
}

void drawFlower(float x, float y)
{
    glColor3f(0, 0.7f, 0);

    glBegin(GL_LINES);
    glVertex2f(x, y);
    glVertex2f(x, y + 15);
    glEnd();

    glColor3f(1, 0, 1);
    circle(x, y + 18, 4);
}

void drawLakeBoundary()
{
    // Stone embankment wall between the road and the lake
    glColor3f(0.55f, 0.53f, 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(-1000, WALL_BOTTOM);
    glVertex2f(1000, WALL_BOTTOM);
    glVertex2f(1000, WALL_TOP);
    glVertex2f(-1000, WALL_TOP);
    glEnd();

    // Brick/stone texture lines
    glColor3f(0.45f, 0.43f, 0.4f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int x = -1000; x <= 1000; x += 45)
    {
        glVertex2f((float)x, WALL_TOP);
        glVertex2f((float)x, WALL_BOTTOM);
    }
    glVertex2f(-1000, WALL_TOP - 10);
    glVertex2f(1000, WALL_TOP - 10);
    glEnd();

    // Smart lamp posts along the top of the wall
    glColor3f(0.2f, 0.2f, 0.22f);
    for (int x = -1000; x <= 1000; x += 100)
    {
        glBegin(GL_QUADS);
        glVertex2f(x - 2, WALL_TOP);
        glVertex2f(x + 2, WALL_TOP);
        glVertex2f(x + 2, WALL_TOP + 30);
        glVertex2f(x - 2, WALL_TOP + 30);
        glEnd();

        // Glowing sensor/light head - lit cyan at night like a smart light
        if (!dayMode)
        {
            glColor3f(0.2f, 0.9f, 1.0f);
            circle((float)x, WALL_TOP + 32, 4.0f);
        }
        else
        {
            glColor3f(0.5f, 0.5f, 0.55f);
            circle((float)x, WALL_TOP + 32, 3.0f);
        }
        glColor3f(0.2f, 0.2f, 0.22f);
    }

    // LED accent strip along the railing - a subtle "smart city" tech touch
    if (dayMode)
        glColor3f(0.3f, 0.75f, 0.85f);
    else
        glColor3f(0.15f, 0.95f, 1.0f);

    glBegin(GL_QUADS);
    glVertex2f(-1000, WALL_TOP + 8);
    glVertex2f(1000, WALL_TOP + 8);
    glVertex2f(1000, WALL_TOP + 10);
    glVertex2f(-1000, WALL_TOP + 10);
    glEnd();

    // Horizontal safety rail on top of the posts
    glColor3f(0.25f, 0.25f, 0.28f);
    glBegin(GL_QUADS);
    glVertex2f(-1000, WALL_TOP + 13);
    glVertex2f(1000, WALL_TOP + 13);
    glVertex2f(1000, WALL_TOP + 17);
    glVertex2f(-1000, WALL_TOP + 17);
    glEnd();
}

void drawLake()
{
    // Water fills the whole strip below the road (previously blank sky color)
    glColor3f(0.15f, 0.45f, 0.75f);

    glBegin(GL_QUADS);
    glVertex2f(-1000, LAKE_BOTTOM);
    glVertex2f(1000, LAKE_BOTTOM);
    glVertex2f(1000, LAKE_TOP);
    glVertex2f(-1000, LAKE_TOP);
    glEnd();

    // A slightly darker band at the very bottom for depth
    glColor3f(0.08f, 0.3f, 0.55f);
    glBegin(GL_QUADS);
    glVertex2f(-1000, LAKE_BOTTOM);
    glVertex2f(1000, LAKE_BOTTOM);
    glVertex2f(1000, LAKE_BOTTOM + 35);
    glVertex2f(-1000, LAKE_BOTTOM + 35);
    glEnd();

    // Gentle ripple lines across the surface
    glColor3f(0.55f, 0.75f, 0.95f);
    glLineWidth(1.0f);

    for (int row = 0; row < 3; row++)
    {
        float ry = LAKE_TOP - 25 - row * 35;
        float phase = waterPhase * 2 + row;

        glBegin(GL_LINE_STRIP);
        for (int i = -1000; i <= 1000; i += 40)
        {
            float wave = 4.0f * sin(i * 0.02f + phase);
            glVertex2f((float)i, ry + wave);
        }
        glEnd();
    }

    // Floating lily pads scattered around the fountain
    float padSpots[6][2] = {
        {-260, LAKE_CY + 10}, {-160, LAKE_CY - 20}, {150, LAKE_CY + 15},
        {260, LAKE_CY - 10},  {-60, LAKE_CY - 35},  {60, LAKE_CY + 30}
    };

    for (int i = 0; i < 6; i++)
    {
        float px = padSpots[i][0];
        float py = padSpots[i][1] + 2.0f * sin(waterPhase + i);

        glColor3f(0.15f, 0.55f, 0.25f);
        circle(px, py, 10.0f);

        glColor3f(0.95f, 0.6f, 0.75f);
        circle(px, py, 3.5f);
    }

    // Smart lamp light reflections shimmering on the water at night
    if (!dayMode)
    {
        glColor3f(0.25f, 0.85f, 0.95f);
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        for (int x = -1000; x <= 1000; x += 100)
        {
            float shimmer = 3.0f * sin(waterPhase * 2 + x * 0.05f);
            glVertex2f((float)x + shimmer, LAKE_TOP - 5);
            glVertex2f((float)x + shimmer, LAKE_TOP - 45);
        }
        glEnd();
        glLineWidth(1.0f);
    }
}

const float SIDEWALK_TOP = -40.0f;
const float SIDEWALK_BOTTOM = -100.0f; // meets the top of the road
const float PERSON_Y = -70.0f;         // well clear of the lane-1 car roofline (-80)

const float GRASS_PATH_X = -50.0f;   // path ta kothay hobe (x position) - dorkar moto change korte paren
const float GRASS_PATH_WIDTH = 40.0f; // path koto chodo hobe
void drawSidewalk()
{
    glColor3f(0.78f, 0.76f, 0.72f);
    glBegin(GL_QUADS);
    glVertex2f(-1000, SIDEWALK_BOTTOM);
    glVertex2f(1000, SIDEWALK_BOTTOM);
    glVertex2f(1000, SIDEWALK_TOP);
    glVertex2f(-1000, SIDEWALK_TOP);

    glEnd();

    // Paving-slab seams for a bit of texture
    glColor3f(0.65f, 0.63f, 0.6f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int x = -1000; x <= 1000; x += 60)
    {
        glVertex2f((float)x, SIDEWALK_TOP);
        glVertex2f((float)x, SIDEWALK_BOTTOM);
    }
    glEnd();
}

void drawLaneDivider(float y)
{
    glColor3f(1, 1, 0);

    for (int i = -1000; i < 1000; i += 100)
    {
        glBegin(GL_QUADS);
        glVertex2f(i, y);
        glVertex2f(i + 50, y);
        glVertex2f(i + 50, y + 8);
        glVertex2f(i, y + 8);
        glEnd();
    }
}

// Zebra-stripe pedestrian crossing spanning the full road width at
// the given x position, so pedestrians have a marked place to cross
// near each traffic light intersection.
void drawCrosswalk(float x)
{
    glColor3f(0.9f, 0.9f, 0.85f);

    for (float y = -352; y < -108; y += 24)
    {
        glBegin(GL_QUADS);
        glVertex2f(x - 30, y);
        glVertex2f(x + 30, y);
        glVertex2f(x + 30, y + 14);
        glVertex2f(x - 30, y + 14);
        glEnd();
    }
}

// =======================================================================
// FEATURE 1 (original numbering): MARKET AREA
// =======================================================================

void initMarket()
{
    for (int i = 0; i < MARKET_CUSTOMER_COUNT; i++)
    {
        marketCustomerX[i]  = MARKET_START_X - 40 + i * 90.0f;
        marketCustomerDir[i] = (i % 2 == 0) ? 1.0f : -1.0f;
    }
}

void drawUmbrella(float x, float y, float r, float g, float b)
{
    glColor3f(r, g, b);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y + 22);
    for (int i = 0; i <= 20; i++)
    {
        float a = 3.1416f * i / 20.0f;
        glVertex2f(x - 20 * cos(a), y + 12 + 10 * sin(a));
    }
    glEnd();

    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_LINES);
    glVertex2f(x, y + 12);
    glVertex2f(x, y - 20);
    glEnd();
}

// type: 0 = fruit stall (round produce), 1 = vegetable stall (blocky produce)
void drawStall(float x, float y, int type)
{
    // Table
    glColor3f(0.55f, 0.35f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(x - 25, y);
    glVertex2f(x + 25, y);
    glVertex2f(x + 25, y + 6);
    glVertex2f(x - 25, y + 6);
    glEnd();

    glBegin(GL_LINES);
    glVertex2f(x - 22, y);
    glVertex2f(x - 22, y - 18);
    glVertex2f(x + 22, y);
    glVertex2f(x + 22, y - 18);
    glEnd();

    // Produce piled on top
    if (type == 0)
    {
        float col[3][3] = {{1,0.2f,0.2f},{1,0.6f,0.1f},{1,1,0.2f}};
        for (int i = 0; i < 6; i++)
        {
            glColor3f(col[i % 3][0], col[i % 3][1], col[i % 3][2]);
            circle(x - 18 + i * 7.0f, y + 10 + (i % 2) * 5.0f, 5.0f);
        }
    }
    else
    {
        float col[3][3] = {{0.1f,0.6f,0.1f},{0.7f,0.2f,0.2f},{0.9f,0.6f,0.1f}};
        for (int i = 0; i < 5; i++)
        {
            glColor3f(col[i % 3][0], col[i % 3][1], col[i % 3][2]);
            glBegin(GL_QUADS);
            glVertex2f(x - 18 + i * 8.0f, y + 6);
            glVertex2f(x - 12 + i * 8.0f, y + 6);
            glVertex2f(x - 12 + i * 8.0f, y + 15);
            glVertex2f(x - 18 + i * 8.0f, y + 15);
            glEnd();
        }
    }

    drawUmbrella(x, y + 20, (type == 0) ? 0.9f : 0.2f, 0.3f, (type == 0) ? 0.2f : 0.7f);
}

// Simple shop: body, sloped roof, blank sign board (no text, as requested),
// a couple of decorative string lights along the front edge.
void drawShop(float x, float shopColorR, float shopColorG, float shopColorB)
{
    float y = MARKET_Y;

    // Body
    glColor3f(shopColorR, shopColorG, shopColorB);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + 100, y);
    glVertex2f(x + 100, y + 65);
    glVertex2f(x, y + 65);
    glEnd();

    // Roof (sloped triangle)
    glColor3f(shopColorR * 0.6f, shopColorG * 0.6f, shopColorB * 0.6f);
    glBegin(GL_TRIANGLES);
    glVertex2f(x - 10, y + 65);
    glVertex2f(x + 110, y + 65);
    glVertex2f(x + 50, y + 100);
    glEnd();

    // Doorway
    glColor3f(0.35f, 0.25f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(x + 40, y);
    glVertex2f(x + 60, y);
    glVertex2f(x + 60, y + 35);
    glVertex2f(x + 40, y + 35);
    glEnd();

    // Sign board - blank cream base, decorated with a colored stripe
// pattern instead of text (keeps it text-free but not empty-looking)
glColor3f(0.95f, 0.95f, 0.9f);
glBegin(GL_QUADS);
glVertex2f(x + 20, y + 45);
glVertex2f(x + 80, y + 45);
glVertex2f(x + 80, y + 58);
glVertex2f(x + 20, y + 58);
glEnd();

// Thin border frame around the sign
glColor3f(shopColorR * 0.7f, shopColorG * 0.7f, shopColorB * 0.7f);
glLineWidth(1.5f);
glBegin(GL_LINE_LOOP);
glVertex2f(x + 20, y + 45);
glVertex2f(x + 80, y + 45);
glVertex2f(x + 80, y + 58);
glVertex2f(x + 20, y + 58);
glEnd();
glLineWidth(1.0f);

// Decorative stripe pattern in the shop's own accent color -
// reads as a shop logo/emblem without needing any text
glColor3f(shopColorR, shopColorG, shopColorB);
for (int s = 0; s < 4; s++)
{
    float sx = x + 26 + s * 13.0f;
    glBegin(GL_QUADS);
    glVertex2f(sx, y + 48);
    glVertex2f(sx + 7, y + 48);
    glVertex2f(sx + 7, y + 55);
    glVertex2f(sx, y + 55);
    glEnd();
}

// Small circular emblem dot centered above the stripes, in a
// contrasting accent color for a bit of visual variety per shop
glColor3f(1.0f, 0.85f, 0.3f);
circle(x + 50, y + 51.5f, 2.0f);

    // Decorative string lights along the roof edge
    for (int i = 0; i <= 5; i++)
    {
        float lx = x + i * 20.0f - 5.0f;
        if (!dayMode && marketLightsOn)
            glColor3f(1.0f, 0.85f, 0.3f);
        else
            glColor3f(0.6f, 0.55f, 0.4f);
        circle(lx, y + 66, 2.5f);
    }
}

void drawMarketCustomer(float x, float y)
{
    if (rainMode) return;
    glColor3f(1, 0.8f, 0.6f);
    circle(x, y + 22, 6);

    glColor3f(0.2f, 0.3f, 0.8f);
    glBegin(GL_QUADS);
    glVertex2f(x - 5, y);
    glVertex2f(x + 5, y);
    glVertex2f(x + 5, y + 16);
    glVertex2f(x - 5, y + 16);
    glEnd();

    glColor3f(0.15f, 0.15f, 0.15f);
    glBegin(GL_LINES);
    glVertex2f(x - 3, y);
    glVertex2f(x - 3, y - 14);
    glVertex2f(x + 3, y);
    glVertex2f(x + 3, y - 14);
    glEnd();
}

void drawMarket()
{
    for (int i = 0; i < MARKET_SHOP_COUNT; i++)
        drawShop(MARKET_START_X + i * MARKET_SHOP_GAP,
                 marketShopColors[i][0], marketShopColors[i][1], marketShopColors[i][2]);

    // Stalls sit in front of the shops, alternating fruit/veg
    for (int i = 0; i < MARKET_SHOP_COUNT; i++)
        drawStall(MARKET_START_X + 50 + i * MARKET_SHOP_GAP, MARKET_Y - 5, i % 2);

    for (int i = 0; i < MARKET_CUSTOMER_COUNT; i++)
        drawMarketCustomer(marketCustomerX[i], MARKET_Y);
}

void updateMarket()
{
    for (int i = 0; i < MARKET_CUSTOMER_COUNT; i++)
    {
        marketCustomerX[i] += marketCustomerDir[i] * 0.5f;
        // Keep each customer wandering within their own shop's frontage
        // so they never drift through a neighboring stall.
        float minX = MARKET_START_X - 40 + i * 90.0f - 15.0f;
        float maxX = MARKET_START_X - 40 + i * 90.0f + 15.0f;
        if (marketCustomerX[i] < minX || marketCustomerX[i] > maxX)
            marketCustomerDir[i] *= -1.0f;
    }

    marketBlinkCount++;
    if (marketBlinkCount > 40)
    {
        marketLightsOn = !marketLightsOn;
        marketBlinkCount = 0;
    }
}

// =======================================================================
// FEATURE 2 (original numbering): BOAT
// =======================================================================

void drawBoat()
{
    float y = LAKE_CY + 25.0f;

    glPushMatrix();
    glTranslatef(boatX, y, 0);
    glScalef(2.1f, 2.1f, 1.0f);

    // ---- Hull: flat gunwale top + smooth curved bottom (more points
    //      = smoother curve, so it reads as a real boat, not a blob) ----
    // ---- Hull: smooth, symmetric curved boat shape ----
    glColor3f(0.42f, 0.24f, 0.1f);
    glBegin(GL_POLYGON);
    glVertex2f(-44, 10);   // top-left (gunwale)
    glVertex2f(44, 10);    // top-right (gunwale)
    glVertex2f(40, 2);
    glVertex2f(32, -6);
    glVertex2f(20, -11);
    glVertex2f(8, -13);
    glVertex2f(-8, -13);
    glVertex2f(-20, -11);
    glVertex2f(-32, -6);
    glVertex2f(-40, 2);
    glEnd();

    // Inner hull shading - lighter band just under the gunwale, gives
    // depth/a "hollowed out" look without breaking the outer silhouette
    glColor3f(0.56f, 0.35f, 0.17f);
    glBegin(GL_QUADS);
    glVertex2f(-38, 8);
    glVertex2f(38, 8);
    glVertex2f(34, 2);
    glVertex2f(-34, 2);
    glEnd();

    // Gunwale rim outline - crisp top edge
    glColor3f(0.26f, 0.14f, 0.05f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(-44, 10);
    glVertex2f(44, 10);
    glEnd();
    glLineWidth(1.0f);
    // ---- Mast + বাংলাদেশের পতাকা - stern (ডানদিকে) ----
    glColor3f(0.3f, 0.2f, 0.1f);
    glBegin(GL_LINES);
    glVertex2f(28, 8);
    glVertex2f(28, 32);
    glEnd();

    glColor3f(0.0f, 0.4f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(28, 32);
    glVertex2f(46, 30);
    glVertex2f(46, 21);
    glVertex2f(28, 23);
    glEnd();
    glColor3f(0.85f, 0.05f, 0.1f);
    circle(37.0f, 26.5f, 3.2f);

    // =====================================================
    // BOATMAN
    // =====================================================
    float mx = -22.0f;
    float mBase = 8.0f;

    float rowSwing = sin(rowPhase);

    // leg
    glColor3f(0.25f, 0.25f, 0.3f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(mx - 2.5f, mBase); glVertex2f(mx - 2.5f, mBase + 7);
    glVertex2f(mx + 2.5f, mBase); glVertex2f(mx + 2.5f, mBase + 7);
    glEnd();

    glColor3f(0.92f, 0.92f, 0.88f);
    glBegin(GL_POLYGON);
    glVertex2f(mx - 5, mBase + 7);
    glVertex2f(mx + 5, mBase + 7);
    glVertex2f(mx + 4, mBase + 14);
    glVertex2f(mx - 4, mBase + 14);
    glEnd();

    glColor3f(0.5f, 0.32f, 0.18f);
    glBegin(GL_QUADS);
    glVertex2f(mx - 4.5f, mBase + 13);
    glVertex2f(mx + 4.5f, mBase + 13);
    glVertex2f(mx + 4, mBase + 20);
    glVertex2f(mx - 4, mBase + 20);
    glEnd();

    glColor3f(1.0f, 0.8f, 0.6f);
    circle(mx, mBase + 24, 3.5f);

    glColor3f(0.8f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(mx - 3.5f, mBase + 25.5f);
    glVertex2f(mx + 3.5f, mBase + 25.5f);
    glVertex2f(mx + 3.5f, mBase + 27.5f);
    glVertex2f(mx - 3.5f, mBase + 27.5f);
    glEnd();

    // হাত - বৈঠা বাওয়ার animation (rowSwing দিয়ে দোলে), boat-এর বাইরের
    // (বাম) দিকে বৈঠা নামানো, যাতে hull-এর সাথে ওভারল্যাপ না করে
    float handX = mx - 8 + rowSwing * 4.0f;
    float handY = mBase + 16 + rowSwing * 2.0f;

    glColor3f(1.0f, 0.8f, 0.6f);
    glLineWidth(2.2f);
    glBegin(GL_LINES);
    glVertex2f(mx - 3, mBase + 18);
    glVertex2f(handX, handY);
    glVertex2f(mx + 3, mBase + 18);
    glVertex2f(handX + 2, handY - 2);
    glEnd();

    float oarTipX = handX - 12 + rowSwing * 5.0f;
    float oarTipY = mBase - 22;

    glColor3f(0.35f, 0.22f, 0.1f);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
    glVertex2f(handX, handY);
    glVertex2f(oarTipX, oarTipY);
    glEnd();
    glLineWidth(1.0f);

    glColor3f(0.3f, 0.18f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2f(oarTipX - 2, oarTipY + 2);
    glVertex2f(oarTipX + 2, oarTipY + 2);
    glVertex2f(oarTipX + 2, oarTipY - 8);
    glVertex2f(oarTipX - 2, oarTipY - 8);
    glEnd();

    if (rowSwing < -0.6f)
    {
        glColor3f(0.8f, 0.9f, 1.0f);
        circle(oarTipX, oarTipY + 3, 2.0f);
        circle(oarTipX - 3, oarTipY + 1, 1.3f);
    }

    glPopMatrix();
}
void updateBoat()
{
    boatX += boatDir * 1.2f;
    if (boatX > BOAT_MAX_X || boatX < BOAT_MIN_X)
        boatDir *= -1.0f;

    rowPhase += 0.12f;
    if (rowPhase > 6.2832f) rowPhase -= 6.2832f;
}
// =======================================================================
// FEATURE 4 (original numbering): FOOTPATH BOUNDARY
// =======================================================================
void drawGrassPath()
{
    // (zigzag/winding) inner road
    const int SEGMENTS = 6;
    float segH = (120.0f - SIDEWALK_TOP) / SEGMENTS;

    for (int i = 0; i < SEGMENTS; i++)
    {
        float yTop = 120.0f - i * segH;
        float yBot = yTop - segH;

        float offsetTop = 12.0f * sin(i * 1.1f);
        float offsetBot = 12.0f * sin((i + 1) * 1.1f);

        float cxTop = GRASS_PATH_X + offsetTop;
        float cxBot = GRASS_PATH_X + offsetBot;

        glColor3f(0.72f, 0.58f, 0.32f);
        glBegin(GL_QUADS);
        glVertex2f(cxTop - GRASS_PATH_WIDTH / 2, yTop);
        glVertex2f(cxTop + GRASS_PATH_WIDTH / 2, yTop);
        glVertex2f(cxBot + GRASS_PATH_WIDTH / 2, yBot);
        glVertex2f(cxBot - GRASS_PATH_WIDTH / 2, yBot);
        glEnd();

        glColor3f(0.58f, 0.44f, 0.22f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex2f(cxTop - GRASS_PATH_WIDTH / 2, yTop);
        glVertex2f(cxBot - GRASS_PATH_WIDTH / 2, yBot);
        glVertex2f(cxTop + GRASS_PATH_WIDTH / 2, yTop);
        glVertex2f(cxBot + GRASS_PATH_WIDTH / 2, yBot);
        glEnd();
        glLineWidth(1.0f);
    }
}
void drawFootpathBoundary()
{
    // A low trimmed hedge running along the top edge of the sidewalk,
    // the natural seam between the grass/city ground and the footpath.
    glColor3f(0.15f, 0.45f, 0.15f);
    for (int x = -1000; x < 1000; x += 25)
    {
        if (fabs((x + 10) - GRASS_PATH_X) < GRASS_PATH_WIDTH / 2 + 8) continue; // path-er jaygay gap
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x + 10, SIDEWALK_TOP + 14);
        for (int i = 0; i <= 12; i++)
        {
            float a = 3.1416f * i / 12.0f;
            glVertex2f(x + 10 - 12 * cos(a), SIDEWALK_TOP + 6 + 8 * sin(a));
        }
        glEnd();
    }

    // Tiny accent flowers dotted along the hedge
    for (int x = -990; x < 1000; x += 75)
    {
        if (fabs((float)x - GRASS_PATH_X) < GRASS_PATH_WIDTH / 2 + 8) continue;
        glColor3f(1, 0.9f, 0.2f);
        circle((float)x, SIDEWALK_TOP + 14, 2.0f);
    }
}

// =======================================================================
// FEATURE 5 (original numbering): STREET DETAILS
// =======================================================================

void drawDustbin(float x, float y)
{
    glColor3f(0.2f, 0.4f, 0.25f);
    glBegin(GL_QUADS);
    glVertex2f(x - 10, y);
    glVertex2f(x + 10, y);
    glVertex2f(x + 8, y + 28);
    glVertex2f(x - 8, y + 28);
    glEnd();

    glColor3f(0.15f, 0.3f, 0.18f);
    glBegin(GL_QUADS);
    glVertex2f(x - 11, y + 28);
    glVertex2f(x + 11, y + 28);
    glVertex2f(x + 11, y + 32);
    glVertex2f(x - 11, y + 32);
    glEnd();
}

void drawRoadSign(float x, float y)
{
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_QUADS);
    glVertex2f(x - 2, y);
    glVertex2f(x + 2, y);
    glVertex2f(x + 2, y + 55);
    glVertex2f(x - 2, y + 55);
    glEnd();

    glColor3f(0.1f, 0.4f, 0.8f);
    glBegin(GL_TRIANGLES);
    glVertex2f(x, y + 75);
    glVertex2f(x - 16, y + 55);
    glVertex2f(x + 16, y + 55);
    glEnd();
}



void drawBikeStand(float x, float y)
{
    glColor3f(0.25f, 0.25f, 0.25f);
    for (int i = 0; i < 3; i++)
    {
        glBegin(GL_LINE_LOOP);
        for (int j = 0; j <= 20; j++)
        {
            float a = 2 * 3.1416f * j / 20.0f;
            glVertex2f(x + i * 20.0f + 8 * cos(a), y + 12 + 8 * sin(a));
        }
        glEnd();
    }
}

void drawFireHydrant(float x, float y)
{
    glColor3f(0.8f, 0.1f, 0.1f);
    glBegin(GL_QUADS);
    glVertex2f(x - 6, y);
    glVertex2f(x + 6, y);
    glVertex2f(x + 6, y + 20);
    glVertex2f(x - 6, y + 20);
    glEnd();
    circle(x, y + 20, 6);

    glColor3f(0.6f, 0.05f, 0.05f);
    glBegin(GL_QUADS);
    glVertex2f(x - 10, y + 10);
    glVertex2f(x - 4, y + 10);
    glVertex2f(x - 4, y + 14);
    glVertex2f(x - 10, y + 14);
    glEnd();
}

void drawMailbox(float x, float y)
{
    glColor3f(0.1f, 0.3f, 0.6f);
    glBegin(GL_QUADS);
    glVertex2f(x - 8, y);
    glVertex2f(x + 8, y);
    glVertex2f(x + 8, y + 22);
    glVertex2f(x - 8, y + 22);
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y + 22);
    for (int i = 0; i <= 10; i++)
    {
        float a = 3.1416f * i / 10.0f;
        glVertex2f(x - 8 * cos(a), y + 22 + 5 * sin(a));
    }
    glEnd();
}

// =======================================================================
// FEATURE 6 (original numbering): PEDESTRIAN WALK ANIMATION UPDATE
// =======================================================================

void updateWalkAnimation()
{
    walkPhase += 0.25f;
    if (walkPhase > 6.2832f) walkPhase -= 6.2832f;
}

// =======================================================================
// FEATURE 1 (this pass): SMART TRAFFIC CONTROL SYSTEM
// =======================================================================
// car1/car3/busX already move together at the same speed with fixed
// spacing (LANE1_SPACING), so easing/resuming them together via a
// single shared lane1SpeedFactor keeps that gap exactly fixed --
// meaning none of them can ever overlap each other.

// Smallest forward distance (in the direction of travel) from any lane-1
// member to the STOP-LINE BOUNDARY of its nearest upcoming crosswalk
// (crosswalk near edge, pulled back by that member's own half-length
// plus a small margin -- the point where its FRONT bumper must halt).
// A member that has already reached/passed a boundary contributes
// nothing for that crosswalk (skipped) -- it is already committed and
// must be allowed to clear the crossing, never held there. Wraparound
// is automatic: right after a member wraps to LANE_MIN, boundary - x
// is simply a large positive number again, so no manual "+= LANE_RANGE"
// bookkeeping is needed.
float lane1DistanceToStopLine()
{
    float memberX[3]      = { car1, car3, busX };
    float memberHalf[3]   = { 50.0f, 50.0f, 90.0f };
    float crosswalks[2]   = { CROSSWALK_A_X, CROSSWALK_B_X };
    float best = 1e9f;

    for (int i = 0; i < 3; i++)
    {
        for (int c = 0; c < 2; c++)
        {
            float boundary = (crosswalks[c] - 30.0f) - memberHalf[i] - STOP_LINE_MARGIN;
            float d = boundary - memberX[i];
            if (d <= 0.0f) continue; // already at/through this line -- not a reason to brake
            if (d < best) best = d;
        }
    }
    return best;
}

float lane2DistanceToStopLine()
{
    float crosswalks[2] = { CROSSWALK_A_X, CROSSWALK_B_X };
    float halfLen = 50.0f;
    float best = 1e9f;

    for (int c = 0; c < 2; c++)
    {
        float boundary = (crosswalks[c] + 30.0f) + halfLen + STOP_LINE_MARGIN;
        float d = car2 - boundary; // car2 travels in -x, so "ahead" means car2 > boundary
        if (d <= 0.0f) continue;
        if (d < best) best = d;
    }
    return best;
}

float commitThreshold(float halfLen)
{
    return 60.0f + STOP_LINE_MARGIN + 2.0f * halfLen + 4.0f; // e.g. 186 for a car, 266 for the bus
}

float lane1MaxStep(float x, float halfLen)
{
    float crosswalks[2] = { CROSSWALK_A_X, CROSSWALK_B_X };
    float best = 1e9f;
    float threshold = commitThreshold(halfLen);

    for (int c = 0; c < 2; c++)
    {
        float boundary = (crosswalks[c] - 30.0f) - halfLen - STOP_LINE_MARGIN; // front must halt clearly before this x
        float allowed = boundary - x;
        if (allowed <= -threshold) continue; // genuinely already past the whole crosswalk -- let it clear
        if (allowed < best) best = allowed;
    }
    return best; // 1e9 if no crosswalk still ahead -> unrestricted
}

float lane2MaxStep(float x, float halfLen)
{
    float crosswalks[2] = { CROSSWALK_A_X, CROSSWALK_B_X };
    float best = 1e9f;
    float threshold = commitThreshold(halfLen);

    for (int c = 0; c < 2; c++)
    {
        float boundary = (crosswalks[c] + 30.0f) + halfLen + STOP_LINE_MARGIN; // leftward: front must halt clearly above this x
        float allowed = x - boundary;
        if (allowed <= -threshold) continue;
        if (allowed < best) best = allowed;
    }
    return best;
}

float snapBehindStopLine(float x, float halfLen, bool rightward)
{
    float crosswalks[2] = { CROSSWALK_A_X, CROSSWALK_B_X };
    float threshold = commitThreshold(halfLen);

    for (int c = 0; c < 2; c++)
    {
        if (rightward)
        {
            float boundary = (crosswalks[c] - 30.0f) - halfLen - STOP_LINE_MARGIN;
            float overshoot = x - boundary;
            if (overshoot > 0.0f && overshoot < threshold)
                x = boundary;
        }
        else
        {
            float boundary = (crosswalks[c] + 30.0f) + halfLen + STOP_LINE_MARGIN;
            float overshoot = boundary - x;
            if (overshoot > 0.0f && overshoot < threshold)
                x = boundary;
        }
    }
    return x;
}

void updateTraffic()
{
    // ---- Lane 1 (rightward: car1, car3, busX) ----
    float dist1 = lane1DistanceToStopLine();
    float targetFactor1 = 1.0f;

    // Braking starts as soon as the light leaves green (signal != 0,
    // i.e. yellow OR red) so vehicles get the full yellow phase to
    // decelerate smoothly, and are already at (or very near) rest by
    // the time red actually begins.
    if (signal != 0 && dist1 < STOP_ZONE_LANE1)
        targetFactor1 = dist1 / STOP_ZONE_LANE1;
    if (targetFactor1 < 0.0f) targetFactor1 = 0.0f;

    // Smoothly interpolate so braking looks natural, not instant
    lane1SpeedFactor += (targetFactor1 - lane1SpeedFactor) * 0.15f;
    if (lane1SpeedFactor < 0.02f) lane1SpeedFactor = 0.0f; // fully queued/stopped

    float step1 = TRAFFIC_SPEED * lane1SpeedFactor;

    if (signal != 0)
    {
        float maxCar1 = lane1MaxStep(car1, 50.0f);
        float maxCar3 = lane1MaxStep(car3, 50.0f);
        float maxBus  = lane1MaxStep(busX, 90.0f);
        float clamped = step1;
        if (maxCar1 < clamped) clamped = maxCar1;
        if (maxCar3 < clamped) clamped = maxCar3;
        if (maxBus  < clamped) clamped = maxBus;
        if (clamped < 0.0f) clamped = 0.0f;
        step1 = clamped;
    }

    car1 += step1;  if (car1 > LANE_MAX) car1 -= LANE_RANGE;
    car3 += step1;  if (car3 > LANE_MAX) car3 -= LANE_RANGE;
    busX += step1;  if (busX > LANE_MAX) busX -= LANE_RANGE;

    if (signal != 0)
    {
        car1 = snapBehindStopLine(car1, 50.0f, true);
        car3 = snapBehindStopLine(car3, 50.0f, true);
        busX = snapBehindStopLine(busX, 90.0f, true);
    }

    // ---- Lane 2 (leftward: car2) ----
    float dist2 = lane2DistanceToStopLine();
    float targetFactor2 = 1.0f;

    // Same fix as lane 1 above: ease starting at yellow (signal != 0),
    // not only on red, so car2 also decelerates gradually instead of
    // being caught by the hard clamp with no warning.
    if (signal != 0 && dist2 < STOP_ZONE_LANE2)
        targetFactor2 = dist2 / STOP_ZONE_LANE2;
    if (targetFactor2 < 0.0f) targetFactor2 = 0.0f;

    lane2SpeedFactor += (targetFactor2 - lane2SpeedFactor) * 0.15f;
    if (lane2SpeedFactor < 0.02f) lane2SpeedFactor = 0.0f;

    float step2 = 4.0f * lane2SpeedFactor;

    if (signal != 0)
    {
        float maxStep2 = lane2MaxStep(car2, 50.0f);
        if (maxStep2 < step2) step2 = maxStep2;
        if (step2 < 0.0f) step2 = 0.0f;
    }

    car2 -= step2;  if (car2 < LANE_MIN) car2 += LANE_RANGE;

    if (signal != 0)
        car2 = snapBehindStopLine(car2, 50.0f, false);
}

float singleDistanceToStopLine(float x)
{
    float crosswalks[2] = { CROSSWALK_A_X, CROSSWALK_B_X };
    float best = 1e9f;

    for (int c = 0; c < 2; c++)
    {
        float d = crosswalks[c] - x;
        if (d < 0.0f) d += LANE_RANGE; // already passed this lap -> reached again after wrap
        if (d < best) best = d;
    }
    return best;
}

const float STOP_ZONE_LANE3 = 260.0f; // same magnitude as lane 1 (same speed, same car half-length)
float lane3SpeedFactor = 1.0f;

void updatePlayerCar()
{
    if (!carMove) return; // manual pause via 'C' -- leave position untouched, unchanged behaviour

    float dist3 = singleDistanceToStopLine(carPosition);
    float targetFactor3 = 1.0f;

    if (signal != 0 && dist3 < STOP_ZONE_LANE3) // ease starting at yellow, same as lanes 1/2
        targetFactor3 = dist3 / STOP_ZONE_LANE3;
    if (targetFactor3 < 0.0f) targetFactor3 = 0.0f;

    lane3SpeedFactor += (targetFactor3 - lane3SpeedFactor) * 0.15f;
    if (lane3SpeedFactor < 0.02f) lane3SpeedFactor = 0.0f;

    float step3 = 3.0f * lane3SpeedFactor; // same base speed as the original "+= 4"

    if (signal != 0)
    {
        float maxStep3 = lane1MaxStep(carPosition, 50.0f); // same half-length as the other cars
        if (maxStep3 < step3) step3 = maxStep3;
        if (step3 < 0.0f) step3 = 0.0f;
    }

    carPosition += step3;
    if (carPosition > LANE_MAX) carPosition -= LANE_RANGE;

    if (signal != 0)
        carPosition = snapBehindStopLine(carPosition, 50.0f, true);
}

// =======================================================================
// PEDESTRIAN CROSSING (this pass)
// =======================================================================
void drawCrossingPedestrians()
{
    drawPerson(CROSSWALK_A_X, crossPedY[0]);
    drawPerson(CROSSWALK_B_X, crossPedY[1]);
}

int prevSignal = 0;

void updateCrossingPedestrians()
{
    bool redJustStarted = (signal == 2 && prevSignal != 2);

    for (int i = 0; i < 2; i++)
    {
        switch (crossPedState[i])
        {
        case CROSS_WAIT_NEAR:
            if (redJustStarted) crossPedState[i] = CROSS_GOING_FAR;
            break;

        case CROSS_GOING_FAR:
            crossPedY[i] -= CROSS_SPEED;
            if (crossPedY[i] <= CROSS_FAR_Y)
            {
                crossPedY[i] = CROSS_FAR_Y;
                crossPedState[i] = CROSS_WAIT_FAR;
            }
            break;

        case CROSS_WAIT_FAR:

            if (redJustStarted) crossPedState[i] = CROSS_GOING_NEAR;
            break;

        case CROSS_GOING_NEAR:
            crossPedY[i] += CROSS_SPEED;
            if (crossPedY[i] >= CROSS_NEAR_Y)
            {
                crossPedY[i] = CROSS_NEAR_Y;
                crossPedState[i] = CROSS_WAIT_NEAR;
            }
            break;
        }
    }

    prevSignal = signal;
}
// =======================================================================
// FEATURE 10 (original numbering): DECORATIONS (flags)
// =======================================================================


void drawDecorations()
{
    // Small flags along the sidewalk, purely decorative
    for (int x = -900; x < 1000; x += 300)
    {
        glColor3f(0.9f, 0.2f, 0.2f);
        glBegin(GL_TRIANGLES);
        glVertex2f((float)x, -35);
        glVertex2f((float)x + 14, -40);
        glVertex2f((float)x, -45);
        glEnd();
        glColor3f(0.3f, 0.3f, 0.3f);
        glBegin(GL_LINES);
        glVertex2f((float)x, -30);
        glVertex2f((float)x, -50);
        glEnd();
    }
}


// =======================================================================
// FEATURE 12 (original numbering): RIVER FISH JUMP
// =======================================================================

void drawFish()
{
    if (!fishJumping) return;

    float t = fishTimer / 20.0f;           // 0..1 across the jump
    float arc = 30.0f * sin(t * 3.1416f);  // small hop above the water

    glColor3f(0.6f, 0.7f, 0.9f);
    glPushMatrix();
    glTranslatef(fishX, LAKE_CY + 10 + arc, 0);
    glBegin(GL_TRIANGLES);
    glVertex2f(-8, 0);
    glVertex2f(8, 3);
    glVertex2f(8, -3);
    glEnd();
    glPopMatrix();
}

void updateFish()
{
    if (fishJumping)
    {
        fishTimer += 1.0f;
        if (fishTimer > 20.0f) { fishJumping = false; fishTimer = 0.0f; }
    }
    else if (rand() % 300 == 0) // occasionally trigger a jump
    {
        fishJumping = true;
        fishX = -300.0f + (rand() % 600);
    }
}

// GLUT bitmap font diye string draw korar helper
void drawText(float x, float y, const char* text, void* font = GLUT_BITMAP_HELVETICA_18)
{
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; c++)
        glutBitmapCharacter(font, *c);
}

void drawTextCentered(float centerX, float y, const char* text, void* font = GLUT_BITMAP_HELVETICA_18)
{
    int width = 0;
    for (const char* c = text; *c != '\0'; c++)
        width += glutBitmapWidth(font, *c);

    drawText(centerX - width / 2.0f, y, text, font);
}

void drawWelcomeScreen()
{
    glClearColor(0.08f, 0.1f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(1.0f, 1.0f, 1.0f);
    drawTextCentered(0, 260, "Welcome to Our Project", GLUT_BITMAP_TIMES_ROMAN_24);

    glColor3f(0.85f, 0.85f, 0.85f);
    drawTextCentered(0, 150, "Press R to toggle Rain", GLUT_BITMAP_TIMES_ROMAN_24);
    drawTextCentered(0, 115, "Press N for Night mode", GLUT_BITMAP_TIMES_ROMAN_24);
    drawTextCentered(0, 80,  "Press D for Day mode", GLUT_BITMAP_TIMES_ROMAN_24);
    drawTextCentered(0, 45,  "Press C to Pause/Resume your car", GLUT_BITMAP_TIMES_ROMAN_24);
    drawTextCentered(0, 10,  "Use + / - or mouse scroll to Zoom", GLUT_BITMAP_TIMES_ROMAN_24);

    glColor3f(1.0f, 0.85f, 0.3f);
    drawTextCentered(0, -100, "Press ENTER to Start", GLUT_BITMAP_TIMES_ROMAN_24);

    glutSwapBuffers();
}
// =======================================================================
// DISPLAY
// =======================================================================

void display()
{
    {
    if (showWelcome)
    {
        drawWelcomeScreen();
        return;
    }

    drawBackground();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Whole scene is drawn inside a single zoomable transform
    glPushMatrix();
    glScalef(zoom, zoom, 1);

    drawStars();
    if (dayMode) drawSkyGradient(); // night keeps the plain dark clear color so stars stay crisp
    drawMountains();
    drawSkyObjects();

    drawCloud(-700 + cloudMove, 320);
    drawCloud(-250 + cloudMove, 280);
    drawCloud(250 + cloudMove, 340);

    drawPlane();
    drawBird();

    drawGrass();
    drawGrassPath();
    drawDecorations();     // butterflies + flags
    drawSidewalk();
    drawFootpathBoundary(); // hedge border grass/footpath

    {
        float sx = -900;
        for (int i = 0; i < 10; i++)
        {
            drawGroundShadow(sx + 60, 118, 65, 10);
            sx += 180;
        }
    }
    drawBuildings();

    drawGroundShadow(-800, 118, 34, 8);
    drawGroundShadow(-500, 118, 34, 8);
    drawGroundShadow(-150, 118, 34, 8);
    drawGroundShadow(200, 118, 34, 8);
    drawGroundShadow(600, 118, 34, 8);
    drawGroundShadow(900, 118, 34, 8);

    drawTree(-800, 120);
    drawTree(-500, 120);
    drawTree(-150, 120);
    drawTree(200, 120);
    drawTree(600, 120);
    drawTree(900, 120);

    drawBench(-650, 125);
    drawBench(50, 125);
    drawBench(650, 125);

    drawFlower(-870, 125);
    drawFlower(-570, 125);
    drawFlower(-220, 125);
    drawFlower(130, 125);
    drawFlower(530, 125);
    drawFlower(830, 125);

    drawMarket(); // shops, stalls, umbrellas, customers

    // Road (enlarged to comfortably fit 3 separate traffic lanes)
    glColor3f(0.2f, 0.2f, 0.2f);

    glBegin(GL_QUADS);
    glVertex2f(-1000, -360);
    glVertex2f(1000, -360);
    glVertex2f(1000, -100);
    glVertex2f(-1000, -100);
    glEnd();

    // Crosswalks at each traffic-light intersection, drawn on top of
    // the road surface but before the lane dividers/vehicles
    drawCrosswalk(-350);
    drawCrosswalk(350);

    // Lane dividers, one between each pair of lanes
    drawLaneDivider(-162); // between lane 1 and lane 2
    drawLaneDivider(-252); // between lane 2 and lane 3

    drawTrafficLight(-350, -100);
    drawTrafficLight(350, -100);

    drawStreetLight(-800, -100);
    drawStreetLight(-500, -100);
    drawStreetLight(-200, -100);
    drawStreetLight(100, -100);
    drawStreetLight(400, -100);
    drawStreetLight(700, -100);

    drawRoadSign(-600, -95);
    //drawBusStop(150, -95);
    drawFireHydrant(-50, -95);
    drawMailbox(850, -95);

    // Lane 1: rightward traffic, fixed gap kept via matching speed/wrap
    drawCar(car1, LANE1_Y, 1, 0, 0);
    drawCar(car3, LANE1_Y, 0, 1, 0);

    // Lane 2: leftward traffic
    drawCar(car2, LANE2_Y, 0, 0, 1);

    drawBus(busX, LANE1_Y);
    drawPerson(personX, PERSON_Y);
    drawCrossingPedestrians(); // people who actually use the crosswalk when cars are stopped

    // Lane 3: player-controlled car, own dedicated lane
    drawCar(carPosition, LANE3_Y, 1, 0.2f, 0.2f);

    drawRain();

    // Lake with a smart-city fountain, separated from the road
    // by a stone embankment wall with glowing smart lamp posts
    drawLakeBoundary();
    drawLake();
    drawBoat(); // animated boat
    drawFish(); // occasional fish jump
    drawRain();

    glPopMatrix(); // end zoom transform

    glutSwapBuffers();
}
}

void timer(int)
{
    planeX += 3;
    if (planeX > 1500) planeX = -1500;

    birdX += 2;
    if (birdX > 1100) birdX = -1100;

    personX += 1.2f;
    if (personX > 1000) personX = -1000;

    updateTraffic();

    rainY += 8;
    if (rainY > 70) rainY -= 70;

    // FEATURE 1: proper 10s GREEN / 3s YELLOW / 10s RED cycle
    signalFrameCount++;
    int currentPhaseLength =
        (signal == 0) ? SIGNAL_GREEN_FRAMES :
        (signal == 1) ? SIGNAL_YELLOW_FRAMES :
                         SIGNAL_RED_FRAMES;

    if (signalFrameCount > currentPhaseLength)
    {
        signal = (signal + 1) % 3;
        signalFrameCount = 0;
    }

    static int blinkCount = 0;
    blinkCount++;
    if (blinkCount > 30)
    {
        blinkOn = !blinkOn;
        blinkCount = 0;
    }

    cloudMove += 0.4f;
    if (cloudMove > 1200) cloudMove = -1200;

    updatePlayerCar();

    waterPhase += 0.015f;
    if (waterPhase > 6.2832f) waterPhase -= 6.2832f;

    // Drive all the other animations from the same 16ms timer tick
    updateMarket();
    updateBoat();
    updateSun();
    updateWalkAnimation();
    updateFish();
    updateCrossingPedestrians();

    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

void updateSun()
{
    sunAngle += 0.3f;
    if (sunAngle > 360.0f) sunAngle -= 360.0f;
}

void keyboard(unsigned char key, int, int)
{
    switch (key)
    {
    case 'd':
    case 'D':
        dayMode = true;
        break;

    case 'n':
    case 'N':
        dayMode = false;
        break;

    case 'r':
    case 'R':
        rainMode = !rainMode;
        break;

    case 'c':
    case 'C':
        carMove = !carMove;
        break;

    case '+':
        zoom += 0.1f;
        break;

    case '-':
        zoom -= 0.1f;
        if (zoom < 0.3f) zoom = 0.3f;
        break;

    case 13: // Enter key
        showWelcome = false;
        break;
    case 27:
        exit(0);
    }

    glutPostRedisplay();
}

void mouse(int button, int state, int, int)
{
    if (state != GLUT_DOWN)
        return;

    if (button == 3)
        zoom += 0.05f;

    if (button == 4)
    {
        zoom -= 0.05f;
        if (zoom < 0.3f) zoom = 0.3f;
    }

    glutPostRedisplay();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(1300, 700);
    glutCreateWindow("Urban Traffic & Environment Simulation");

    init();

    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutTimerFunc(16, timer, 0);

    glutMainLoop();

    return 0;
}
