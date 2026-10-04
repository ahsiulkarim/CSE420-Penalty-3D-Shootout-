#include<windows.h>
#include <GL/glut.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"   // download from: https://github.com/nothings/stb

GLuint goalkeeperTexID = 0;  // texture handle
GLuint anotherTexID = 0;

// ─────────────────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────────────────
#define PI          3.14159265f
#define WIN_W       900
#define WIN_H       650
#define MAX_SHOTS   5
#define GOAL_LEFT  -5.0f
#define GOAL_RIGHT  5.0f
#define GOAL_TOP    4.5f
#define GOAL_Z     -10.0f

// ─────────────────────────────────────────────────────────
//  Round / Phase tracking
// ─────────────────────────────────────────────────────────
enum GameState  { STATE_MENU, STATE_PLAYING, STATE_HALFTIME, STATE_RESULT };
enum ShootTeam  { TEAM_ARGENTINA, TEAM_BRAZIL };

GameState gameState    = STATE_MENU;
ShootTeam shootingTeam = TEAM_ARGENTINA;

int argScore  = 0;
int braScore  = 0;
int score     = 0;
int shotsLeft = MAX_SHOTS;
int shotsTaken= 0;

// Shot result history: 0 = pending (black), 1 = goal (green), -1 = missed/saved (red)
int shotResults[MAX_SHOTS] = {0, 0, 0, 0, 0};

// ─────────────────────────────────────────────────────────
//  Celebration system
// ─────────────────────────────────────────────────────────
float celebTimer     = 0.0f;   // counts down from 3.0 on goal
bool  argCelebrating = false;
bool  braCelebrating = false;

// per-fan celebration offset (to stagger them)
float fanCelebPhase[14][5]; // [col][row]

// ─────────────────────────────────────────────────────────
//  Ball
// ─────────────────────────────────────────────────────────
float ballX = 0.0f, ballY = 0.55f, ballZ = 16.0f;
float ballDX = 0.0f, ballDY = 0.0f, ballDZ = 0.0f;
float ballRotX = 0.0f, ballRotZ = 0.0f;
bool  isShooting = false;

float aimX   =  0.0f;
float aimY   =  2.0f;
float idleTimer = 0.0f;

// ─────────────────────────────────────────────────────────
//  Goalkeeper
// ─────────────────────────────────────────────────────────
float keeperX     = 0.0f;
float keeperY     = 1.5f;
bool  keeperDiving    = false;
float keeperDiveX     = 0.0f;
float keeperDiveY     = 0.0f;
float keeperDiveTimer = 0.0f;

// ─── Penalty taker animation ───────────────────────────────
float penaltyAnimTimer  = 0.0f;
bool  penaltyAnimActive = false;

bool  p2MoveLeft  = false;
bool  p2MoveRight = false;
float p2ManualSpeed = 0.17f;

// ─────────────────────────────────────────────────────────
//  Feedback
// ─────────────────────────────────────────────────────────
char  feedbackMsg[80] = "";
float feedbackTimer   = 0.0f;
int   lastResult      = 0;

// ─────────────────────────────────────────────────────────
//  Star particles
// ─────────────────────────────────────────────────────────
struct Star { float x, y, vx, vy, life, r, g, b; };
Star stars[80];
int  starCount = 0;

// ─────────────────────────────────────────────────────────
//  Corner flag animation
// ─────────────────────────────────────────────────────────
float flagWave = 0.0f;

// ─────────────────────────────────────────────────────────
//  Clouds
// ─────────────────────────────────────────────────────────
struct Cloud { float x, y, z, speed, scale; };
#define NUM_CLOUDS 7
Cloud clouds[NUM_CLOUDS];

void initClouds()
{
    float xs[]   = { -45,-28,-12, 5, 20, 35, 50 };
    float ys[]   = {  14, 18, 12,16, 14, 19, 13 };
    float zs[]   = { -52,-50,-54,-51,-53,-50,-52 };
    float spds[] = { 0.014f,0.010f,0.016f,0.012f,0.018f,0.011f,0.015f };
    float scls[] = { 1.4f,1.0f,1.8f,1.2f,1.6f,0.9f,1.3f };
    for (int i = 0; i < NUM_CLOUDS; i++)
        clouds[i] = { xs[i], ys[i], zs[i], spds[i], scls[i] };
}
void loadGoalkeeperTexture(const char* filename)
{
    int w, h, channels;
    // flip vertically because OpenGL's origin is bottom-left
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filename, &w, &h, &channels, 4); // force RGBA



if (!data) {
    printf("ERROR: Could not load image: %s\n", filename);
    printf("STB reason: %s\n", stbi_failure_reason());  // ← ADD THIS
    return;
}
printf("Image loaded: %dx%d, channels=%d\n", w, h, channels); // ← ADD THIS

    glGenTextures(1, &goalkeeperTexID);
    glBindTexture(GL_TEXTURE_2D, goalkeeperTexID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    printf("Goalkeeper image loaded OK (%dx%d)\n", w, h);
}

void drawGoalkeeperImage()
{
    if (goalkeeperTexID == 0) return;   // texture not loaded, skip silently

    // ── position & size (tweak these to taste) ──
    float imgX = WIN_W * 0.17f;   // ← was 0.62f, now LEFT side
   float imgY = WIN_H * 0.38f;
   float imgW = 128.0f;
   float imgH = 128.0f;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, goalkeeperTexID);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);   // white = no tint
    glBegin(GL_QUADS);
      glTexCoord2f(0, 0);  glVertex2f(imgX,        imgY);
      glTexCoord2f(1, 0);  glVertex2f(imgX + imgW, imgY);
      glTexCoord2f(1, 1);  glVertex2f(imgX + imgW, imgY + imgH);
      glTexCoord2f(0, 1);  glVertex2f(imgX,        imgY + imgH);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}
void loadGoalkeeperTexture2(const char* filename)
{
    int w, h, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filename, &w, &h, &channels, 4);

    if (!data) {
        printf("ERROR: %s\n", stbi_failure_reason());
        return;
    }
    printf("Image 2 loaded: %dx%d\n", w, h);

    glGenTextures(1, &anotherTexID);      // ← uses new variable
    glBindTexture(GL_TEXTURE_2D, anotherTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
}

void drawAnotherImage()
{
    if (anotherTexID == 0) return;

   float imgX = WIN_W * 0.64f;   // ← was 0.10f, now RIGHT side
   float imgY = WIN_H * 0.38f;
   float imgW = 128.0f;
   float imgH = 128.0f;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, anotherTexID);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
      glTexCoord2f(0, 0); glVertex2f(imgX,        imgY);
      glTexCoord2f(1, 0); glVertex2f(imgX + imgW, imgY);
      glTexCoord2f(1, 1); glVertex2f(imgX + imgW, imgY + imgH);
      glTexCoord2f(0, 1); glVertex2f(imgX,        imgY + imgH);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

void drawOneCloud(float cx, float cy, float cz, float sc)
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 0.82f);
    struct { float dx, dy, dz, r; } blobs[] = {
        { 0.0f, 0.0f, 0.0f, 1.8f }, { 2.2f, 0.0f, 0.0f, 1.4f },
        {-2.2f, 0.0f, 0.0f, 1.4f }, { 1.1f, 1.0f, 0.0f, 1.2f },
        {-1.1f, 0.9f, 0.0f, 1.1f }, { 0.0f, 1.2f, 0.0f, 1.3f },
    };
    for (auto& b : blobs) {
        glPushMatrix();
          glTranslatef(cx + b.dx*sc, cy + b.dy*sc, cz);
          glutSolidSphere(b.r * sc, 8, 6);
        glPopMatrix();
    }
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void drawClouds()
{
    for (int i = 0; i < NUM_CLOUDS; i++)
        drawOneCloud(clouds[i].x, clouds[i].y, clouds[i].z, clouds[i].scale);
}

void updateClouds()
{
    for (int i = 0; i < NUM_CLOUDS; i++) {
        clouds[i].x -= clouds[i].speed;
        if (clouds[i].x < -60.0f) clouds[i].x = 60.0f;
    }
}

// ─────────────────────────────────────────────────────────
//  Birds
// ─────────────────────────────────────────────────────────
#define NUM_BIRDS 9
struct Bird { float x,y,z,speed,flapPhase,offsetX,offsetY; };
Bird birds[NUM_BIRDS];
float birdFlockX = 30.0f;

void initBirds()
{
    float offX[] = { 0,-3,-3,-6,-6,-9,-9,-12,-12 };
    float offY[] = { 0, 1,-1, 2,-2, 3,-3,  4, -4 };
    for (int i = 0; i < NUM_BIRDS; i++) {
        birds[i].offsetX   = offX[i];
        birds[i].offsetY   = offY[i];
        birds[i].flapPhase = i * 0.4f;
        birds[i].speed     = 0.05f;
        birds[i].y         = 22.0f + offY[i];
        birds[i].z         = -48.0f;
    }
}

void drawOneBird(float bx, float by, float bz, float flap)
{
    glDisable(GL_LIGHTING);
    glColor3f(0.08f, 0.08f, 0.08f);
    glLineWidth(2.2f);
    glBegin(GL_LINES);
      glVertex3f(bx-0.5f, by, bz); glVertex3f(bx+0.5f, by, bz);
    glEnd();
    float wingDip = sinf(flap) * 0.8f;
    glBegin(GL_LINE_STRIP);
      glVertex3f(bx,by,bz); glVertex3f(bx-1.2f,by+wingDip,bz); glVertex3f(bx-2.2f,by+wingDip*0.4f,bz);
    glEnd();
    glBegin(GL_LINE_STRIP);
      glVertex3f(bx,by,bz); glVertex3f(bx+1.2f,by+wingDip,bz); glVertex3f(bx+2.2f,by+wingDip*0.4f,bz);
    glEnd();
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

void drawBirds()
{
    for (int i = 0; i < NUM_BIRDS; i++)
        drawOneBird(birdFlockX+birds[i].offsetX, birds[i].y, birds[i].z, birds[i].flapPhase);
}

void updateBirds()
{
    birdFlockX -= 0.05f;
    if (birdFlockX < -70.0f) birdFlockX = 70.0f;
    for (int i = 0; i < NUM_BIRDS; i++) birds[i].flapPhase += 0.12f;
}

// ─────────────────────────────────────────────────────────
//  Utility
// ─────────────────────────────────────────────────────────
void drawBitmapString(float x, float y, void* font, const char* str)
{
    glRasterPos2f(x, y);
    while (*str) { glutBitmapCharacter(font, *str++); }
}

void enter2D()
{
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0, WIN_W, 0, WIN_H);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING);
}
void leave2D()
{
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
}

void spawnStars()
{
    starCount = 80;
    for (int i = 0; i < starCount; i++) {
        stars[i].x = WIN_W*0.5f+(rand()%200-100);
        stars[i].y = WIN_H*0.6f+(rand()%100-50);
        float ang = (rand()%360)*PI/180.0f;
        float spd = 2.0f+(rand()%400)/100.0f;
        stars[i].vx = cosf(ang)*spd; stars[i].vy = sinf(ang)*spd;
        stars[i].life = 1.0f;
        stars[i].r = 0.5f+(rand()%50)/100.0f;
        stars[i].g = 0.5f+(rand()%50)/100.0f;
        stars[i].b = (rand()%100)/100.0f;
    }
}

void resetBall()
{
    ballX=0; ballY=0.55f; ballZ=16.0f;
    ballDX=ballDY=ballDZ=0;
    ballRotX=ballRotZ=0; isShooting=false;
}

void triggerKeeperDive()
{
    keeperDiveX=keeperX; keeperDiveY=1.5f;
    keeperDiving=true; keeperDiveTimer=0;
}

void triggerCelebration(ShootTeam team)
{
    celebTimer = 4.0f;
    argCelebrating = (team == TEAM_ARGENTINA);
    braCelebrating = (team == TEAM_BRAZIL);
    // randomize per-fan phase offsets so celebration looks organic
    for (int c=0; c<14; c++)
        for (int r=0; r<5; r++)
            fanCelebPhase[c][r] = (rand()%100)/100.0f * PI;
}

void startRound(ShootTeam team)
{
    shootingTeam=team; score=0; shotsLeft=MAX_SHOTS; shotsTaken=0;
    for (int i=0;i<MAX_SHOTS;i++) shotResults[i]=0;
    resetBall(); keeperDiving=false; keeperX=0;
    starCount=0; p2MoveLeft=p2MoveRight=false;
    aimX=0.0f; aimY=2.0f;
    strcpy(feedbackMsg,""); feedbackTimer=0;
    argCelebrating=false; braCelebrating=false; celebTimer=0;
    penaltyAnimTimer  = 0.0f;   // ← ADD
    penaltyAnimActive = false;  // ← ADD
    gameState=STATE_PLAYING;
}

void initCelebPhases()
{
    for (int c=0; c<14; c++)
        for (int r=0; r<5; r++)
            fanCelebPhase[c][r] = (rand()%100)/100.0f * PI;
}

// ─────────────────────────────────────────────────────────
//  Lighting
// ─────────────────────────────────────────────────────────
void setupLighting()
{
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    GLfloat ambient[]  = { 0.3f,0.3f,0.3f,1.0f };
    GLfloat diffuse[]  = { 0.9f,0.85f,0.7f,1.0f };
    GLfloat specular[] = { 0.6f,0.6f,0.6f,1.0f };
    GLfloat lightPos[] = { 5.0f,20.0f,15.0f,1.0f };
    glLightfv(GL_LIGHT0,GL_AMBIENT,ambient); glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse);
    glLightfv(GL_LIGHT0,GL_SPECULAR,specular); glLightfv(GL_LIGHT0,GL_POSITION,lightPos);
    glEnable(GL_LIGHT1);
    GLfloat amb2[]  = { 0.1f,0.15f,0.2f,1.0f };
    GLfloat diff2[] = { 0.2f,0.2f,0.3f,1.0f };
    GLfloat pos2[]  = { -5.0f,5.0f,25.0f,1.0f };
    glLightfv(GL_LIGHT1,GL_AMBIENT,amb2); glLightfv(GL_LIGHT1,GL_DIFFUSE,diff2);
    glLightfv(GL_LIGHT1,GL_POSITION,pos2);
}

// ─────────────────────────────────────────────────────────
//  PITCH
// ─────────────────────────────────────────────────────────
void drawPitch()
{
    // Outer terracotta run-off border
    glDisable(GL_LIGHTING);
    glColor3f(0.68f, 0.32f, 0.14f);
    glBegin(GL_QUADS);
      glNormal3f(0,1,0);
      glVertex3f(-25,-0.02f,-25); glVertex3f( 25,-0.02f,-25);
      glVertex3f( 25,-0.02f, 25); glVertex3f(-25,-0.02f, 25);
    glEnd();

  // ── Outer pitch boundary — thick bright-white double-line ────
    glDisable(GL_LIGHTING);

    // Outer line (at field edge)
    glColor3f(0.0f, 0.0f, 0.0f);
    glLineWidth(6.0f);
    glBegin(GL_LINE_LOOP);
      glVertex3f(-22.5f, 0.02f,-22.5f);
      glVertex3f( 22.5f, 0.02f,-22.5f);
      glVertex3f( 22.5f, 0.02f, 22.5f);
      glVertex3f(-22.5f, 0.02f, 22.5f);
    glEnd();

    // Inner line (inset 0.55 units — classic double-border look)
    glColor3f(0.95f, 0.95f, 0.95f);
    glLineWidth(3.5f);
    glBegin(GL_LINE_LOOP);
      glVertex3f(-21.95f, 0.02f,-21.95f);
      glVertex3f( 21.95f, 0.02f,-21.95f);
      glVertex3f( 21.95f, 0.02f, 21.95f);
      glVertex3f(-21.95f, 0.02f, 21.95f);
    glEnd();

    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);

    // Main grass
    glColor3f(0.13f, 0.55f, 0.13f);
    glBegin(GL_QUADS);
      glNormal3f(0,1,0);
      glVertex3f(-22.5f,-0.01f,-22.5f); glVertex3f( 22.5f,-0.01f,-22.5f);
      glVertex3f( 22.5f,-0.01f, 22.5f); glVertex3f(-22.5f,-0.01f, 22.5f);
    glEnd();

    // Alternating mowing stripes
    glDisable(GL_LIGHTING);
    for (int i = -8; i <10; i++) {
        if (i % 2 == 0) {
            glColor4f(0.15f, 0.58f, 0.15f, 0.6f);
            glBegin(GL_QUADS);
              glVertex3f(i*2.5f,0.0f,-20.0f); glVertex3f((i+1)*2.5f,0.0f,-20.0f);
              glVertex3f((i+1)*2.5f,0.0f,20.0f); glVertex3f(i*2.5f,0.0f,20.0f);
            glEnd();
        }
    }

   // ── Field markings ────────────────────────────────────────────
    glColor3f(1, 1, 1);
    glLineWidth(2.5f);

    // Penalty spot
    glPointSize(7.0f);
    glBegin(GL_POINTS); glVertex3f(0, 0.01f, 16.0f); glEnd();
    glPointSize(1.0f);

    // Penalty arc (centred on spot, front face only)
    glBegin(GL_LINE_STRIP);
    for (int a = -60; a <= 60; a += 2) {
        float rad = a * PI / 180.0f;
        glVertex3f(4.5f * sinf(rad), 0.01f, 16.0f - 4.5f * cosf(rad));
    }
    glEnd();

    // ── Large penalty area  (expanded: ±14 wide) ─────────────────
    glBegin(GL_LINE_LOOP);
      glVertex3f(-14.0f, 0.01f, GOAL_Z);
      glVertex3f( 14.0f, 0.01f, GOAL_Z);
      glVertex3f( 14.0f, 0.01f, 13.0f);
      glVertex3f(-14.0f, 0.01f, 13.0f);
    glEnd();

    // ── Small goal box  (expanded: ±8 wide) ──────────────────────
    glBegin(GL_LINE_LOOP);
      glVertex3f(-8.0f, 0.01f, GOAL_Z);
      glVertex3f( 8.0f, 0.01f, GOAL_Z);
      glVertex3f( 8.0f, 0.01f, -7.0f);
      glVertex3f(-8.0f, 0.01f, -7.0f);
    glEnd();

    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

// ─────────────────────────────────────────────────────────
//  CORNER FLAGS
// ─────────────────────────────────────────────────────────


// ─────────────────────────────────────────────────────────
//  Goal Post
// ─────────────────────────────────────────────────────────
void drawGoalPost()
{
    glColor3f(0.0f, 0.0f, 0.0f);   // black posts
    GLUquadric* q = gluNewQuadric();
    gluQuadricNormals(q, GLU_SMOOTH);
    auto cyl = [&](float tx,float ty,float tz,float rx,float ry,float rz,float ra,float len,float r){
        glPushMatrix(); glTranslatef(tx,ty,tz); glRotatef(ra,rx,ry,rz);
        gluCylinder(q,r,r,len,12,1); glPopMatrix();
    };
    cyl(GOAL_LEFT, 0,GOAL_Z,        1,0,0,-90, GOAL_TOP,            0.12f);
    cyl(GOAL_RIGHT,0,GOAL_Z,        1,0,0,-90, GOAL_TOP,            0.12f);
    cyl(GOAL_LEFT, GOAL_TOP,GOAL_Z, 0,1,0, 90, GOAL_RIGHT-GOAL_LEFT,0.12f);
    cyl(GOAL_LEFT, GOAL_TOP,GOAL_Z-2.5f,0,1,0,90,GOAL_RIGHT-GOAL_LEFT,0.08f);
    cyl(GOAL_LEFT, 0,GOAL_Z-2.5f,  1,0,0,-90, GOAL_TOP,            0.08f);
    cyl(GOAL_RIGHT,0,GOAL_Z-2.5f,  1,0,0,-90, GOAL_TOP,            0.08f);
    gluDeleteQuadric(q);

    glDisable(GL_LIGHTING);
    glColor4f(1,1,1,0.25f);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    for (float nx=GOAL_LEFT; nx<=GOAL_RIGHT+0.01f; nx+=0.6f) {
        glBegin(GL_LINES);
          glVertex3f(nx,0.0f,GOAL_Z);   glVertex3f(nx,GOAL_TOP,GOAL_Z);
          glVertex3f(nx,0.0f,GOAL_Z);   glVertex3f(nx,0.0f,GOAL_Z-2.5f);
        glEnd();
    }
    for (float ny=0; ny<=GOAL_TOP+0.01f; ny+=0.6f) {
        glBegin(GL_LINES);
          glVertex3f(GOAL_LEFT,ny,GOAL_Z); glVertex3f(GOAL_RIGHT,ny,GOAL_Z);
        glEnd();
    }
    glDisable(GL_BLEND); glEnable(GL_LIGHTING);
}

// ─────────────────────────────────────────────────────────
//  Goalkeeper
// ─────────────────────────────────────────────────────────
void drawGoalkeeper()
{
    float kx, ky;
    if (keeperDiving) {
        float t = keeperDiveTimer / 0.5f; if (t>1) t=1;
        kx = keeperX + (keeperDiveX-keeperX)*t;
        ky = keeperY + (keeperDiveY-keeperY)*t;
    } else { kx=keeperX; ky=1.5f; }

    float jr,jg,jb,sr,sg,sb,gr,gg,gb;
    if (shootingTeam==TEAM_ARGENTINA) {
        jr=0.0f;jg=0.5f;jb=0.1f; sr=0.05f;sg=0.05f;sb=0.65f; gr=0.98f;gg=0.82f;gb=0.0f;
    } else {
        jr=0.44f;jg=0.72f;jb=0.96f; sr=0.08f;sg=0.08f;sb=0.55f; gr=1.0f;gg=1.0f;gb=1.0f;
    }
    glPushMatrix(); glTranslatef(kx,ky,GOAL_Z+0.3f);
    glColor3f(jr,jg,jb); glPushMatrix(); glScalef(0.7f,1.0f,0.4f); glutSolidCube(1.0f); glPopMatrix();
    glColor3f(gr,gg,gb); glPushMatrix(); glTranslatef(0,0.05f,0.21f); glScalef(0.5f,0.55f,0.05f); glutSolidCube(1.0f); glPopMatrix();
    glColor3f(0.85f,0.65f,0.45f); glPushMatrix(); glTranslatef(0,1.0f,0); glutSolidSphere(0.38f,12,12); glPopMatrix();
    glColor3f(gr,gg,gb);
    glPushMatrix(); glTranslatef(-0.65f,0.2f,0); glRotatef(keeperDiving?-30:60,0,0,1); glScalef(0.2f,0.7f,0.2f); glutSolidCube(1.0f); glPopMatrix();
    glPushMatrix(); glTranslatef( 0.65f,0.2f,0); glRotatef(keeperDiving? 30:-60,0,0,1); glScalef(0.2f,0.7f,0.2f); glutSolidCube(1.0f); glPopMatrix();
    glColor3f(sr,sg,sb);
    glPushMatrix(); glTranslatef(-0.2f,-0.9f,0); glScalef(0.25f,0.7f,0.25f); glutSolidCube(1.0f); glPopMatrix();
    glPushMatrix(); glTranslatef( 0.2f,-0.9f,0); glScalef(0.25f,0.7f,0.25f); glutSolidCube(1.0f); glPopMatrix();
    glColor3f(0.05f,0.05f,0.05f);
    glPushMatrix(); glTranslatef(-0.2f,-1.3f,0.1f); glScalef(0.28f,0.22f,0.45f); glutSolidCube(1.0f); glPopMatrix();
    glPushMatrix(); glTranslatef( 0.2f,-1.3f,0.1f); glScalef(0.28f,0.22f,0.45f); glutSolidCube(1.0f); glPopMatrix();
    glPopMatrix();
}

// ─────────────────────────────────────────────────────────
//  Ball
// ─────────────────────────────────────────────────────────
void drawBall()
{
    glPushMatrix();
      glTranslatef(ballX,ballY,ballZ);
      glRotatef(ballRotX,1,0,0); glRotatef(ballRotZ,0,0,1);
      glColor3f(1,1,1); glutSolidSphere(0.45f,24,24);
      glColor3f(0.05f,0.05f,0.05f);
      float offs[][3]={{0,0.45f,0},{0,-0.45f,0},{0.45f,0,0},{-0.45f,0,0},{0,0,0.45f},{0,0,-0.45f}};
      for (int i=0;i<6;i++){
          glPushMatrix(); glTranslatef(offs[i][0]*0.6f,offs[i][1]*0.6f,offs[i][2]*0.6f);
          glutSolidSphere(0.12f,8,8); glPopMatrix();
      }
    glPopMatrix();
}


// ─────────────────────────────────────────────────────────
//  Penalty Taker  (articulated kick: backswing → impact → follow-through → recovery)
// ─────────────────────────────────────────────────────────
void drawPenaltyTaker()
{
    // Fade taker out at the very end of the recovery phase
    if (penaltyAnimActive && penaltyAnimTimer > 1.44f) return;

    float t = penaltyAnimActive ? penaltyAnimTimer : 0.0f;

    // ── Animation parameters ─────────────────────────────────
    //   thighAngle : kicking upper-leg rotation  (+ve = forward / toward goal)
    //   shinBend   : knee bend, shin folds *backward* relative to thigh (+ve = bent)
    //   bodyLean   : torso forward lean (degrees)
    //   plantShift : plant-leg slight lean for weight transfer
    float thighAngle = 0.0f, shinBend = 12.0f;
    float bodyLean   = 5.0f, plantShift = 0.0f;

    if (!penaltyAnimActive) {
        // IDLE — natural relaxed stance with outer bob
        thighAngle = 0.0f;   shinBend = 12.0f;
        bodyLean   = 5.0f;   plantShift = 0.0f;

    } else if (t < 0.14f) {
        // PHASE 1 — BACKSWING  (leg pulls back, knee bends)
        float u = t / 0.14f;
        thighAngle  =  0.0f - 45.0f * u;
        shinBend    = 12.0f + 33.0f * u;   // deep knee bend
        bodyLean    =  5.0f + 10.0f * u;
        plantShift  =  0.0f +  5.0f * u;

    } else if (t < 0.32f) {
        // PHASE 2 — KICK FORWARD  (rapid snap, knee straightens at ball contact)
        float u = (t - 0.14f) / 0.18f;
        thighAngle  = -45.0f + 130.0f * u;   // −45 → +85
        shinBend    =  45.0f * (1.0f - u);   // knee whips straight at contact
        bodyLean    =  15.0f +  12.0f * u;
        plantShift  =   5.0f -   8.0f * u;

    } else if (t < 0.85f) {
        // PHASE 3 — FOLLOW-THROUGH  (leg high, body committed forward)
        float u = (t - 0.32f) / 0.53f;
        thighAngle  = 85.0f - 25.0f * u;
        shinBend    =  0.0f + 18.0f * u;
        bodyLean    = 27.0f - 12.0f * u;
        plantShift  = -3.0f +  3.0f * u;

    } else {
        // PHASE 4 — RECOVERY  (return to upright stance)
        float u = (t - 0.85f) / 0.60f;
        if (u > 1.0f) u = 1.0f;
        thighAngle  = 60.0f - 60.0f * u;
        shinBend    = 18.0f -  8.0f * u;
        bodyLean    = 15.0f - 10.0f * u;
        plantShift  =  0.0f;
    }

    // Body bob only while idle
    float bob = penaltyAnimActive ? 0.0f : sinf(idleTimer) * 0.05f;

    // ── Team colours ─────────────────────────────────────────
    float jr, jg, jb, sr, sg, sb;
    if (shootingTeam == TEAM_ARGENTINA) {
        jr=0.45f; jg=0.72f; jb=0.95f;   // sky-blue jersey
        sr=0.05f; sg=0.05f; sb=0.60f;   // navy shorts
    } else {
        jr=0.98f; jg=0.82f; jb=0.00f;   // yellow jersey
        sr=0.00f; sg=0.50f; sb=0.12f;   // green shorts
    }

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 17.8f);   // stand just behind penalty spot

    // ── TORSO ────────────────────────────────────────────────
    glColor3f(jr, jg, jb);
    glPushMatrix();
      glTranslatef(0.0f, 1.25f + bob, 0.0f);
      glRotatef(-bodyLean, 1, 0, 0);          // lean forward into kick
      glScalef(0.52f, 0.70f, 0.30f);
      glutSolidCube(1.0f);
    glPopMatrix();

    // ── HEAD ─────────────────────────────────────────────────
    glColor3f(0.85f, 0.65f, 0.45f);
    glPushMatrix();
      glTranslatef(0.0f, 1.90f + bob, -bodyLean * 0.008f);
      glutSolidSphere(0.24f, 10, 10);
    glPopMatrix();

    // ── ARMS (opposite-phase counter-balance) ─────────────────
    glColor3f(jr, jg, jb);
    float armSwing = thighAngle * 0.35f;

    // Left arm — swings forward as kicking leg goes back, back on kick
    glPushMatrix();
      glTranslatef(-0.38f, 1.25f + bob, 0.0f);
      glRotatef(-bodyLean, 1, 0, 0);
      glRotatef(-armSwing, 1, 0, 0);
      glRotatef(12.0f, 0, 0, 1);
      glScalef(0.16f, 0.55f, 0.16f);
      glutSolidCube(1.0f);
    glPopMatrix();

    // Right arm — opposite phase
    glPushMatrix();
      glTranslatef(0.38f, 1.25f + bob, 0.0f);
      glRotatef(-bodyLean, 1, 0, 0);
      glRotatef( armSwing, 1, 0, 0);
      glRotatef(-12.0f, 0, 0, 1);
      glScalef(0.16f, 0.55f, 0.16f);
      glutSolidCube(1.0f);
    glPopMatrix();

    // ── PLANT LEG — LEFT (weight-bearing) ────────────────────
    glColor3f(sr, sg, sb);
    glPushMatrix();
      glTranslatef(-0.16f, 0.9f + bob, 0.0f);
      glRotatef(plantShift, 1, 0, 0);

      // Upper leg
      glPushMatrix();
        glTranslatef(0, -0.25f, 0);
        glScalef(0.22f, 0.50f, 0.22f);
        glutSolidCube(1.0f);
      glPopMatrix();

      // Knee joint → lower leg
      glPushMatrix();
        glTranslatef(0, -0.50f, 0);
        glRotatef(-8.0f, 1, 0, 0);    // natural slight bend
        glPushMatrix();
          glTranslatef(0, -0.25f, 0);
          glScalef(0.19f, 0.50f, 0.19f);
          glutSolidCube(1.0f);
        glPopMatrix();
        // Left boot
        glPushMatrix();
          glTranslatef(0, -0.50f, 0);
          glColor3f(0.05f, 0.05f, 0.05f);
          glTranslatef(0, -0.08f, 0.10f);
          glScalef(0.22f, 0.16f, 0.44f);
          glutSolidCube(1.0f);
        glPopMatrix();
      glPopMatrix();
    glPopMatrix();

    // ── KICKING LEG — RIGHT (fully articulated hip→knee→ankle) ─
    glColor3f(sr, sg, sb);
    glPushMatrix();
      glTranslatef(0.16f, 0.9f + bob, 0.0f);
      glRotatef(thighAngle, 1, 0, 0);    // hip rotation: +ve = forward toward goal

      // Upper leg (thigh)
      glPushMatrix();
        glTranslatef(0, -0.25f, 0);
        glScalef(0.22f, 0.50f, 0.22f);
        glutSolidCube(1.0f);
      glPopMatrix();

      // Knee joint → lower leg (shin)
      glPushMatrix();
        glTranslatef(0, -0.50f, 0);
        glRotatef(-shinBend, 1, 0, 0);   // -ve = shin folds backward at knee
        glPushMatrix();
          glTranslatef(0, -0.25f, 0);
          glScalef(0.19f, 0.50f, 0.19f);
          glutSolidCube(1.0f);
        glPopMatrix();
        // Right boot (kicking foot)
        glPushMatrix();
          glTranslatef(0, -0.50f, 0);
          glColor3f(0.05f, 0.05f, 0.05f);
          glTranslatef(0, -0.08f, 0.10f);
          glScalef(0.22f, 0.16f, 0.44f);
          glutSolidCube(1.0f);
        glPopMatrix();
      glPopMatrix();
    glPopMatrix();

    glPopMatrix();  // taker base
}

// ─────────────────────────────────────────────────────────
//  Crosshair
// ─────────────────────────────────────────────────────────
void drawAimCrosshair()
{
    if (isShooting) return;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.5f);
    float az = GOAL_Z + 0.05f;
    glColor4f(1.0f, 0.2f, 0.2f, 0.85f);
    glBegin(GL_LINES);
      glVertex3f(aimX-0.7f, aimY, az); glVertex3f(aimX+0.7f, aimY, az);
    glEnd();
    glBegin(GL_LINES);
      glVertex3f(aimX, aimY-0.35f, az); glVertex3f(aimX, aimY+0.35f, az);
    glEnd();
    glBegin(GL_LINE_LOOP);
    for (int a=0;a<36;a++){
        float r = a*10.0f*PI/180.0f;
        glVertex3f(aimX+0.4f*cosf(r), aimY+0.4f*sinf(r), az);
    }
    glEnd();
    glLineWidth(1.0f); glDisable(GL_BLEND); glEnable(GL_LIGHTING);
}

// ─────────────────────────────────────────────────────────
//  STADIUM SEATS — concrete stepped tiers
// ─────────────────────────────────────────────────────────

// Draw a concrete seat platform (one row of seats)
void drawSeatTier(float x1, float x2, float y, float z, float depth,
                  float r, float g, float b)
{
    // Seat platform
    glColor3f( 0.4f, 0.5f, 0.5f);
    glBegin(GL_QUADS);
      glNormal3f(0,1,0);
      glVertex3f(x1, y,       z);
      glVertex3f(x2, y,       z);
      glVertex3f(x2, y,       z - depth);
      glVertex3f(x1, y,       z - depth);
    glEnd();
    // Riser (step face)
     glColor3f(0.4f, 0.5f,  0.5f);
    glBegin(GL_QUADS);
      glNormal3f(0,0,1);
      glVertex3f(x1, y,        z);
      glVertex3f(x2, y,        z);
      glVertex3f(x2, y - 0.45f, z);
      glVertex3f(x1, y - 0.45f, z);
    glEnd();
}
void drawCornerFlag(float fx, float fz)
{
    glDisable(GL_LIGHTING);

    // ── Pole ─────────────────────────────────────────────────────
    glColor3f(0.90f, 0.90f, 0.90f);
    glLineWidth(4.5f);
    glBegin(GL_LINES);
      glVertex3f(fx, 0.0f, fz);
      glVertex3f(fx, 2.85f, fz);
    glEnd();
    glLineWidth(1.0f);

    // ── Rippling flag (8 vertical quad-strips, phase-offset per column) ──
    float side   = (fx < 0) ? 1.0f : -1.0f;   // flag faces inward
    float flagW  = 1.35f;   // total width
    float flagH  = 0.58f;   // height
    float topY   = 2.75f;
    const int SEGS = 8;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Main flag fill
   glColor4f(0.95f, 0.08f, 0.08f, 0.97f);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= SEGS; i++) {
        float u     = (float)i / SEGS;
        float wx    = fx + side * flagW * u;
        float phase = flagWave + u * 2.8f;        // phase builds along length
        float ampY  = 0.20f * u;                  // zero at pole, full at tip
        float ampZ  = 0.09f * u;
        float waveY = sinf(phase)       * ampY;
        float waveZ = cosf(phase + 0.5f)* ampZ;
        glVertex3f(wx, topY          + waveY, fz + waveZ);
        glVertex3f(wx, topY - flagH  + waveY * 0.55f, fz + waveZ * 0.55f);
    }
    glEnd();

    // Dark outline — top edge
    glColor4f(0.0f, 0.0f, 0.0f, 0.55f);
    glLineWidth(1.5f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= SEGS; i++) {
        float u     = (float)i / SEGS;
        float wx    = fx + side * flagW * u;
        float phase = flagWave + u * 2.8f;
        float waveY = sinf(phase)        * 0.20f * u;
        float waveZ = cosf(phase + 0.5f) * 0.09f * u;
        glVertex3f(wx, topY + waveY, fz + waveZ);
    }
    glEnd();

    // Dark outline — bottom edge
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= SEGS; i++) {
        float u     = (float)i / SEGS;
        float wx    = fx + side * flagW * u;
        float phase = flagWave + u * 2.8f;
        float waveY = sinf(phase)        * 0.20f * u * 0.55f;
        float waveZ = cosf(phase + 0.5f) * 0.09f * u * 0.55f;
        glVertex3f(wx, topY - flagH + waveY, fz + waveZ);
    }
    glEnd();

    glLineWidth(1.0f);
    glDisable(GL_BLEND);

    // ── Gold ball finial on pole tip ─────────────────────────────
    glColor3f(0.85f, 0.05f, 0.05f);
    glPushMatrix();
      glTranslatef(fx, 2.88f, fz);
      glutSolidSphere(0.11f, 10, 10);
    glPopMatrix();

    glEnable(GL_LIGHTING);
}

void drawCornerFlags()
{
    // Front corners of penalty area (fully visible from camera)
    drawCornerFlag(-14.0f,  13.0f);   // front-left
    drawCornerFlag( 14.0f,  13.0f);   // front-right

    // Back corners of penalty area (at goal line)
    drawCornerFlag(-14.0f, GOAL_Z);   // back-left
    drawCornerFlag( 14.0f, GOAL_Z);   // back-right
}

void drawStadiumSeats()
{
    // ── BACK STAND — 5 concrete tiers ──
    for (int row = 0; row < 5; row++) {
        float y = 1.8f + row * 1.0f;
        float z = -21.5f - row * 0.9f;
        // Back left: Argentina blue
        drawSeatTier(-24.0f, 0.0f, y, z, 0.85f, 0.35f, 0.6f, 0.92f);
        // Back right: Brazil yellow
        drawSeatTier(0.0f, 24.0f, y, z, 0.85f, 0.9f, 0.78f, 0.15f);
    }

    // ── LEFT SIDE STAND — 5 tiers, Argentina blue ──
    for (int row = 0; row < 5; row++) {
        float x = -23.5f - row * 1.0f;
        float y  = 1.8f  + row * 1.0f;
        // Draw as a face along the z-axis
        glColor3f(0.25f, 0.50f, 0.80f);
        glBegin(GL_QUADS);
          glNormal3f(1,0,0);
          glVertex3f(x, y,        -22.0f);
          glVertex3f(x, y,         21.0f);
          glVertex3f(x+0.85f, y,   21.0f);
          glVertex3f(x+0.85f, y,  -22.0f);
        glEnd();
        // riser
        glColor3f(0.18f, 0.35f, 0.58f);
        glBegin(GL_QUADS);
          glNormal3f(-1,0,0);
          glVertex3f(x, y,         -22.0f);
          glVertex3f(x, y,          21.0f);
          glVertex3f(x, y - 0.45f,  21.0f);
          glVertex3f(x, y - 0.45f, -22.0f);
        glEnd();
    }

    // ── RIGHT SIDE STAND — 5 tiers, Brazil yellow ──
    for (int row = 0; row < 5; row++) {
        float x = 23.5f + row * 1.0f;
        float y  = 1.8f  + row * 1.0f;
        glColor3f(0.80f, 0.70f, 0.10f);
        glBegin(GL_QUADS);
          glNormal3f(-1,0,0);
          glVertex3f(x, y,        -22.0f);
          glVertex3f(x, y,         21.0f);
          glVertex3f(x-0.85f, y,   21.0f);
          glVertex3f(x-0.85f, y,  -22.0f);
        glEnd();
        glColor3f(0.58f, 0.50f, 0.08f);
        glBegin(GL_QUADS);
          glNormal3f(1,0,0);
          glVertex3f(x, y,         -22.0f);
          glVertex3f(x, y,          21.0f);
          glVertex3f(x, y - 0.45f,  21.0f);
          glVertex3f(x, y - 0.45f, -22.0f);
        glEnd();
    }
}

// ─────────────────────────────────────────────────────────
//  SEATED FAN — draws one fan either sitting or standing/celebrating
//
//  sitting:    compact torso+head, knees raised
//  standing:   full upright figure, arms raised if celebrating
// ─────────────────────────────────────────────────────────
void drawSeatedFan(float x, float y, float z,
                   float jr, float jg, float jb,
                   float pr, float pg, float pb,
                   bool celebrating, float phase)
{
    glPushMatrix();
    glTranslatef(x, y, z);

    if (!celebrating) {
        // ── SEATED POSE ──
        // Torso (small, leaning slightly back)
        glColor3f(jr, jg, jb);
        glPushMatrix();
          glRotatef(10, 1, 0, 0);
          glScalef(0.32f, 0.42f, 0.22f);
          glutSolidCube(1.0f);
        glPopMatrix();
        // Head
        glColor3f(0.82f, 0.62f, 0.44f);
        glPushMatrix();
          glTranslatef(0, 0.38f, 0);
          glutSolidSphere(0.16f, 7, 7);
        glPopMatrix();
        // Thighs (horizontal)
        glColor3f(pr, pg, pb);
        glPushMatrix();
          glTranslatef(-0.10f, -0.22f, 0.12f);
          glRotatef(-80, 1, 0, 0);
          glScalef(0.10f, 0.30f, 0.10f);
          glutSolidCube(1.0f);
        glPopMatrix();
        glPushMatrix();
          glTranslatef( 0.10f, -0.22f, 0.12f);
          glRotatef(-80, 1, 0, 0);
          glScalef(0.10f, 0.30f, 0.10f);
          glutSolidCube(1.0f);
        glPopMatrix();
        // Arms resting on lap
        glColor3f(jr, jg, jb);
        glPushMatrix();
          glTranslatef(-0.20f, 0.05f, 0.06f);
          glRotatef(-50, 1, 0, 0);
          glScalef(0.10f, 0.28f, 0.10f);
          glutSolidCube(1.0f);
        glPopMatrix();
        glPushMatrix();
          glTranslatef( 0.20f, 0.05f, 0.06f);
          glRotatef(-50, 1, 0, 0);
          glScalef(0.10f, 0.28f, 0.10f);
          glutSolidCube(1.0f);
        glPopMatrix();
    } else {
        // ── STANDING / CELEBRATING POSE ──
        float bob  = sinf(phase) * 0.07f;            // body bob
        float wave = sinf(phase + PI*0.5f) * 22.0f;  // arm wave angle
        glTranslatef(0, 0.35f + bob, 0);

        // Torso
        glColor3f(jr, jg, jb);
        glPushMatrix();
          glScalef(0.32f, 0.50f, 0.22f);
          glutSolidCube(1.0f);
        glPopMatrix();
        // Head
        glColor3f(0.82f, 0.62f, 0.44f);
        glPushMatrix();
          glTranslatef(0, 0.42f, 0);
          glutSolidSphere(0.17f, 7, 7);
        glPopMatrix();
        // Arms raised and waving
        glColor3f(jr, jg, jb);
        glPushMatrix();
          glTranslatef(-0.22f, 0.18f, 0);
          glRotatef(-(110.0f + wave), 0, 0, 1);
          glScalef(0.10f, 0.38f, 0.10f);
          glutSolidCube(1.0f);
        glPopMatrix();
        glPushMatrix();
          glTranslatef( 0.22f, 0.18f, 0);
          glRotatef( (110.0f + wave), 0, 0, 1);
          glScalef(0.10f, 0.38f, 0.10f);
          glutSolidCube(1.0f);
        glPopMatrix();
        // Legs standing
        glColor3f(pr, pg, pb);
        glPushMatrix();
          glTranslatef(-0.10f, -0.45f, 0);
          glScalef(0.12f, 0.40f, 0.12f);
          glutSolidCube(1.0f);
        glPopMatrix();
        glPushMatrix();
          glTranslatef( 0.10f, -0.45f, 0);
          glScalef(0.12f, 0.40f, 0.12f);
          glutSolidCube(1.0f);
        glPopMatrix();
    }

    glPopMatrix();
}

// ─────────────────────────────────────────────────────────
//  CROWD — draws all fans with seats visible beneath them
// ─────────────────────────────────────────────────────────
void drawCrowd()
{
    // Draw the tiered concrete platform first
    drawStadiumSeats();

    glDisable(GL_LIGHTING);

    // Palettes
    const float ARG_J[3]  = { 0.45f, 0.72f, 0.95f };
    const float ARG_J2[3] = { 1.00f, 1.00f, 1.00f };
    const float ARG_P[3]  = { 0.10f, 0.10f, 0.55f };
    const float BRA_J[3]  = { 0.98f, 0.82f, 0.00f };
    const float BRA_J2[3] = { 0.00f, 0.50f, 0.12f };
    const float BRA_P[3]  = { 0.05f, 0.05f, 0.60f };

    // ── BACK STAND — 5 rows ──
    int numBackCols = 28;
    for (int row = 0; row < 5; row++) {
        float fanY = 2.25f + row * 1.0f;
        float fanZ = -21.5f - row * 0.9f;
        for (int col = -18; col < numBackCols - 10; col++) {
            float fanX = col * 1.25f + 0.62f;
            bool alt = ((col + row) % 2 == 0);

            bool isArg = (col < -1);
            bool isBra = (col >= 1);
            bool isMix = (!isArg && !isBra);

            const float* j; const float* p;
            bool celebrating = false;

            if (isArg || (isMix && (((col+row)+14)%2!=0))) {
                j = alt ? ARG_J : ARG_J2;
                p = ARG_P;
                celebrating = argCelebrating;
            } else {
                j = alt ? BRA_J : BRA_J2;
                p = BRA_P;
                celebrating = braCelebrating;
            }

            float phase = fanCelebPhase[col+10 >= 0 ? (col+10) % 14 : 0][row % 5];
            if (celebrating) phase += celebTimer * 4.0f;

            drawSeatedFan(fanX, fanY, fanZ,
                          j[0],j[1],j[2], p[0],p[1],p[2],
                          celebrating, phase);
        }
    }

    // ── LEFT SIDE STAND — ALL Argentina ──
    for (int row = 0; row < 5; row++) {
        float fanX = -24.1f - row * 1.0f;
        float fanY =  2.3f  + row * 1.0f;
        for (int col = 0; col < 12; col++) {
            float fanZ = -17.0f + col * 3.2f;
            bool alt = (col + row) % 2 == 0;
            const float* j = alt ? ARG_J : ARG_J2;

            float phase = fanCelebPhase[col % 14][row % 5];
            if (argCelebrating) phase += celebTimer * 4.0f;

            drawSeatedFan(fanX, fanY, fanZ,
                          j[0],j[1],j[2], ARG_P[0],ARG_P[1],ARG_P[2],
                          argCelebrating, phase);
        }
    }

    // ── RIGHT SIDE STAND — ALL Brazil ──
    for (int row = 0; row < 5; row++) {
        float fanX = 24.1f + row * 1.0f;
        float fanY =  2.3f  + row * 1.0f;
        for (int col = 0; col < 12; col++) {
            float fanZ = -17.0f + col * 3.2f;
            bool alt = (col + row) % 2 == 0;
            const float* j = alt ? BRA_J : BRA_J2;

            float phase = fanCelebPhase[col % 14][row % 5];
            if (braCelebrating) phase += celebTimer * 4.0f;

            drawSeatedFan(fanX, fanY, fanZ,
                          j[0],j[1],j[2], BRA_P[0],BRA_P[1],BRA_P[2],
                          braCelebrating, phase);
        }
    }

    // Concrete back wall
    glColor3f(0.28f, 0.28f, 0.28f);
    glBegin(GL_QUADS);
      glVertex3f(-26,0,-26); glVertex3f(26,0,-26);
      glVertex3f( 26,9,-28); glVertex3f(-26,9,-28);
    glEnd();

    // Stadium roof rim (back)
    glColor3f(0.72f, 0.72f, 0.72f);
    glBegin(GL_QUADS);
      glVertex3f(-27, 8.5f, -27); glVertex3f(27, 8.5f, -27);
      glVertex3f( 27, 9.2f, -25); glVertex3f(-27, 9.2f, -25);
    glEnd();
    // Left roof rim
    glBegin(GL_QUADS);
      glVertex3f(-27.5f, 7.5f, -25); glVertex3f(-27.5f, 7.5f, 22);
      glVertex3f(-28.5f, 8.5f, 22); glVertex3f(-28.5f, 8.5f, -25);
    glEnd();
    // Right roof rim
    glBegin(GL_QUADS);
      glVertex3f(27.5f, 7.5f, -25); glVertex3f(27.5f, 7.5f, 22);
      glVertex3f(28.5f, 8.5f, 22); glVertex3f(28.5f, 8.5f, -25);
    glEnd();

    glEnable(GL_LIGHTING);
}

// ─────────────────────────────────────────────────────────
//  Sky
// ─────────────────────────────────────────────────────────
void drawSky()
{
    glDisable(GL_LIGHTING);
    glBegin(GL_QUADS);
      glColor3f(0.35f,0.62f,0.95f); glVertex3f(-60,0,-60); glVertex3f(60,0,-60);
      glColor3f(0.10f,0.30f,0.75f); glVertex3f(60,40,-60); glVertex3f(-60,40,-60);
    glEnd();
    glColor3f(1.0f,0.95f,0.5f);
    glPushMatrix(); glTranslatef(12,22,-55); glutSolidSphere(2.5f,16,16); glPopMatrix();
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f,0.95f,0.4f,0.15f);
    glPushMatrix(); glTranslatef(12,22,-55); glutSolidSphere(5.0f,16,16); glPopMatrix();
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    drawClouds();
    drawBirds();
}

// ─────────────────────────────────────────────────────────
//  HUD
// ─────────────────────────────────────────────────────────
void drawShotIndicators()
{
    enter2D();
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int i = 0; i < MAX_SHOTS; i++) {
        float cx = WIN_W - 150 + i * 28.0f;
        float cy = WIN_H - 45.0f;
        float r  = 10.0f;

        int res = shotResults[i];  // 0=pending, 1=goal, -1=missed

        // Fill colour: black=pending, green=goal, red=missed
        if      (res ==  1) glColor4f(0.10f, 0.85f, 0.20f, 1.0f);  // green
        else if (res == -1) glColor4f(0.90f, 0.15f, 0.15f, 1.0f);  // red
        else                glColor4f(0.08f, 0.08f, 0.08f, 1.0f);  // black

        // Filled circle
        glBegin(GL_TRIANGLE_FAN);
          glVertex2f(cx, cy);
          for (int a = 0; a <= 32; a++) {
              float ang = a * (2.0f * PI / 32.0f);
              glVertex2f(cx + r * cosf(ang), cy + r * sinf(ang));
          }
        glEnd();

        // White outline ring
        glColor4f(1.0f, 1.0f, 1.0f, 0.85f);
        glLineWidth(1.8f);
        glBegin(GL_LINE_LOOP);
          for (int a = 0; a < 32; a++) {
              float ang = a * (2.0f * PI / 32.0f);
              glVertex2f(cx + r * cosf(ang), cy + r * sinf(ang));
          }
        glEnd();
        glLineWidth(1.0f);

        // Small icon inside: checkmark for goal, X for missed, dot for pending
        if (res == 1) {
            // Checkmark
            glColor3f(1.0f, 1.0f, 1.0f);
            glLineWidth(2.0f);
            glBegin(GL_LINE_STRIP);
              glVertex2f(cx - 5.0f, cy);
              glVertex2f(cx - 1.5f, cy - 4.5f);
              glVertex2f(cx + 5.5f, cy + 5.0f);
            glEnd();
            glLineWidth(1.0f);
        } else if (res == -1) {
            // X mark
            glColor3f(1.0f, 1.0f, 1.0f);
            glLineWidth(2.0f);
            glBegin(GL_LINES);
              glVertex2f(cx - 4.5f, cy - 4.5f); glVertex2f(cx + 4.5f, cy + 4.5f);
              glVertex2f(cx + 4.5f, cy - 4.5f); glVertex2f(cx - 4.5f, cy + 4.5f);
            glEnd();
            glLineWidth(1.0f);
        } else {
            // Small dot for pending
            glColor4f(0.5f, 0.5f, 0.5f, 0.8f);
            glBegin(GL_TRIANGLE_FAN);
              glVertex2f(cx, cy);
              for (int a = 0; a <= 16; a++) {
                  float ang = a * (2.0f * PI / 16.0f);
                  glVertex2f(cx + 3.0f * cosf(ang), cy + 3.0f * sinf(ang));
              }
            glEnd();
        }
    }

    glDisable(GL_BLEND);
    leave2D();
}

void drawHUD()
{
    enter2D();

    // Score panel
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0,0,0,0.55f);
    glBegin(GL_QUADS);
      glVertex2f(10,WIN_H-65); glVertex2f(300,WIN_H-65);
      glVertex2f(300,WIN_H-5); glVertex2f(10,WIN_H-5);
    glEnd(); glDisable(GL_BLEND);

    char buf[128];
    if (shootingTeam==TEAM_ARGENTINA) {
        glColor3f(0.45f,0.72f,0.95f);
        drawBitmapString(18,WIN_H-22,GLUT_BITMAP_HELVETICA_18,"ROUND 1: Argentina Shoots");
    } else {
        glColor3f(0.3f,0.9f,0.4f);
        drawBitmapString(18,WIN_H-22,GLUT_BITMAP_HELVETICA_18,"ROUND 2: Brazil Shoots");
    }
    sprintf(buf,"SCORE: %d / %d   |   SHOTS LEFT: %d", score, shotsTaken, shotsLeft);
    glColor3f(1,1,1); drawBitmapString(18,WIN_H-45,GLUT_BITMAP_HELVETICA_12,buf);

    // Mini scoreboard
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0,0,0,0.5f);
    glBegin(GL_QUADS);
      glVertex2f(WIN_W-170,WIN_H-65); glVertex2f(WIN_W-5,WIN_H-65);
      glVertex2f(WIN_W-5,WIN_H-5);   glVertex2f(WIN_W-170,WIN_H-5);
    glEnd(); glDisable(GL_BLEND);
    glColor3f(0.45f,0.72f,0.95f); sprintf(buf,"ARG: %d",argScore);
    drawBitmapString(WIN_W-160,WIN_H-28,GLUT_BITMAP_HELVETICA_18,buf);
    glColor3f(0.3f,0.9f,0.4f); sprintf(buf,"BRA: %d",braScore);
    drawBitmapString(WIN_W-80,WIN_H-28,GLUT_BITMAP_HELVETICA_18,buf);

    // Shooter label
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(shootingTeam==TEAM_ARGENTINA?0.45f:0.0f,
              shootingTeam==TEAM_ARGENTINA?0.72f:0.50f,
              shootingTeam==TEAM_ARGENTINA?0.95f:0.12f, 0.75f);
    glBegin(GL_QUADS);
      glVertex2f(10,35); glVertex2f(275,35); glVertex2f(275,60); glVertex2f(10,60);
    glEnd(); glDisable(GL_BLEND);
    glColor3f(1,1,1);
    if (shootingTeam==TEAM_ARGENTINA)
        drawBitmapString(16,43,GLUT_BITMAP_HELVETICA_12,"P1-ARG Shoot: LEFT/RIGHT=Aim  SPACE=Shoot");
    else
        drawBitmapString(16,43,GLUT_BITMAP_HELVETICA_12,"P2-BRA Shoot: LEFT/RIGHT=Aim  SPACE=Shoot");

    // Keeper label
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(shootingTeam==TEAM_ARGENTINA?0.0f:0.45f,
              shootingTeam==TEAM_ARGENTINA?0.50f:0.72f,
              shootingTeam==TEAM_ARGENTINA?0.12f:0.95f, 0.75f);
    glBegin(GL_QUADS);
      glVertex2f(10,62); glVertex2f(210,62); glVertex2f(210,85); glVertex2f(10,85);
    glEnd(); glDisable(GL_BLEND);
    glColor3f(1,1,1);
    if (shootingTeam==TEAM_ARGENTINA)
        drawBitmapString(16,70,GLUT_BITMAP_HELVETICA_12,"P2-BRA Keep: A=Left  D=Right");
    else
        drawBitmapString(16,70,GLUT_BITMAP_HELVETICA_12,"P1-ARG Keep: A=Left  D=Right");

    // Bottom bar
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0,0,0,0.45f);
    glBegin(GL_QUADS);
      glVertex2f(0,0); glVertex2f(WIN_W,0); glVertex2f(WIN_W,28); glVertex2f(0,28);
    glEnd(); glDisable(GL_BLEND);
    glColor3f(0.8f,0.8f,0.8f);
    drawBitmapString(10,8,GLUT_BITMAP_HELVETICA_12,"R: Restart   ESC: Quit");

    // Celebration banner
    if (celebTimer > 0.0f) {
        float alpha = (celebTimer > 2.0f) ? 1.0f : celebTimer / 1.0f;
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (argCelebrating)
            glColor4f(0.45f, 0.72f, 0.95f, alpha * 0.82f);
        else
            glColor4f(0.98f, 0.82f, 0.00f, alpha * 0.82f);
        glBegin(GL_QUADS);
          glVertex2f(WIN_W*0.25f, WIN_H*0.42f);
          glVertex2f(WIN_W*0.75f, WIN_H*0.42f);
          glVertex2f(WIN_W*0.75f, WIN_H*0.52f);
          glVertex2f(WIN_W*0.25f, WIN_H*0.52f);
        glEnd();
        glDisable(GL_BLEND);
        glColor3f(0.0f, 0.0f, 0.0f);
        const char* celebText = argCelebrating
            ? "GOOOAL! Argentina fans go wild!"
            : "GOOOAL! Brazil fans are celebrating!";
        int len = (int)strlen(celebText);
        drawBitmapString((WIN_W - len*8.5f)*0.5f, WIN_H*0.465f,
                         GLUT_BITMAP_HELVETICA_18, celebText);
    }

    // Feedback
    if (feedbackTimer>0.0f) {
        float alpha = (feedbackTimer>1.5f)?1.0f:feedbackTimer/1.5f;
        if      (lastResult==1) glColor4f(0.2f,1.0f,0.2f,alpha);
        else if (lastResult==0) glColor4f(1.0f,0.3f,0.3f,alpha);
        else                    glColor4f(1.0f,0.85f,0.1f,alpha);
        int len=(int)strlen(feedbackMsg);
        drawBitmapString((WIN_W-len*10)*0.5f, WIN_H*0.55f, GLUT_BITMAP_TIMES_ROMAN_24, feedbackMsg);
    }

    leave2D();
    drawShotIndicators();
}

void drawStars()
{
    if (!starCount) return;
    enter2D();
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(5.0f); glBegin(GL_POINTS);
    for (int i=0;i<starCount;i++)
        if (stars[i].life>0){
            glColor4f(stars[i].r,stars[i].g,stars[i].b,stars[i].life);
            glVertex2f(stars[i].x,stars[i].y);
        }
    glEnd(); glPointSize(1.0f); glDisable(GL_BLEND);
    leave2D();
}

// ─────────────────────────────────────────────────────────
//  Menu screen
// ─────────────────────────────────────────────────────────
void drawMenuScreen()
{
    enter2D();
    glBegin(GL_QUADS);
      glColor3f(0.04f,0.18f,0.04f); glVertex2f(0,0); glVertex2f(WIN_W,0);
      glColor3f(0.02f,0.08f,0.25f); glVertex2f(WIN_W,WIN_H); glVertex2f(0,WIN_H);
    glEnd();

    glColor3f(1.0f,0.85f,0.0f);
    drawBitmapString(WIN_W/2-196,WIN_H/2+200,GLUT_BITMAP_TIMES_ROMAN_24,"   PENALTY  SHOOTOUT  3D");
    glColor3f(0.9f,0.9f,0.9f);
    drawBitmapString(WIN_W/2-155,WIN_H/2+150,GLUT_BITMAP_HELVETICA_18,"           Argentina vs Brazil ");
    glColor3f(1.0f,1.0f,0.3f);
    drawBitmapString(WIN_W/2-185,WIN_H/2+110,GLUT_BITMAP_HELVETICA_12,"Two complete rounds - each team shoots 5 & keeps 5");
    glColor3f(0.45f,0.72f,0.95f);
    drawBitmapString(WIN_W/2-350,WIN_H/2+70,GLUT_BITMAP_HELVETICA_18,"P1 (Argentina): Emiliano Martínez");
    glColor3f(0.3f,0.9f,0.4f);
    drawBitmapString(WIN_W/2+100,WIN_H/2+70,GLUT_BITMAP_HELVETICA_18,"P2 (Brazil): Ederson");
    glColor3f(0.9f,0.9f,0.9f);
    drawBitmapString(WIN_W/2-120,WIN_H/2-110,GLUT_BITMAP_HELVETICA_12," A/D=Move keeper");
    drawBitmapString(WIN_W/2-120,WIN_H/2-130,GLUT_BITMAP_HELVETICA_12," LEFT/RIGHT=Aim, SPACE=Shoot");
    glColor3f(0.5f,1.0f,0.5f);
    drawBitmapString(WIN_W/2-100,WIN_H/2-160,GLUT_BITMAP_HELVETICA_18,"Press ENTER to Play");
     drawGoalkeeperImage();
      drawAnotherImage();

    leave2D();
}

// ─────────────────────────────────────────────────────────
//  Halftime screen
// ─────────────────────────────────────────────────────────
void drawHalftimeScreen()
{
    enter2D();
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0,0,0,0.72f);
    glBegin(GL_QUADS);
      glVertex2f(WIN_W*0.15f,WIN_H*0.2f); glVertex2f(WIN_W*0.85f,WIN_H*0.2f);
      glVertex2f(WIN_W*0.85f,WIN_H*0.82f); glVertex2f(WIN_W*0.15f,WIN_H*0.82f);
    glEnd(); glDisable(GL_BLEND);
    glColor3f(1.0f,0.85f,0.0f);
    drawBitmapString(WIN_W/2-110,WIN_H*0.74f,GLUT_BITMAP_TIMES_ROMAN_24,"HALF TIME!");
    char buf[80];
    glColor3f(0.45f,0.72f,0.95f);
    sprintf(buf,"Argentina scored: %d / %d",argScore,MAX_SHOTS);
    drawBitmapString(WIN_W/2-140,WIN_H*0.62f,GLUT_BITMAP_HELVETICA_18,buf);
    glColor3f(0.9f,0.9f,0.9f);
    drawBitmapString(WIN_W/2-175,WIN_H*0.50f,GLUT_BITMAP_HELVETICA_18,"Now Brazil shoots! Argentina keeps!");
    glColor3f(0.3f,0.9f,0.4f);
    drawBitmapString(WIN_W/2-145,WIN_H*0.38f,GLUT_BITMAP_HELVETICA_18,"P2 (Brazil)  <- LEFT/RIGHT + SPACE");
    glColor3f(0.45f,0.72f,0.95f);
    drawBitmapString(WIN_W/2-145,WIN_H*0.30f,GLUT_BITMAP_HELVETICA_18,"P1 (Argentina)  <- A / D  keeper");
    glColor3f(0.8f,0.8f,0.8f);
    drawBitmapString(WIN_W/2-125,WIN_H*0.24f,GLUT_BITMAP_HELVETICA_12,"Press ENTER to start Round 2");
    leave2D();
}

// ─────────────────────────────────────────────────────────
//  Result screen
// ─────────────────────────────────────────────────────────
void drawResultScreen()
{
    enter2D();
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0,0,0,0.70f);
    glBegin(GL_QUADS);
      glVertex2f(WIN_W*0.12f,WIN_H*0.15f); glVertex2f(WIN_W*0.88f,WIN_H*0.15f);
      glVertex2f(WIN_W*0.88f,WIN_H*0.88f); glVertex2f(WIN_W*0.12f,WIN_H*0.88f);
    glEnd(); glDisable(GL_BLEND);
    glColor3f(1.0f,0.85f,0.0f);
    drawBitmapString(WIN_W/2-90,WIN_H*0.80f,GLUT_BITMAP_TIMES_ROMAN_24,"FULL TIME!");
    char buf[80];
    glColor3f(0.45f,0.72f,0.95f); sprintf(buf,"Argentina: %d goals",argScore);
    drawBitmapString(WIN_W/2-130,WIN_H*0.68f,GLUT_BITMAP_HELVETICA_18,buf);
    glColor3f(0.3f,0.9f,0.4f); sprintf(buf,"Brazil:    %d goals",braScore);
    drawBitmapString(WIN_W/2-130,WIN_H*0.58f,GLUT_BITMAP_HELVETICA_18,buf);
    glColor3f(1.0f,0.95f,0.0f);
    if (argScore>braScore) drawBitmapString(WIN_W/2-165,WIN_H*0.46f,GLUT_BITMAP_TIMES_ROMAN_24,"ARGENTINA WINS! Vamos!");
    else if (braScore>argScore) drawBitmapString(WIN_W/2-145,WIN_H*0.46f,GLUT_BITMAP_TIMES_ROMAN_24,"BRAZIL WINS! Vai Brasil!");
    else {
        drawBitmapString(WIN_W/2-110,WIN_H*0.46f,GLUT_BITMAP_TIMES_ROMAN_24,"IT'S A DRAW!");
        glColor3f(0.9f,0.9f,0.9f);
        drawBitmapString(WIN_W/2-120,WIN_H*0.38f,GLUT_BITMAP_HELVETICA_12,"(Sudden death would follow in a real match!)");
    }
    int total=argScore+braScore;
    const char* rating=
        total>=9?"What a shootout! Both keepers outclassed!":
        total>=7?"Incredible tension! Great match!":
        total>=5?"Solid shooting from both sides!":
                  "Keepers dominated today!";
    glColor3f(0.75f,0.75f,0.75f);
    drawBitmapString(WIN_W/2-155,WIN_H*0.30f,GLUT_BITMAP_HELVETICA_12,rating);
    glColor3f(0.8f,0.8f,0.8f);
    drawBitmapString(WIN_W/2-130,WIN_H*0.22f,GLUT_BITMAP_HELVETICA_12,"R = Play Again   |   ESC = Quit");
    leave2D();
}

// ─────────────────────────────────────────────────────────
//  Display
// ─────────────────────────────────────────────────────────
void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    if (gameState==STATE_MENU) { drawMenuScreen(); glutSwapBuffers(); return; }

    if (gameState==STATE_HALFTIME) {
        gluLookAt(0,6,26,0,2.5,0,0,1,0);
        setupLighting();
        drawSky();
        drawCrowd();
        drawPitch();
        drawGoalPost();
        drawGoalkeeper();
        drawPenaltyTaker();   // ← ADD THIS
        drawBall();
        drawCornerFlags();
        drawHalftimeScreen();
        glutSwapBuffers(); return;
    }

    gluLookAt(0,6,26,0,2.5,0,0,1,0);
    setupLighting();
    drawSky();
    drawCrowd();
    drawPitch();
drawGoalPost();
drawGoalkeeper();
drawPenaltyTaker();   // ← ADD THIS
drawCornerFlags();
drawBall();
    //drawAimCrosshair();
    drawHUD();
    drawStars();

    if (gameState==STATE_RESULT) drawResultScreen();

    glutSwapBuffers();
}

// ─────────────────────────────────────────────────────────
//  Shoot
// ─────────────────────────────────────────────────────────
void shoot()
{
    if (isShooting || shotsLeft<=0) return;
    isShooting=true; shotsLeft--; shotsTaken++;
    float distZ=ballZ-GOAL_Z, distX=aimX-ballX, distY=aimY-ballY;
    float steps=distZ/0.5f;
    ballDZ=-0.5f; ballDX=distX/steps; ballDY=distY/steps;
    triggerKeeperDive();
    strcpy(feedbackMsg,""); feedbackTimer=0.0f; lastResult=-1;
    penaltyAnimTimer  = 0.0f;   // ← ADD
    penaltyAnimActive = true;   // ← ADD
}

// ─────────────────────────────────────────────────────────
//  Update
// ─────────────────────────────────────────────────────────
void update(int val)
{
    if (gameState==STATE_PLAYING) {
        updateClouds(); updateBirds();
        flagWave += 0.09f;

        // Advance penalty taker kick animation
        if (penaltyAnimActive) {
            penaltyAnimTimer += 0.020f;           // full cycle ≈ 1.5 s
            if (penaltyAnimTimer >= 1.5f) {
                penaltyAnimActive = false;
                penaltyAnimTimer  = 0.0f;
            }
        }

        // Countdown celebration timer
        if (celebTimer > 0.0f) {
            celebTimer -= 0.016f;
            if (celebTimer <= 0.0f) {
                celebTimer = 0.0f;
                argCelebrating = false;
                braCelebrating = false;
            }
        }

        if (!isShooting) { idleTimer+=0.05f; ballY=0.55f+0.06f*sinf(idleTimer); }

        if (!isShooting && !keeperDiving) {
            if (p2MoveLeft)  keeperX -= p2ManualSpeed;
            if (p2MoveRight) keeperX += p2ManualSpeed;
            if (keeperX >  4.2f) keeperX =  4.2f;
            if (keeperX < -4.2f) keeperX = -4.2f;
        }

        if (keeperDiving) {
            keeperDiveTimer += 0.016f;
            if (keeperDiveTimer>0.6f){ keeperDiving=false; keeperX=keeperDiveX; keeperY=1.5f; }
        }

        if (isShooting) {
            ballZ+=ballDZ; ballX+=ballDX; ballY+=ballDY;
            ballRotX+=8.0f; ballRotZ+=3.0f;

            if (ballZ<=GOAL_Z+0.5f && ballZ>=GOAL_Z-0.5f) {
                bool inX=(ballX>GOAL_LEFT+0.2f && ballX<GOAL_RIGHT-0.2f);
                bool inY=(ballY>0.0f && ballY<GOAL_TOP-0.2f);

                if (!inX||!inY) {
                    strcpy(feedbackMsg,"OUT! Off target!"); lastResult=-1; feedbackTimer=1.0f;
                    if (shotsTaken-1 >= 0 && shotsTaken-1 < MAX_SHOTS)
                        shotResults[shotsTaken-1] = -1;
                    resetBall();
                } else {
                    float t=keeperDiveTimer/0.5f; if(t>1)t=1;
                    float kx=keeperX+(keeperDiveX-keeperX)*t;
                    float ky=1.5f+(keeperDiveY-1.5f)*t;
                    float dist=sqrtf((ballX-kx)*(ballX-kx)+(ballY-ky)*(ballY-ky));

                    if (dist<1.1f) {
                        const char* saveMsgs[2]={"SAVED!  Keeper wins this one!","SAVED!  What a stop!"};
                        strcpy(feedbackMsg,saveMsgs[rand()%2]);
                        lastResult=0; feedbackTimer=1.0f;
                        if (shotsTaken-1 >= 0 && shotsTaken-1 < MAX_SHOTS)
                            shotResults[shotsTaken-1] = -1;
                        resetBall();
                        // Keeper's side fans celebrate the save
                        if (shootingTeam==TEAM_ARGENTINA)
                            triggerCelebration(TEAM_BRAZIL);
                        else
                            triggerCelebration(TEAM_ARGENTINA);
                    } else {
                        score++;
                        if (shootingTeam==TEAM_ARGENTINA) argScore=score;
                        else braScore=score;
                        const char* goalMsg=(shootingTeam==TEAM_ARGENTINA)?"GOAL!!! Argentina scores!":"GOAL!!! Brazil scores!";
                        strcpy(feedbackMsg,goalMsg);
                        lastResult=1; feedbackTimer=1.0f;
                        if (shotsTaken-1 >= 0 && shotsTaken-1 < MAX_SHOTS)
                            shotResults[shotsTaken-1] = 1;
                        spawnStars();
                        triggerCelebration(shootingTeam);   // scoring team fans celebrate
                        resetBall();
                    }
                }

                if (shotsLeft==0) {
                    if (shootingTeam==TEAM_ARGENTINA) {
                        argScore=score;
                        glutTimerFunc(2200,[](int){ gameState=STATE_HALFTIME; },0);
                    } else {
                        braScore=score;
                        glutTimerFunc(2200,[](int){ gameState=STATE_RESULT; },0);
                    }
                }
            }

            if (ballZ<GOAL_Z-3.0f) {
                resetBall();
                if (shotsLeft==0){
                    if (shootingTeam==TEAM_ARGENTINA){ argScore=score; gameState=STATE_HALFTIME; }
                    else { braScore=score; gameState=STATE_RESULT; }
                }
            }
        }

        if (feedbackTimer>0) feedbackTimer-=0.016f;
        for (int i=0;i<starCount;i++) {
            if (stars[i].life>0){
                stars[i].x+=stars[i].vx; stars[i].y+=stars[i].vy;
                stars[i].vy-=0.08f; stars[i].life-=0.012f;
                if (stars[i].life<0) stars[i].life=0;
            }
        }
    } else if (gameState==STATE_MENU || gameState==STATE_HALFTIME) {
        updateClouds(); updateBirds();
        flagWave += 0.09f;
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

// ─────────────────────────────────────────────────────────
//  Keyboard
// ─────────────────────────────────────────────────────────
void keyboard(unsigned char key, int x, int y)
{
    switch(key) {
    case 27: exit(0); break;
    case ' ':
        if (gameState==STATE_PLAYING) shoot();
        break;
    case 13:
        if (gameState==STATE_MENU)     { initClouds(); initBirds(); initCelebPhases(); startRound(TEAM_ARGENTINA); }
        else if (gameState==STATE_HALFTIME) { initCelebPhases(); startRound(TEAM_BRAZIL); }
        break;
    case 'r': case 'R':
        if (gameState==STATE_RESULT||gameState==STATE_PLAYING||gameState==STATE_HALFTIME){
            argScore=0; braScore=0;
            initClouds(); initBirds(); initCelebPhases(); startRound(TEAM_ARGENTINA);
        }
        break;
    case 'a': case 'A': p2MoveLeft  = true;  break;
    case 'd': case 'D': p2MoveRight = true;  break;
    }
}

void keyboardUp(unsigned char key, int x, int y)
{
    switch(key) {
    case 'a': case 'A': p2MoveLeft  = false; break;
    case 'd': case 'D': p2MoveRight = false; break;
    }
}

void specialKeys(int key, int x, int y)
{
    if (gameState!=STATE_PLAYING || isShooting) return;
    float stepH = 0.32f;
    switch(key) {
    case GLUT_KEY_LEFT:  aimX -= stepH; break;
    case GLUT_KEY_RIGHT: aimX += stepH; break;
    }
    if (aimX < GOAL_LEFT  + 0.5f) aimX = GOAL_LEFT  + 0.5f;
    if (aimX > GOAL_RIGHT - 0.5f) aimX = GOAL_RIGHT - 0.5f;
}

void reshape(int w, int h)
{
    if (!h) h=1;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(45.0,(double)w/h,0.5,150.0);
    glMatrixMode(GL_MODELVIEW);
}

void init()
{
    srand((unsigned)time(NULL));
    glClearColor(0.35f,0.62f,0.95f,1.0f);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST);
    glEnable(GL_NORMALIZE);
    initClouds();
    initBirds();
    initCelebPhases();
     glEnable(GL_TEXTURE_2D);  // ← ADD THIS in init()
    loadGoalkeeperTexture("goalkeeper_argentina.png");
    loadGoalkeeperTexture2("goalkeeper_brazil.png");
}

int main(int argc, char** argv)
{
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH);
    glutInitWindowSize(WIN_W,WIN_H);
    glutInitWindowPosition(100,80);
    glutCreateWindow("Penalty Blitz 3D  |  Argentina vs Brazil  |  Full Shootout");
    init();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialKeys);
    glutTimerFunc(16,update,0);
    glutMainLoop();
    return 0;
}
