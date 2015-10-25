
#define WITHSOUND 0
#define FRANKTFT 0
#define USERTFT 1


#include <SPI.h>

#if WITHSOUND
#include <SD.h>
#include <Wire.h>
#include <Audio.h>
#include <play_sd_aac.h>
#endif

#include <ILI9341_t3.h>
#include <XPT2046_Touchscreen.h>

#include "blocks.h"
#include "font_BlackOpsOne-Regular.h"
#include "font_DroidSans.h"

#include <SerialFlash.h>
#include "tetris_mp3.h" //should be tetris_aac.h :-)

#if FRANKTFT
#define TFT_DC      20
#define TFT_CS      21
#define TFT_RST    255  // 255 = unused, connect to 3.3V
#define TFT_MOSI     7
#define TFT_SCLK    14
#define TFT_MISO    12
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_MISO);
#endif
#if USERTFT
//Adjust these !!!:
#define TFT_DC  9
#define TFT_CS 10
#define TFT_RST    255  // 255 = unused, connect to 3.3V
#define TFT_MOSI    11
#define TFT_SCLK    13
#define TFT_MISO    12
// MOSI=11, MISO=12, SCK=13
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC);
#endif

#define TOUCH_CS  8
XPT2046_Touchscreen ts(TOUCH_CS);

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 100
#define TS_MINY 100
#define TS_MAXX 3800
#define TS_MAXY 4000

//Adjust sound-output !!!
#if WITHSOUND
AudioPlaySdAac      playSnd;       //xy=154,78
AudioOutputPWM      sndOut;           //xy=334,89
AudioConnection     patchCord1(playSnd, 0, sndOut, 0);
AudioConnection     patchCord2(playSnd, 1, sndOut, 1);
#endif

#define SCREEN_BG   ILI9341_NAVY
#define SPEED_START 300
#define SPEED_MAX   200

uint16_t color_gamma[3][NUMCOLORS];
uint8_t  field[FIELD_WIDTH][FIELD_HIGHT];
uint16_t aSpeed, score, highscore;
int8_t   aBlock, aColor, aX, aY, aRotation;

void setup() {
  AudioMemory(10);

  //If using Teensy-Audioshield, insert lines here for setup!

  //color[0] is background, no gamma
  for (unsigned i=1; i < NUMCOLORS; i++) {
    color_gamma[0][i] = gamma(color[i], 30);
    color_gamma[1][i] = gamma(color[i], -70);
    color_gamma[2][i] = gamma(color[i], -35);
  }

  int t = millis();
  while (!Serial || t-millis()<1000) {;}
  
  Serial.println("--T3TRIS--");
  tft.begin();
  ts.begin();
  tft.setRotation(2);

  highscore = 0;

  tft.fillScreen(SCREEN_BG);
  tft.setCursor(10, 10);
  tft.setFont(BlackOpsOne_40);
  printColorText("T3TRIS",1);

  tft.setFont(DroidSans_10);
  tft.setCursor(210,5);
  tft.setTextColor(ILI9341_YELLOW);
  tft.print("F.B.");

  tft.setCursor(0,100);
  tft.setFont(DroidSans_10);
  tft.setTextColor(ILI9341_GREEN);
  tft.print("Score");

  tft.setCursor(0,200);
  tft.setFont(DroidSans_10);
  tft.setTextColor(ILI9341_YELLOW);
  tft.print("Hi-");
  tft.setCursor(0,211);
  tft.print("Score");

  initGame();
}

void initGame(){
  score = 0;
  aSpeed = SPEED_START;
  initField();
}

void printColorText(const char * txt, unsigned colorOffset) {
  unsigned col=colorOffset;
  while (*txt) {
   tft.setTextColor(color[col]);
   tft.print(*txt);
   if (++col > NUMCOLORS-1) col = 1;
   txt++;
  }
}

void initField() {
  memset(field, 0, sizeof(field));
  tft.fillRect( FIELD_X, FIELD_Y, FIELD_WIDTH*PIX, FIELD_HIGHT*PIX,color[0]);
}

void printNum(unsigned num) {
  if (num<10000) tft.print("0");
  if (num<1000) tft.print("0");
  if (num<100) tft.print("0");
  if (num<10) tft.print("0");
  tft.print(num);
}

void printScore() {  
  tft.setFont(DroidSans_10);
  tft.setTextColor(ILI9341_GREEN);
  tft.setCursor(3,116);
#if WITHSOUND
  AudioNoInterrupts();
#endif  
  tft.fillRect( 3, 116, FIELD_X -3, 10, SCREEN_BG);
  printNum(score);
#if WITHSOUND  
  AudioInterrupts();
#endif  
}

void printHighScore() {
  tft.setFont(DroidSans_10);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(3,228);  
  tft.fillRect( 3, 228, FIELD_X -3, 10, SCREEN_BG);
  printNum(highscore);  
}


void printGameOver() {
  tft.setFont(BlackOpsOne_40);
  tft.fillRect( FIELD_X , 120, FIELD_XW, 40, SCREEN_BG);
  tft.fillRect( FIELD_X , 170, FIELD_XW, 40, SCREEN_BG);

  int t = millis();
  unsigned cofs = 1;
  do {
    tft.setCursor( FIELD_X+10, 120);
    printColorText("GAME",cofs);
    tft.setCursor( FIELD_X+20, 170);
    printColorText("OVER",cofs);
    if (++cofs > NUMCOLORS-1) cofs = 1;
   delay(30);
  } while (millis()-t < 2000);
}

void playSound(bool onoff) {
#if WITHSOUND  
  if (!playSnd.isPlaying() && onoff) playSnd.play(tetris_aac,tetris_aac_len);
  else
  if (playSnd.isPlaying() && !onoff) playSnd.stop();
#endif  
}

void loop(void) {
bool r = false;
int c = 0;
int c1 = 0;
 
 playSound(true);

 while(!r) {
  if (++c==8) {
      effect1();
      c = 0;
  }
  if (++c1==50) {
    playSound(true);
    c1 = 0;
  }
  r = game(true);
 }
 game(false);
}


bool game(bool demoMode) {
  bool gameOver = false;
  uint8_t oldaX, oldaY,oldaRotation;
  int tk = 0;

  initGame();
  nextBlock();
  drawBlock(aBlock, aX, aY, aRotation, aColor);

  do {
    yield();
    if (!demoMode) playSound(true);

    oldaX = aX;
    oldaY = aY;
    oldaRotation = aRotation;

    int t = millis();

    if (!demoMode) do {  // process controls
      if (millis() - tk > aSpeed/3) {
        char ch = controls();
        if (ch != '\0') tk = millis();
        switch (ch) {
          case 's' : //down
            tk = millis()-100;
            continue;
          case '+' : { //rotate
            int tmp = aRotation +1;
            if (tmp > 3) tmp = 0;
            if (checkMoveBlock(0,0,tmp)) {
                oldaRotation = aRotation;
                aRotation = tmp;
                drawBlockEx(aBlock, aX, aY, aRotation, aColor, aX, aY, oldaRotation);
                oldaRotation = aRotation;
             }
             break;
             }
           case 'a' : //right
           case 'd' : //left
              int dX = (ch=='d') ? 1 : -1;
              if (checkMoveBlock(dX,0,0)) {
                  oldaX = aX;
                  aX += dX;
                  drawBlockEx(aBlock, aX, aY, aRotation, aColor, oldaX, aY, aRotation);
                  oldaX = aX;
              }
              break;
        }
      }
      yield();
    } while ( millis() - t < aSpeed);   // process controls end

    else { //demoMode
      delay(5);
      char ch = controls();
      if (ch != '\0')  return true;
    }

    //move the block down
    bool movePossible = checkMoveBlock(0,1,0);
    if ( movePossible ){
        score += 1;
        aY++ ;
        drawBlockEx(aBlock, aX, aY, aRotation, aColor, oldaX, oldaY, oldaRotation);
    }

    else {
      //block stopped moving down
      //store location
      setBlock();
      checkLines();
      //get new block and draw it
      score += 10;
      nextBlock();
      drawBlock(aBlock, aX, aY, aRotation, aColor);
      //immedately check if it can move down
      if (!checkMoveBlock(0,0,0)) {
        //no, game over !
        initField();
        gameOver = true;
      }
    }

    printScore();
  } while(!gameOver);

  printScore();
  if (score > highscore) {
      highscore = score;
      printHighScore();
  }
  if (!demoMode) {
    Serial.println();        
    Serial.print("Score: ");
    Serial.println(score);        
    playSound(false);
    printGameOver();    
  }
  return false;
}

char controls() {
  if (Serial.available()) {
    return (Serial.read());
  }
  if ( ts.bufferEmpty() ) return ('\0');

  TS_Point p = ts.getPoint();
  if (p.z < 400) return ('\0');

  p.y = TS_MAXY - p.y;
  p.x = TS_MAXX - p.x;
  p.x = map(p.x, TS_MINX, TS_MAXX, 0, 3);
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, 3);

  if ((p.y < 1)  && (p.x > 1)) return ('d');
  if ((p.y < 1)  && (p.x < 1)) return ('a');
  if ((p.y >= 2) && (p.x < 1)) return ('s');
  if ((p.y >= 2) && (p.x > 1)) return ('+');

#if 0
  tft.setFont(DroidSans_10);
  tft.setTextColor(ILI9341_GREEN);
  tft.fillRect( 0, 0, 40, 40,color[0]);
  tft.setCursor(3,3);
  tft.print(p.x);
  tft.setCursor(3,16);
  tft.print(p.y);
#endif

  return ('\0' );
}

void setBlock() {
  int bH = BLOCKHIGHT(aBlock, aRotation);
  int bW = BLOCKWIDTH(aBlock, aRotation);

  for (int y=0; y<bH; y++) {
    for (int x=0; x<bW; x++) {
      if ( (block[aRotation][aBlock][y * 4 + x + 2] > 0) ) {
        field[x + aX][y + aY] = aColor;
      }
    }
  }

}

void checkLines() {
  //evtl mit ay beginnen ?
  int x,y,c,i;
  for (y=0; y<FIELD_HIGHT; y++) {
    c = 0;
    for (x=0; x<FIELD_WIDTH; x++) {
      if (field[x][y] > 0) c++;
    }

    if ( c >= FIELD_WIDTH ) {//complete line ! //FIELD_WIDTH

      for (i = NUMCOLORS-1; i >= 0; i--) {
        for (x =0; x < FIELD_WIDTH; x++) {
          drawBlockPix(FIELD_X + x*PIX,FIELD_Y + y*PIX,i);
        }
        delay(60);
      }

      //move entire field above complete line down and redraw
      for (i = y; i>0; i--)
        for (x=0; x<FIELD_WIDTH; x++)
          field[x][i]=field[x][i-1];
      for (x=0; x<FIELD_WIDTH; x++)
          field[x][0]=0;

      drawField();
      if (aSpeed>0) aSpeed -= 5;
    }
  }
}

bool checkMoveBlock(int deltaX, int deltaY, int deltaRotation) {

  int rot = aRotation + deltaRotation;
  int bH = BLOCKHIGHT(aBlock, rot);
  if (deltaY)
    if (aY + bH + 1 > FIELD_HIGHT)  //lower border
      return false;

  int bW = BLOCKWIDTH(aBlock, rot);  //left border
  if (deltaX > 0) {
    if (aX + bW + 1 > FIELD_WIDTH)
      return false;
  }
  else if (deltaX < 0) {   //right border
    if (aX - 1 < 0)
      return false;
  }

  int dX = aX + deltaX;
  int dY = aY + deltaY;

  for (int y=bH-1; y>= 0; y--) {
    for (int x=0; x<bW; x++) {
      if ( (field[x + dX][y + dY] > 0) && (block[rot][aBlock][y * 4 + x + 2] > 0) ) {
        return false;
      }
    }
  }

  return true;
}

void nextBlock() {
  aColor = random(1,NUMCOLORS);
  aBlock = random(NUMBLOCKS);
  aRotation = random(4);
  aY = 0;
  aX = random(FIELD_WIDTH - BLOCKWIDTH(aBlock, aRotation) + 1);
}

void effect1() {
  int t = millis();
  do {
    nextBlock();
    drawBlock(aBlock,aX,random(FIELD_HIGHT),aRotation,aColor);
  } while (millis() - t < 1000);
}

uint16_t gamma(int16_t color, int16_t gamma){
 return  tft.color565(
    constrain(((color>>8) & 0xf8) + gamma,0,255),  //r
    constrain(((color>>3) & 0xfc) + gamma,0,255),  //g
    constrain(((color<<3) & 0xf8) + gamma,0,255)); //b
}

void drawBlockPix(int px, int py, int col) {

    if (px>=FIELD_XW) return;
    if (px<FIELD_X) return;
    if (py>=FIELD_YW) return;
    if (py<FIELD_Y) return;

    if (col == 0) {
      //remove Pix, draw backgroundcolor
      tft.fillRect(px, py, PIX, PIX, color[col]);
      return;
    }

    const int w=4;

    tft.fillRect(px+w, py+w, PIX-w*2+1, PIX-w*2+1, color[col]);
    for (int i = 0; i<w;i++) {
     tft.drawFastHLine(px + i, py + i, PIX-2*i , color_gamma[0][col]);
     tft.drawFastHLine(px + i, PIX + py - i - 1 , PIX-2*i , color_gamma[1][col]);
     tft.drawFastVLine(px + i, py + i , PIX-2*i , color_gamma[2][col]);
     tft.drawFastVLine(px + PIX - i - 1, py + i , PIX-2*i , color_gamma[2][col]);
    }

 }

void drawBlock(int blocknum, int px, int py, int rotation, int col) {
    int w = BLOCKWIDTH(blocknum, rotation);
    int h = BLOCKHIGHT(blocknum, rotation);

    for (int x=0; x<w; x++) {
       for (int y=0; y<h; y++) {
         if (block[rotation][blocknum][y*4 + x + 2])
            drawBlockPix(FIELD_X+px*PIX+x*PIX, FIELD_Y+py*PIX+y*PIX, col);
         }
     }
 }

 static uint8_t dbuf[FIELD_WIDTH][FIELD_HIGHT] ={0};
 void drawBlockEx(int blocknum, int px, int py, int rotation, int col, int oldx, int oldy, int oldrotation) {

    int x,y;
    int w = BLOCKWIDTH(blocknum, oldrotation);
    int h = BLOCKHIGHT(blocknum, oldrotation);

    for (x=0; x<w; x++)
       for (y=0; y<h; y++)
         if (block[oldrotation][blocknum][y*4 + x + 2]>0) dbuf[x + oldx][y + oldy] = 2;

    w = BLOCKWIDTH(blocknum, rotation);
    h = BLOCKHIGHT(blocknum, rotation);
    for (x=0; x<w; x++)
       for (y=0; y<h; y++)
         if (block[rotation][blocknum][y*4 + x + 2]>0) dbuf[x + px][y + py] = 1;
    
#if WITHSOUND
    AudioNoInterrupts();
#endif
    for (y=FIELD_HIGHT-1; y>=oldy; y--)
       for (x=0; x<FIELD_WIDTH; x++)
         switch(dbuf[x][y]) {
           case 1:  drawBlockPix(FIELD_X+x*PIX, FIELD_Y+y*PIX, col); dbuf[x][y]=0;break;
           case 2:  tft.fillRect(FIELD_X+x*PIX, FIELD_Y+y*PIX, PIX, PIX, color[0]); dbuf[x][y]=0; break;
        }
#if WITHSOUND        
    AudioInterrupts();
#endif    
 }

void drawField() {
  int x,y;
  for (y=FIELD_HIGHT-1; y>=0; y--)
    for (x=0; x<FIELD_WIDTH; x++)
       drawBlockPix(FIELD_X+x*PIX, FIELD_Y+y*PIX, field[x][y]);
}

