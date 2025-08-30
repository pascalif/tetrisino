/*
   One player Tetris game for Arduino Nano

   Credits, tech details, todo list : cf README.md file

   2025 edition


   COMPILATION
   See include's below to install proper stuff

   UPLOADING
   Tools > Board : Arduino AVR Boards > Arduino Nano
   Baud rate 115200
   Tools > Processor : ATMEGA328P (Old bootloader)

BUGS
- power switch

TODOs
- scroller le '...' en mode screensaver
*/


// ===========================================================================================================
// LIBRARIES
// ===========================================================================================================

#include <SPI.h>

// For timers (built-in library)
// https://github.com/thomasfredericks/Metro-Arduino-Wiring
// https://github.com/thomasfredericks/Metro-Arduino-Wiring/archive/master.zip
// To install it, copy "Metro" folder in /home/pascal/Arduino/libraries/ folder
#include <Metro.h>

// Base library for Max72xxPanel library
// To install it, include "Adafruit GFX library" in Libraries manager
#include <Adafruit_GFX.h>

// For the 8 7-segments-digits
// To install it, include "LedControl" in Libraries manager - 2025-08 : v1.0.6
// Doc: https://wayoda.github.io/LedControl/
#include <LedControl.h>

// For the 6 8x8 matrix of matrices
// Download it from https://github.com/markruys/arduino-Max72xxPanel
#include <Max72xxPanel.h>

// To debounce buttons
// To install it, include "ADebouncer" in Libraries manager - 2025-08 : v1.1.0
#include "ADebouncer.h"


// ===========================================================================================================
// CUSTOMIZABLE BEHAVIOUR
// ===========================================================================================================

// ----------------------------------------------------------
// Flags
// ----------------------------------------------------------

// Set to true/false to enable/disable entering keys with Arduino's wired inputs (which is the final goal)
#define FLAG_READ_REAL_INPUTS true

// Uncomment this switch to enable music playing after Konami code (UUDDLRLR) has been entered
#define FLAG_ENABLE_MUSIC

// Uncomment to enable entering keys on keyboard
// For prod version, comment this switch to save Flash memory
//#define FLAG_DEV_READ_SERIAL_INPUTS

// Uncomment to log on serial monitor various events and user actions
// For prod version, comment this switch to save Flash memory
//#define FLAG_DEV_ENABLE_LOGS

#define SERIAL_BAUD_SPEED 115200


// ----------------------------------------------------------
// Rules
// ----------------------------------------------------------

// Level-up after this number of lines have been completed
#define LINES_PER_LEVEL 10

// Score increment per line after a block is pushed down by the user with the 'down' action
#define SCORE_PER_DOWN_PER_LINE 1

// Score increment per line after a block is dropped by the user with the 'drop' action
#define SCORE_PER_DROP_PER_LINE 2

// Base score per line completed
// In case of multiple lines, 1st will increase score with this value, 2nd will increase with this value x 2, and so on...
// So the total score increment in case of multiple lines will be :
//   1 line  :  1 x factor
//   2 lines :  3 x factor
//   3 lines :  6 x factor
//   4 lines : 10 x factor
#define SCORE_PER_COMPLETED_LINE_FACTOR 20


// ----------------------------------------------------------
// Animations & g_display
// ----------------------------------------------------------

// Time we stay on the preparation screen before starting one the screensavers
#define SCREENSAVER_DELAY_BEFORE 20000

// Duration of a screensaver before switching to another
#define SCREENSAVER_DURATION 20000

// Screensaver 'Fly' : animation speed
#define SCREENSAVER_FLY_SPEED 30

// Screensaver 'HighScores' : animation speed
#define SCREENSAVER_HIGHSCORES_SPEED 70


// Duration of a fully lit screen when leveling-up
#define LEVELUP_FLASH_DELAY 100

// Time interval between two malus lines (msec)
#define MODE_MALUS_LINES_INTERVAL 20000

// For debug purpose on Serial (if > 0, limit qty of communications with matrix and time spent in interrupts)
// Should be 0 on prod for maximal frames/seconds rate
#define MATRIX_REFRESH_RATE_REDUCE_FACTOR 0

// Display intensity of scoring digits (max : 15)
#define DISP_SCORE_INTENSITY 1

// Display intensity of leds matrix (max : 255)
#define MATRIX_INTENSITY 2

// Debounce delays
#define ROTATE_BUTTON_DEBOUNCE 30
#define START_BUTTON_DEBOUNCE 50
#define MODE_BUTTON_DEBOUNCE 100

// Delays before autorepeat
#define SIDE_BUTTON_AUTOREPEAT_DELAY 110  // 120 is ok. Less than that, it's hard to move one column by one column
#define DOWN_BUTTON_AUTOREPEAT_DELAY 60   // faster than lateral movement, nice to see fast down
#define DROP_BUTTON_AUTOREPEAT_DELAY 250


// Parameter to compute delays between auto down moves for levels before and after the level DOWN_SPEED_LEVEL_LIMIT_1
#define DOWN_SPEED_BASE_1 500
#define DOWN_SPEED_INCR_1 45
#define DOWN_SPEED_BASE_2 140
#define DOWN_SPEED_INCR_2 10
#define DOWN_SPEED_LEVEL_LIMIT_1 9
#define DOWN_SPEED_LEVEL_LIMIT_2 14
// Algo :
// level <= DOWN_SPEED_LEVEL_LIMIT_1 :  speed = DOWN_SPEED_BASE_1 - (level - 1) * DOWN_SPEED_INCR_1                          : [ 1:500 ; 9:140 ]
//        >                                   = DOWN_SPEED_BASE_2 - (level - DOWN_SPEED_LEVEL_LIMIT_1) * DOWN_SPEED_INCR_2     [ 10:130, 11:120, 12:110, 13:100, 14:90]
//        > DOWN_SPEED_LEVEL_LIMIT_2 :  speed = 90


// If instabilities at boot time ?
#define SETUP_INITIAL_PAUSE 0

// Some pause after then gameover curtains
#define GAME_OVER_POST_ANIMATION_PAUSE 1000


// ----------------------------------------------------------
// Pins
// ----------------------------------------------------------

#define INPUT_PIN_JOY_DROP 5
#define INPUT_PIN_JOY_LEFT 4
#define INPUT_PIN_JOY_RIGHT 3
#define INPUT_PIN_JOY_DOWN 2

#define INPUT_PIN_BTN_START A0
#define INPUT_PIN_BTN_ROTATE_RIGHT A1
#define INPUT_PIN_BTN_ROTATE_LEFT A2
#define INPUT_PIN_BTN_MODE_NEXT_PIECE A4
#define INPUT_PIN_BTN_MODE_MALUS_LINES A3

#define OUTPUT_PIN_PWM_PIEZO 6  // Must be a PWM. On Nano : 3,5,6,9,10 or 11

#define OUTPUT_PIN_BACKLIGHT A5


// DIN to MOSI               (11)
#define MAX7219_MATRIX_PIN_CS 10
// CLK to SCK                (13)
// (cf http://arduino.cc/en/Reference/SPI ) Other autres pin configurables ?

#define MAX7219_SCORE_ID 0  // Id of the score display
#define MAX7219_SCORE_PIN_DIN 8
#define MAX7219_SCORE_PIN_CLK 9
#define MAX7219_SCORE_PIN_CS 7

#define PLAYABLE_ROWS 24
#define PLAYABLE_COLS 10
#define OFFSET_ROW 0  // In a 3x2 matrix configuration, we use all vertical space


// ===========================================================================================================
// CONSTANTS
// ===========================================================================================================

// Position of each matrix on the matrix grid
#define MATRIX_COL_LEFT 0
#define MATRIX_COL_RIGHT 1
#define MATRIX_LINE_TOP 0
#define MATRIX_LINE_MIDDLE 1
#define MATRIX_LINE_BOTTOM 2

// From g_matrix.setRotation documentation:
#define MATRIX_ROTATION_NONE 0
#define MATRIX_ROTATION_90_CW 1
#define MATRIX_ROTATION_180 2
#define MATRIX_ROTATION_90_CCW 3


// User actions
// ------------
#define ACTION_NONE 0
#define ACTION_BTN_START 1
#define ACTION_JOY_RIGHT 2
#define ACTION_JOY_LEFT 3
#define ACTION_JOY_DOWN 4
#define ACTION_JOY_DROP 5
#define ACTION_BTN_ROTATE_RIGHT 6
#define ACTION_BTN_ROTATE_LEFT 7


// Workflow
// --------
//                   ________________  start ________________
//                   |                                      |
//                   v           start                      |
// BOOTING ===> GAME_WAIT_START =====> GAME_RUNNING => GAME_OVER
// 'Helllo'        'Start'
//                |     ʌ
//                |     | start
//                v     |
//              SCREENSAVER
//                 '...'
//
#define STATE_BOOTING 0
#define STATE_GAME_WAIT_START 1
#define STATE_GAME_RUNNING 2
#define STATE_GAME_OVER 3
#define STATE_SCREENSAVER 99


// Blocks management
// -----------------
#define NB_BLOCK_TYPES 7
#define BLOCK_TYPE_NONE 0xFF
#define BLOCK_TYPE_I 0
#define BLOCK_TYPE_J 1
#define BLOCK_TYPE_L 2
#define BLOCK_TYPE_O 3
#define BLOCK_TYPE_S 4
#define BLOCK_TYPE_T 5
#define BLOCK_TYPE_Z 6

#define ONE_LINE_DOWN_SUCCESSFULL true
#define ONE_LINE_DOWN_BLOCKED false

#define BLOCK_TYPE_I_POS_V 0
#define BLOCK_TYPE_I_POS_H 1

#define BLOCK_TYPE_T_POS_UP 0
#define BLOCK_TYPE_T_POS_RIGHT 1
#define BLOCK_TYPE_T_POS_DOWN 2
#define BLOCK_TYPE_T_POS_LEFT 3

#define BLOCK_TYPE_J_POS_UP 0
#define BLOCK_TYPE_J_POS_RIGHT 1
#define BLOCK_TYPE_J_POS_DOWN 2
#define BLOCK_TYPE_J_POS_LEFT 3

#define BLOCK_TYPE_L_POS_UP 0
#define BLOCK_TYPE_L_POS_RIGHT 1
#define BLOCK_TYPE_L_POS_DOWN 2
#define BLOCK_TYPE_L_POS_LEFT 3

#define BLOCK_TYPE_Z_POS_H 0
#define BLOCK_TYPE_Z_POS_V 1

#define BLOCK_TYPE_S_POS_H 0
#define BLOCK_TYPE_S_POS_V 1


// Modes management
// ----------------
#define MODE_NEXT_BLOCK_ON true
#define MODE_NEXT_BLOCK_OFF false

// Position of rectangle containing 'next block' symbol when this mode is enabled
#define NEXT_BLOCK_OFFSET_X 12
#define NEXT_BLOCK_OFFSET_Y 1

#define MODE_MALUS_LINES_ON true
#define MODE_MALUS_LINES_OFF false


// Screensavers management
// -----------------------
#define SCREENSAVER_MADFLY 0
#define SCREENSAVER_SPIN 1
#define SCREENSAVER_HIGH_SCORES 2


// Display stuff
// -------------
#define PIXEL_OFF 0
#define PIXEL_ON 1

#define DECIMAL_POINT_ON true
#define DECIMAL_POINT_OFF false

#define MATRIX_ELEMENTS_HORIZONTAL 2
#define MATRIX_ELEMENTS_VERTICAL 3


// ===========================================================================================================
// GLOBALS VARIABLES
// ===========================================================================================================

uint8_t g_gameState = STATE_BOOTING;
uint16_t g_highScores[2][2] = { { 0, 0 }, { 0, 0 } };  // [MODE_NEXT_BLOCK_ON?][MODE_MALUS_LINES_ON?]
uint16_t g_score = 0;
uint16_t g_nbLinesCompleted = 0;
uint8_t g_level = 0;
uint8_t g_konamiCodePosition = 0;

uint8_t g_selectedScreenSaver = 0;
uint8_t g_screensaver_internal_state = 0;  // specific to each animation
unsigned long g_screensaver_start_ts = 0;  // start TS of first or next screensaver

bool g_modeNextBlock = MODE_NEXT_BLOCK_ON;
bool g_modeMalusLines = MODE_MALUS_LINES_ON;
Metro g_metroMalusLines = Metro(MODE_MALUS_LINES_INTERVAL);
;

unsigned long g_next_allowed_auto_down_ts = 0;
uint16_t g_current_delay_between_auto_down = 500;  // reinit at game startup

uint8_t g_nextBlockType = BLOCK_TYPE_NONE;
uint8_t g_currentBlockType = BLOCK_TYPE_NONE;
uint8_t g_blockRotation;

// Debouncers for buttons
ADebouncer g_debouncerStart;
ADebouncer g_debouncerRotateLeft;
ADebouncer g_debouncerRotateRight;
ADebouncer g_debouncerModeMalus;
ADebouncer g_debouncerModeShowNext;

// Debouncer for joystick is done by hand because we want consecutive actions to be done within the same 'LOW' (pushed) state
unsigned long g_nextAllowedJoystickAutoRepeatActionTS = 0;


bool g_block[PLAYABLE_COLS][PLAYABLE_ROWS + 2];  // 2 extra for rotation
bool g_pile[PLAYABLE_COLS][PLAYABLE_ROWS];       // Row 0 = TOP
bool g_disp[PLAYABLE_COLS][PLAYABLE_ROWS];

Max72xxPanel g_matrix = Max72xxPanel(MAX7219_MATRIX_PIN_CS, MATRIX_ELEMENTS_HORIZONTAL, MATRIX_ELEMENTS_VERTICAL);

// Le MAX72xx du haut commence a row 0, celui du bas fini a row 31
// La x de gauche est 0
// +--------+
// |0,0     |
// ..........
// |    7,31|
// +--------+

LedControl g_dispScore = LedControl(MAX7219_SCORE_PIN_DIN, MAX7219_SCORE_PIN_CLK, MAX7219_SCORE_PIN_CS, 1);

bool g_interruptsEnabled = true;

uint8_t g_matrix_refresh_iteration_count = 0;

// Warning : there are also static variables in screen savers animations


// ===========================================================================================================
void disableInterrupts() {
  g_interruptsEnabled = false;
  cli();
}

void enableInterrupts() {
  g_interruptsEnabled = true;
  sei();  //allow interrupts
}

void configureAndEnableInterrupt() {
  disableInterrupts();

  //set timer0 interrupt at 2kHz
  TCCR1A = 0;  // set entire TCCR0A register to 0
  TCCR1B = 0;  // same for TCCR0B
  TCNT1 = 0;   //initialize counter value to 0
  // set compare match register for 2khz increments
  OCR1A = 259;  // = (16*10^6) / (2000*64) - 1 (must be <256)
  // turn on CTC mode
  TCCR1A |= (1 << WGM01);
  // Set CS11 and CS10 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE0A);

  enableInterrupts();
}


// ===========================================================================================================
void setState(byte newState) {
  if (newState == g_gameState) return;

#ifdef FLAG_DEV_ENABLE_LOGS
  Serial.print("New state ");
  Serial.println(newState);
  Serial.flush();
#endif

  g_gameState = newState;
}


// ===========================================================================================================
// Define matrixes position
// Usefull to do it not only at setup() time but also at each new game due to the fact that wires have a
// tendency to micro cuts , thus reseting matrixes (otherwise until next power up).
void initOutputMatrices(bool showWelcomeMessage) {
#ifdef FLAG_DEV_ENABLE_LOGS
  Serial.println("Matrices: initialization");
#endif

  // Order of matrixes, "first one (0) being the closest to the Arduino"
  // Ids (0 to 7) are hardwired on the 3x2 big matrix
  // +-----+-----+
  // |  4  |  5  |
  // +-----+-----+
  // |  3  |  2  |
  // +-----+-----+
  // |  0  |  1  |
  // +-----+-----+
  // | Ard.|
  // | pins|

  g_matrix.setPosition(0, MATRIX_COL_LEFT, MATRIX_LINE_BOTTOM);
  g_matrix.setPosition(1, MATRIX_COL_RIGHT, MATRIX_LINE_BOTTOM);
  g_matrix.setPosition(2, MATRIX_COL_RIGHT, MATRIX_LINE_MIDDLE);
  g_matrix.setPosition(3, MATRIX_COL_LEFT, MATRIX_LINE_MIDDLE);
  g_matrix.setPosition(4, MATRIX_COL_LEFT, MATRIX_LINE_TOP);
  g_matrix.setPosition(5, MATRIX_COL_RIGHT, MATRIX_LINE_TOP);

  // rotation: 0=none, 1  MATRIX_ROTATION_NONE
  g_matrix.setRotation(0, MATRIX_ROTATION_90_CW);
  g_matrix.setRotation(1, MATRIX_ROTATION_90_CW);
  g_matrix.setRotation(2, MATRIX_ROTATION_90_CCW);
  g_matrix.setRotation(3, MATRIX_ROTATION_90_CCW);
  g_matrix.setRotation(4, MATRIX_ROTATION_90_CW);
  g_matrix.setRotation(5, MATRIX_ROTATION_90_CW);

  g_matrix.setTextSize(1);
  g_matrix.setTextWrap(true);


  // Show that all matrixes are OK - to be used when installing and testing matrixes on the station at build time only
  if (showWelcomeMessage) {
    // A lower intensity since all 6 8x8 are lit, consuming current
    g_matrix.setIntensity(1);

    for (int i = 0; i < 3; i++) {
      g_matrix.fillScreen(PIXEL_ON);
      g_matrix.write();
      digitalWrite(OUTPUT_PIN_BACKLIGHT, LOW);
      delay(250);

      g_matrix.fillScreen(PIXEL_OFF);
      g_matrix.write();
      digitalWrite(OUTPUT_PIN_BACKLIGHT, HIGH);
      delay(350);
    }
    digitalWrite(OUTPUT_PIN_BACKLIGHT, LOW);


    // Check matrices connections are in correct order
    // Letters are the indicators marked on the box.
#ifdef FLAG_DEV_ENABLE_LOGS
    Serial.println("Matrices: displaying Tetris");
#endif

    g_matrix.setIntensity(MATRIX_INTENSITY);

    g_matrix.drawChar(1, 1, 'T', PIXEL_ON, 0, 1);
    g_matrix.drawChar(2, 8, 'e', PIXEL_ON, 0, 1);
    g_matrix.drawChar(3, 16, 't', PIXEL_ON, 0, 1);
    g_matrix.drawChar(8, 0, 'r', PIXEL_ON, 0, 1);
    g_matrix.drawChar(9, 8, 'i', PIXEL_ON, 0, 1);
    g_matrix.drawChar(10, 16, 's', PIXEL_ON, 0, 1);
    g_matrix.write();

    delay(3000);
  }

  g_matrix.setIntensity(MATRIX_INTENSITY);
  g_matrix.fillScreen(PIXEL_OFF);
  g_matrix.write();
}


void setupDisplayScore() {
  // No power save mode
  g_dispScore.shutdown(MAX7219_SCORE_ID, false);

  g_dispScore.setIntensity(MAX7219_SCORE_ID, DISP_SCORE_INTENSITY);

  // Show digits are OK
  sayHello();
}

void setup() {
  delay(SETUP_INITIAL_PAUSE);

  Serial.begin(SERIAL_BAUD_SPEED);

  pinMode(INPUT_PIN_BTN_START, INPUT_PULLUP);
  pinMode(INPUT_PIN_BTN_ROTATE_RIGHT, INPUT_PULLUP);
  pinMode(INPUT_PIN_BTN_ROTATE_LEFT, INPUT_PULLUP);
  pinMode(INPUT_PIN_JOY_DOWN, INPUT_PULLUP);
  pinMode(INPUT_PIN_JOY_RIGHT, INPUT_PULLUP);
  pinMode(INPUT_PIN_JOY_LEFT, INPUT_PULLUP);
  pinMode(INPUT_PIN_JOY_DROP, INPUT_PULLUP);
  pinMode(INPUT_PIN_BTN_MODE_NEXT_PIECE, INPUT_PULLUP);
  pinMode(INPUT_PIN_BTN_MODE_MALUS_LINES, INPUT_PULLUP);

  g_debouncerStart.mode(debounce_t::DELAYED, START_BUTTON_DEBOUNCE, HIGH);         // HIGH because INPUT_PULLUP
  g_debouncerRotateLeft.mode(debounce_t::DELAYED, ROTATE_BUTTON_DEBOUNCE, HIGH);   // HIGH because INPUT_PULLUP
  g_debouncerRotateRight.mode(debounce_t::DELAYED, ROTATE_BUTTON_DEBOUNCE, HIGH);  // HIGH because INPUT_PULLUP
  g_debouncerModeShowNext.mode(debounce_t::DELAYED, MODE_BUTTON_DEBOUNCE, HIGH);   // HIGH because INPUT_PULLUP
  g_debouncerModeMalus.mode(debounce_t::DELAYED, MODE_BUTTON_DEBOUNCE, HIGH);      // HIGH because INPUT_PULLUP


  pinMode(OUTPUT_PIN_BACKLIGHT, OUTPUT);

  // Do not initialize piezo pin here, it generates background noise
  //pinMode(OUTPUT_PIN_PWM_PIEZO, OUTPUT);

  setupDisplayScore();

  initOutputMatrices(true);

  gotoGamePreparation();
}


// ===========================================================================================================

void sayZzz() {
#ifdef FLAG_DEV_ENABLE_LOGS
  Serial.println("Score display: saying '....'");
#endif

  g_dispScore.clearDisplay(MAX7219_SCORE_ID);
  g_dispScore.setChar(MAX7219_SCORE_ID, 5, '.', DECIMAL_POINT_OFF);
  g_dispScore.setChar(MAX7219_SCORE_ID, 4, '.', DECIMAL_POINT_OFF);
  g_dispScore.setChar(MAX7219_SCORE_ID, 3, '.', DECIMAL_POINT_OFF);
}

/*
LedControl. Turning individual segments

Sur un afficheur 7 segments, le MAX7219 est câblé de telle sorte que chaque “digit” est vu comme une “ligne” de 8 LEDs.
Possible characters by default
'0','1','2','3','4','5','6','7','8','9','0',
'A','b','c','d','E','F','H','L','P',
'.','-','_',' '

// To find individual segments :
for (int i = 0; i <= 7; i++) {
  g_dispScore.setChar(MAX7219_SCORE_ID, 7, i, DECIMAL_POINT_OFF);
  g_dispScore.setRow(MAX7219_SCORE_ID, 6, 1 << i);
  delay(1000);
}

  +- bit6 -+
  |        |
 bit1     bit5
  |        |
  +- bit0 -+
  |        |
 bit2    bit4
  |        |
  +- bit3 -+  dp:bit7

 t :  000 1111 : 0x0F
 a :  001 1101 : 0x1D, ou mieux avec le point decimal: 1001 1101 : 0x9D
 r :  000 0101 : 0x05
 ! : 1010 0000 : 0xA0
 [ :  100 1110 : 0x4E
 ] :  111 1000 : 0x78
 O :  111 1110 : 0x7E
*/
#define LEDCONTROL_LETTER_t 0x0F
#define LEDCONTROL_LETTER_a 0x9D
#define LEDCONTROL_LETTER_r 0x05
#define LEDCONTROL_LETTER_O 0x7E
#define LEDCONTROL_SYMBOL_bang 0xA0
#define LEDCONTROL_SYMBOL_BRACKET_LEFT 0x4E
#define LEDCONTROL_SYMBOL_BRACKET_RIGHT 0x78


void sayHello() {
#ifdef FLAG_DEV_ENABLE_LOGS
  Serial.println("Score display: saying 'Hello'");
#endif

  g_dispScore.clearDisplay(MAX7219_SCORE_ID);

  g_dispScore.setChar(MAX7219_SCORE_ID, 6, 'H', DECIMAL_POINT_OFF);
  g_dispScore.setChar(MAX7219_SCORE_ID, 5, 'E', DECIMAL_POINT_OFF);
  g_dispScore.setChar(MAX7219_SCORE_ID, 4, 'L', DECIMAL_POINT_OFF);
  g_dispScore.setChar(MAX7219_SCORE_ID, 3, 'L', DECIMAL_POINT_OFF);
  g_dispScore.setRow(MAX7219_SCORE_ID, 2, LEDCONTROL_LETTER_O);
  g_dispScore.setRow(MAX7219_SCORE_ID, 1, LEDCONTROL_SYMBOL_bang);
}


void sayStart() {
#ifdef FLAG_DEV_ENABLE_LOGS
  Serial.println("Score display: saying 'Start'");
#endif

  g_dispScore.clearDisplay(MAX7219_SCORE_ID);

  g_dispScore.setRow(MAX7219_SCORE_ID, 7, LEDCONTROL_SYMBOL_BRACKET_LEFT);
  g_dispScore.setChar(MAX7219_SCORE_ID, 6, '5', DECIMAL_POINT_OFF);
  g_dispScore.setRow(MAX7219_SCORE_ID, 5, LEDCONTROL_LETTER_t);
  g_dispScore.setRow(MAX7219_SCORE_ID, 4, LEDCONTROL_LETTER_a);
  g_dispScore.setRow(MAX7219_SCORE_ID, 3, LEDCONTROL_LETTER_r);
  g_dispScore.setRow(MAX7219_SCORE_ID, 2, LEDCONTROL_LETTER_t);
  g_dispScore.setRow(MAX7219_SCORE_ID, 1, LEDCONTROL_SYMBOL_bang);
  g_dispScore.setRow(MAX7219_SCORE_ID, 0, LEDCONTROL_SYMBOL_BRACKET_RIGHT);
}


// ===========================================================================================================
void gotoGamePreparation() {
  // Turn off everything
  g_dispScore.clearDisplay(MAX7219_SCORE_ID);
  resetGridAndBuffers();
  updateLEDBuffer();

  g_screensaver_start_ts = millis() + SCREENSAVER_DELAY_BEFORE;
  setState(STATE_GAME_WAIT_START);

  sayStart();

#ifdef FLAG_DEV_READ_SERIAL_INPUTS
  Serial.println("Keyboard inputs:  [i: Start], [jk: Rotate], [fdrg: Joystick]");
#endif

  configureAndEnableInterrupt();
}


// ===========================================================================================================
void gotoScreenSaver() {
  //disableInterrupts();
  g_interruptsEnabled = false;

  sayZzz();

  g_matrix.fillScreen(PIXEL_OFF);
  g_matrix.write();

  // Trigger timestamp of the change to next screensaver
  g_screensaver_start_ts = millis() + SCREENSAVER_DURATION;

  // Select a random screen saver
  g_selectedScreenSaver = random(0, 3);
  g_screensaver_internal_state = 0;

  setState(STATE_SCREENSAVER);
}


// ===========================================================================================================
void initializeRandomGenerator() {
  int seed = (analogRead(0) + 1) * (analogRead(1) + 1) * (analogRead(2) + 1) * (analogRead(3) + 1) * millis();
  randomSeed(seed);
  random(10, 9610806);
  seed = seed * random(3336, 15679912) + analogRead(random(4));
  randomSeed(seed);
  random(10, 98046);
}

// ===========================================================================================================
void resetGridAndBuffers() {
  for (byte x = 0; x < PLAYABLE_COLS; x++) {
    for (byte y = 0; y < PLAYABLE_ROWS; y++) {
      g_block[x][y] = 0;
      g_disp[x][y] = 0;
      g_pile[x][y] = 0;
    }
    for (byte y = PLAYABLE_ROWS; y < PLAYABLE_ROWS + 2; y++) {
      g_block[x][y] = 0;
    }
  }
}

// ===========================================================================================================
void gotoGameStart() {
#ifdef FLAG_DEV_ENABLE_LOGS
  Serial.println("Starting game");
#endif

  readGameModes();

  resetScore();
  resetLevel();

  initializeRandomGenerator();

  initOutputMatrices(false);

  g_dispScore.clearDisplay(MAX7219_SCORE_ID);
  g_matrix.fillScreen(PIXEL_OFF);
  g_matrix.write();

  g_currentBlockType = BLOCK_TYPE_NONE;
  g_nextBlockType = BLOCK_TYPE_NONE;

  popNewBlock();

  updateLEDBuffer();

  if (g_modeMalusLines) {
    g_metroMalusLines.reset();
  }

  setState(STATE_GAME_RUNNING);

  // Restart automatic display
  enableInterrupts();
}

// ===========================================================================================================
void resetLevel() {
  g_nbLinesCompleted = 0;
  g_level = 1;
  adjustSpeedToLevel(g_level);
}

// ===========================================================================================================
void incLevel() {
  g_level++;

#ifdef FLAG_DEV_ENABLE_LOGS
  Serial.print("Level up ");
  Serial.println(g_level);
#endif

  adjustSpeedToLevel(g_level);
}

// ===========================================================================================================

void adjustSpeedToLevel(byte level) {
  if (level <= DOWN_SPEED_LEVEL_LIMIT_1) {
    g_current_delay_between_auto_down = DOWN_SPEED_BASE_1 - (level - 1) * DOWN_SPEED_INCR_1;
  } else if (level <= DOWN_SPEED_LEVEL_LIMIT_2) {
    g_current_delay_between_auto_down = DOWN_SPEED_BASE_2 - (level - DOWN_SPEED_LEVEL_LIMIT_1) * DOWN_SPEED_INCR_2;
  } else {
    g_current_delay_between_auto_down = DOWN_SPEED_LEVEL_LIMIT_2;
  }

#ifdef FLAG_DEV_ENABLE_LOGS
  Serial.print("New speed ");
  Serial.println(g_current_delay_between_auto_down);
#endif
}

// ===========================================================================================================

void readGameModes() {

  g_modeNextBlock = (g_debouncerModeShowNext.debounce(digitalRead(INPUT_PIN_BTN_MODE_NEXT_PIECE)) ? MODE_NEXT_BLOCK_ON : MODE_NEXT_BLOCK_OFF);
  g_modeMalusLines = (g_debouncerModeMalus.debounce(digitalRead(INPUT_PIN_BTN_MODE_MALUS_LINES)) ? MODE_MALUS_LINES_ON : MODE_MALUS_LINES_OFF);

  //   g_modeNextBlock = (digitalRead(INPUT_PIN_BTN_MODE_NEXT_PIECE) == 1 ? MODE_NEXT_BLOCK_ON : MODE_NEXT_BLOCK_OFF);
  //   g_modeMalusLines = (digitalRead(INPUT_PIN_BTN_MODE_MALUS_LINES) == 1 ? MODE_MALUS_LINES_ON : MODE_MALUS_LINES_OFF);
}

// ===========================================================================================================
void loop() {
  // ---------------------------------------------------------------
  // STATE_GAME_RUNNING
  // ---------------------------------------------------------------
  // Possible actions are :
  // - joystick
  // - rotation
  // Goto GAME_OVER when : nextblock / moveD* / malus line
  if (g_gameState == STATE_GAME_RUNNING) {
    bool bGameStillRunning = true;

    if (g_modeMalusLines && g_metroMalusLines.check()) {
      g_metroMalusLines.reset();
      bGameStillRunning = pushMalusLine();
    }

    if (bGameStillRunning && millis() > g_next_allowed_auto_down_ts) {
      bGameStillRunning = moveBlockDown(true);
      g_next_allowed_auto_down_ts = millis() + g_current_delay_between_auto_down;
    }

    if (bGameStillRunning) {
      uint8_t action = readAction();

      if (action == ACTION_BTN_ROTATE_RIGHT) rotateBlock(true);  // version ok
      else if (action == ACTION_BTN_ROTATE_LEFT) rotateBlock(false);
      else if (action == ACTION_JOY_RIGHT) moveBlockRight();
      else if (action == ACTION_JOY_LEFT) moveBlockLeft();
      else if (action == ACTION_JOY_DOWN) bGameStillRunning = moveBlockDown(false);
      else if (action == ACTION_JOY_DROP) bGameStillRunning = moveBlockDrop();
      else if (action == ACTION_BTN_START) bGameStillRunning = false;
    }

    if (!bGameStillRunning) {
      gotoGameOver();
      resetGridAndBuffers();
      g_konamiCodePosition = 0;
#ifdef FLAG_DEV_ENABLE_LOGS
      Serial.println("GAME OVER");
#endif
    }

  }

  // ---------------------------------------------------------------
  // STATE_GAME_WAIT_START
  // ---------------------------------------------------------------
  // Possible actions are :
  // - start
  // - update modes
  // - update konami code with joystick
  else if (g_gameState == STATE_GAME_WAIT_START) {
    if (millis() > g_screensaver_start_ts) {
      gotoScreenSaver();
    }

    readGameModes();

    uint8_t action = readAction();

    if (g_konamiCodePosition == 0 && action == ACTION_JOY_DROP) g_konamiCodePosition = 1;
    else if (g_konamiCodePosition == 1 && action == ACTION_JOY_DROP) g_konamiCodePosition = 2;
    else if (g_konamiCodePosition == 2 && action == ACTION_JOY_DOWN) g_konamiCodePosition = 3;
    else if (g_konamiCodePosition == 3 && action == ACTION_JOY_DOWN) g_konamiCodePosition = 4;
    else if (g_konamiCodePosition == 4 && action == ACTION_JOY_LEFT) g_konamiCodePosition = 5;
    else if (g_konamiCodePosition == 5 && action == ACTION_JOY_RIGHT) g_konamiCodePosition = 6;
    else if (g_konamiCodePosition == 6 && action == ACTION_JOY_LEFT) g_konamiCodePosition = 7;
    else if (g_konamiCodePosition == 7 && action == ACTION_JOY_RIGHT) {
      g_konamiCodePosition = 0;

      animationLevelUp();
      playTetrisTheme();
    }

    else if (action == ACTION_BTN_START) {
      gotoGameStart();
    }
  }

  // ---------------------------------------------------------------
  // STATE_SCREENSAVER
  // ---------------------------------------------------------------
  // Possible actions are :
  // - any button/switch to exit screensaver
  else if (g_gameState == STATE_SCREENSAVER) {
    // Exit screensaver
    if (readAction() != ACTION_NONE) {
      initOutputMatrices(false);
      gotoGamePreparation();
    }
    // Change screensaver
    else if (millis() > g_screensaver_start_ts) {
      initOutputMatrices(false);
      gotoScreenSaver();
    }
    // Continue screensaver
    else {
      continueScreenSaver();
    }
  }
}


// ===========================================================================================================
void continueScreenSaver() {
  if (g_selectedScreenSaver == SCREENSAVER_MADFLY) continueScreenSaver_MadFly();
  else if (g_selectedScreenSaver == SCREENSAVER_SPIN) continueScreenSaver_Spin();
  else if (g_selectedScreenSaver == SCREENSAVER_HIGH_SCORES) continueScreenSaver_HighScores();
}

// ===========================================================================================================
void continueScreenSaver_MadFly() {
  static byte x = g_matrix.width() / 2;
  static byte y = g_matrix.height() / 2;
  static byte xNext, yNext;

  g_matrix.drawPixel(x, y, PIXEL_ON);
  g_matrix.write();
  delay(SCREENSAVER_FLY_SPEED);

  g_matrix.drawPixel(x, y, PIXEL_OFF);  // Erase the old position of our dot

  do {
    switch (random(4)) {
      case 0:
        xNext = constrain(x + 1, 0, g_matrix.width() - 1);
        yNext = y;
        break;
      case 1:
        xNext = constrain(x - 1, 0, g_matrix.width() - 1);
        yNext = y;
        break;
      case 2:
        yNext = constrain(y + 1, 0, g_matrix.height() - 1);
        xNext = x;
        break;
      case 3:
        yNext = constrain(y - 1, 0, g_matrix.height() - 1);
        xNext = x;
        break;
    }
  } while (x == xNext && y == yNext);  // Repeat until we find a new coordinate

  x = xNext;
  y = yNext;
}

// ===========================================================================================================
void continueScreenSaver_Spin() {
  // Based on code from example 'Spin' splitted in steps to allow user's interaction to stop screensaver
  //
  // g_screensaver_internal_state bits : PXVV VVVV
  // P : on pause or g_displaying a new line
  // X : iterating on X axis or Y
  // V : coordinate value (starting point of the line) on the current axis

  static byte wait_msec = 50;
  static int inc = -2;

  bool isOnPause = bitRead(g_screensaver_internal_state, 7);
  bool isMovingOnXAxis = bitRead(g_screensaver_internal_state, 6);
  byte axisPosition = g_screensaver_internal_state & 0x3F;

  if (isOnPause) {
    delay(wait_msec);

    // Preparing next loop iteration : moving along axis
    axisPosition++;

    if (isMovingOnXAxis) {
      if (axisPosition >= g_matrix.width()) {
        axisPosition = 0;
        isMovingOnXAxis = false;

        wait_msec = wait_msec + inc;
        if (wait_msec == 0) inc = 2;
        if (wait_msec == 50) inc = -2;
      }
    } else {
      if (axisPosition >= g_matrix.height()) {
        axisPosition = 0;
        isMovingOnXAxis = true;
      }
    }

    g_screensaver_internal_state = axisPosition + isMovingOnXAxis * bit(6);
  } else {
    g_matrix.fillScreen(LOW);
    if (isMovingOnXAxis) {
      g_matrix.drawLine(axisPosition, 0, g_matrix.width() - 1 - axisPosition, g_matrix.height() - 1, HIGH);
    } else {
      g_matrix.drawLine(g_matrix.width() - 1, axisPosition, 0, g_matrix.height() - 1 - axisPosition, HIGH);
    }
    g_matrix.write();

    // Preparing next loop iteration : going into sleep
    bitSet(g_screensaver_internal_state, 7);
  }
}

// ===========================================================================================================
void continueScreenSaver_HighScores() {
  // g_screensaver_internal_state bits : NM.. iiii
  // N : NextBlock mode enabled
  // M : MalusLines mode enabled
  // i : index of loop on pixels
  bool showScoreForNextBlockEnabled = bitRead(g_screensaver_internal_state, 7);
  bool showScoreForMalusLinesEnabled = bitRead(g_screensaver_internal_state, 6);
  byte i = g_screensaver_internal_state & 0x3F;

  String tape = String(g_highScores[showScoreForNextBlockEnabled][showScoreForMalusLinesEnabled]);

  byte spacer = 1;
  byte width = 5 + spacer;  // The font width is 5 pixels per digit.

  // Loop on all the pixels needed to g_display the message
  //for ( int i = 0 ; i < width * tape.length() + g_matrix.width() - 1 - spacer; i++ ) {
  {
    g_matrix.fillScreen(LOW);
    g_matrix.drawRect(0, 0, 8, 6, 1);
    g_matrix.drawRect(8, 0, 8, 6, 1);
    g_matrix.drawRect(3, 6, 2, 2, 1);
    g_matrix.drawRect(11, 6, 2, 2, 1);
    byte y0 = 3;
    if (showScoreForNextBlockEnabled) {
      byte x0 = 3;
      g_matrix.drawLine(x0 + 0, y0, x0 + 2, y0, 1);
      g_matrix.drawPixel(x0 + 1, y0 - 1, 1);
    }
    if (showScoreForMalusLinesEnabled) {
      byte x0 = 10;
      g_matrix.drawPixel(x0, y0, 1);
      g_matrix.drawPixel(x0 + 2, y0, 1);
      g_matrix.drawPixel(x0 + 3, y0, 1);
    }

    byte letter = i / width;
    short x = (g_matrix.width() - 1) - i % width;  // Must be a signed type for >= 0 comparison
    while (x + width - spacer >= 0 && letter >= 0) {
      if (letter < tape.length()) {
        g_matrix.drawChar(x + 1, 12, tape[letter], HIGH, LOW, 1);
      }

      letter--;
      x -= width;
    }

    // Outer box (done after letters to rewrite on top of letters on left & right sides)
    g_matrix.drawRect(0, 8, 16, 16, 1);
    g_matrix.write();  // Send bitmap to g_display

    delay(SCREENSAVER_HIGHSCORES_SPEED);

    // Push back internal state
    i++;
    if (i == width * tape.length() + g_matrix.width() - 1 - spacer - 2) {  // -2 because of external box using 2 pixels
      // Reset pixel
      i = 0;
      // Update modes
      // NM : 00 => 10 => 11 => 01 => reloop
      if (!showScoreForNextBlockEnabled && !showScoreForMalusLinesEnabled) {
        showScoreForNextBlockEnabled = 1;
      } else if (showScoreForNextBlockEnabled && !showScoreForMalusLinesEnabled) {
        showScoreForMalusLinesEnabled = 1;
      } else if (showScoreForNextBlockEnabled && showScoreForMalusLinesEnabled) {
        showScoreForNextBlockEnabled = 0;
      } else {
        showScoreForMalusLinesEnabled = 0;
      }
    }

    g_screensaver_internal_state = i;
    bitWrite(g_screensaver_internal_state, 7, showScoreForNextBlockEnabled);
    bitWrite(g_screensaver_internal_state, 6, showScoreForMalusLinesEnabled);
  }
}

// ===========================================================================================================
void resetBlockColumn(byte x) {
  for (byte y = 0; y < PLAYABLE_ROWS; y++) {
    g_block[x][y] = 0;
  }
}

// ===========================================================================================================
void resetBlockRow(byte y) {
  for (byte x = 0; x < PLAYABLE_COLS - 1; x++) {
    g_block[x][y] = 0;
  }
}

// ===========================================================================================================
void copyBlockColumn(byte srcCol, byte tgtCol) {
  for (byte y = 0; y < PLAYABLE_ROWS; y++) {
    g_block[tgtCol][y] = g_block[srcCol][y];
  }
}

// ===========================================================================================================

uint8_t readAction() {
  uint8_t action = readActionNoLog();
#ifdef FLAG_DEV_ENABLE_LOGS
  if (action != ACTION_NONE) {
    Serial.print("State: ");
    Serial.print(g_gameState);
    Serial.print(", action: ");
    Serial.print(action);
    Serial.print(", ts=");
    Serial.println(millis());
  }
#endif
  return action;
}

uint8_t readActionNoLog() {

  int incomingByte = 0;
#ifdef FLAG_DEV_READ_SERIAL_INPUTS
  if (Serial.available() > 0) {
    incomingByte = Serial.read();
    if (incomingByte == 10) return ACTION_NONE;  // '\n'
    Serial.println(incomingByte);
  }
#endif

  // Buttons with INPUT_PULLUP have LOW state when pushed.
  // ".falling()" = button having being pushed and not yet released
  // .rising() could work but it's less for user XP nice IMHO

  // Rotations
  // ---------
  g_debouncerRotateLeft.debounce(digitalRead(INPUT_PIN_BTN_ROTATE_LEFT));
  if (incomingByte == 'j' || FLAG_READ_REAL_INPUTS && g_debouncerRotateLeft.falling()) return ACTION_BTN_ROTATE_LEFT;

  g_debouncerRotateRight.debounce(digitalRead(INPUT_PIN_BTN_ROTATE_RIGHT));
  if (incomingByte == 'k' || FLAG_READ_REAL_INPUTS && g_debouncerRotateRight.falling()) return ACTION_BTN_ROTATE_RIGHT;


  // Joystick
  // --------
  if (millis() < g_nextAllowedJoystickAutoRepeatActionTS) {
    return ACTION_NONE;
  }

  if (incomingByte == 'f' || FLAG_READ_REAL_INPUTS && digitalRead(INPUT_PIN_JOY_DOWN) == LOW) {
    g_nextAllowedJoystickAutoRepeatActionTS = millis() + DOWN_BUTTON_AUTOREPEAT_DELAY;
    return ACTION_JOY_DOWN;
  }

  if (incomingByte == 'd' || FLAG_READ_REAL_INPUTS && digitalRead(INPUT_PIN_JOY_LEFT) == LOW) {
    g_nextAllowedJoystickAutoRepeatActionTS = millis() + SIDE_BUTTON_AUTOREPEAT_DELAY;
    return ACTION_JOY_LEFT;
  }

  if (incomingByte == 'g' || FLAG_READ_REAL_INPUTS && digitalRead(INPUT_PIN_JOY_RIGHT) == LOW) {
    g_nextAllowedJoystickAutoRepeatActionTS = millis() + SIDE_BUTTON_AUTOREPEAT_DELAY;
    return ACTION_JOY_RIGHT;
  }

  if (incomingByte == 'r' || FLAG_READ_REAL_INPUTS && digitalRead(INPUT_PIN_JOY_DROP) == LOW) {
    g_nextAllowedJoystickAutoRepeatActionTS = millis() + DROP_BUTTON_AUTOREPEAT_DELAY;
    return ACTION_JOY_DROP;
  }


  // Start (the least important button is processed last)
  // -----
  g_debouncerStart.debounce(digitalRead(INPUT_PIN_BTN_START));
  if (incomingByte == 'i' || FLAG_READ_REAL_INPUTS && g_debouncerStart.falling()) return ACTION_BTN_START;

  return ACTION_NONE;
}

// ===========================================================================================================
boolean moveBlockLeft() {
  if (has1SpaceOnLeft()) {
    for (byte x = 0; x < PLAYABLE_COLS - 1; x++) {
      copyBlockColumn(x + 1, x);
    }
    resetBlockColumn(PLAYABLE_COLS - 1);

    updateLEDBuffer();
    return 1;
  }

  return 0;
}

// ===========================================================================================================
boolean moveBlockRight() {
  if (has1SpaceOnRight()) {
    for (byte x = PLAYABLE_COLS - 1; x > 0; x--) {
      copyBlockColumn(x - 1, x);
    }
    resetBlockColumn(0);

    updateLEDBuffer();
    return 1;
  }
  return 0;
}


// ===========================================================================================================
// Rotations
// ===========================================================================================================

byte getBlockLeftSide() {
  for (byte x = 0; x <= PLAYABLE_COLS - 1; x++) {
    for (byte y = 0; y < PLAYABLE_ROWS; y++) {
      if (g_block[x][y]) {
        return x;
      }
    }
  }
  return PLAYABLE_COLS - 1;
}

byte getBlockTopSide() {
  for (byte y = 0; y <= PLAYABLE_ROWS - 1; y++) {
    for (byte x = 0; x < PLAYABLE_COLS; x++) {
      if (g_block[x][y]) {
        return y;
      }
    }
  }
  return PLAYABLE_ROWS - 1;
}

// ===========================================================================================================

void rotateBlock(bool toRight) {
  if (g_currentBlockType == BLOCK_TYPE_O) return;

  bool rotationDone = false;
  if (g_currentBlockType == BLOCK_TYPE_I) {
    rotationDone = tryRotateBlockI();
  } else if (g_currentBlockType == BLOCK_TYPE_Z) {
    rotationDone = tryRotateBlockZ();
  } else if (g_currentBlockType == BLOCK_TYPE_S) {
    rotationDone = tryRotateBlockS();
  } else if (toRight) {
    rotationDone = tryRotateBlockRight();
  } else {
    rotationDone = tryRotateBlockLeft();
  }

  if (rotationDone) {
    // If rotating made block and pile overlap, push block's rows up
    while (!check_overlap()) {
      for (byte y = 0; y < PLAYABLE_ROWS + 2; y++) {
        for (byte x = 0; x < PLAYABLE_COLS; x++) {
          g_block[x][y] = g_block[x][y + 1];
        }
      }
      g_next_allowed_auto_down_ts = millis() + g_current_delay_between_auto_down;
    }

    updateLEDBuffer();
  }
}


// ===========================================================================================================

bool tryRotateBlockI() {

  //  O         .
  // .G.. <==> OGOO
  //  O         .
  //  O         .

  if (g_blockRotation == BLOCK_TYPE_I_POS_V) {
    if (!has1SpaceOnLeft()) {
      if (has3SpacesOnRight()) {
        if (!moveBlockRight())
          return false;
      } else return false;
    } else if (!has1SpaceOnRight()) {
      if (has3SpacesOnLeft()) {
        if (!moveBlockLeft() || !moveBlockLeft())  // offset twice
          return false;
      } else
        return false;
    } else if (!has2SpacesOnRight()) {
      if (has2SpacesOnLeft()) {
        if (!moveBlockLeft())
          return false;
      } else
        return false;
    }

    byte gravity_x = getBlockLeftSide();
    byte gravity_y = getBlockTopSide() + 1;

    g_block[gravity_x][gravity_y - 1] = 0;
    g_block[gravity_x][gravity_y + 1] = 0;
    g_block[gravity_x][gravity_y + 2] = 0;

    g_block[gravity_x - 1][gravity_y] = 1;
    g_block[gravity_x + 1][gravity_y] = 1;
    g_block[gravity_x + 2][gravity_y] = 1;

    g_blockRotation = BLOCK_TYPE_I_POS_H;
  } else  // BLOCK_TYPE_I_POS_H
  {
    byte gravity_x = getBlockLeftSide() + 1;
    byte gravity_y = getBlockTopSide();

    g_block[gravity_x - 1][gravity_y] = 0;
    g_block[gravity_x + 1][gravity_y] = 0;
    g_block[gravity_x + 2][gravity_y] = 0;

    g_block[gravity_x][gravity_y - 1] = 1;
    g_block[gravity_x][gravity_y + 1] = 1;
    g_block[gravity_x][gravity_y + 2] = 1;

    g_blockRotation = BLOCK_TYPE_I_POS_V;
  }

  return true;
}


bool tryRotateBlockZ() {
  if (g_blockRotation == BLOCK_TYPE_Z_POS_H) {
    // OO
    //  GO
    byte gravity_x = getBlockLeftSide() + 1;
    byte gravity_y = getBlockTopSide() + 1;

    g_block[gravity_x - 1][gravity_y - 1] = 0;
    g_block[gravity_x][gravity_y - 1] = 0;

    g_block[gravity_x + 1][gravity_y - 1] = 1;
    g_block[gravity_x][gravity_y + 1] = 1;

    g_blockRotation = BLOCK_TYPE_Z_POS_V;
  } else {
    //  O     OO
    // GO  =>  GO
    // O
    if (!has1SpaceOnLeft()) {
      if (!moveBlockRight())
        return false;
    }

    byte gravity_x = getBlockLeftSide();
    byte gravity_y = getBlockTopSide() + 1;

    g_block[gravity_x + 1][gravity_y - 1] = 0;
    g_block[gravity_x][gravity_y + 1] = 0;

    g_block[gravity_x - 1][gravity_y - 1] = 1;
    g_block[gravity_x][gravity_y - 1] = 1;

    g_blockRotation = BLOCK_TYPE_Z_POS_H;
  }

  return true;
}

bool tryRotateBlockS() {
  if (g_blockRotation == BLOCK_TYPE_S_POS_H) {
    //  OO
    // OG
    byte gravity_x = getBlockLeftSide() + 1;
    byte gravity_y = getBlockTopSide() + 1;

    g_block[gravity_x + 1][gravity_y - 1] = 0;
    g_block[gravity_x - 1][gravity_y] = 0;

    g_block[gravity_x + 1][gravity_y] = 1;
    g_block[gravity_x + 1][gravity_y + 1] = 1;

    g_blockRotation = BLOCK_TYPE_S_POS_V;
  } else {
    if (!has1SpaceOnLeft()) {
      if (!moveBlockRight())
        return false;
    }

    // O
    // GO
    //  O
    byte gravity_x = getBlockLeftSide();
    byte gravity_y = getBlockTopSide() + 1;

    g_block[gravity_x + 1][gravity_y] = 0;
    g_block[gravity_x + 1][gravity_y + 1] = 0;

    g_block[gravity_x - 1][gravity_y] = 1;
    g_block[gravity_x + 1][gravity_y - 1] = 1;

    g_blockRotation = BLOCK_TYPE_S_POS_H;
  }

  return true;
}

// ===========================================================================================================

// Legacy code ok
bool tryRotateBlockRight() {
  byte gravity_x;
  byte gravity_y;

  if (g_currentBlockType == BLOCK_TYPE_T) {
    if (g_blockRotation == BLOCK_TYPE_T_POS_UP) {
      gravity_x = getBlockLeftSide() + 1;
      gravity_y = getBlockTopSide() + 1;

      g_block[gravity_x - 1][gravity_y] = 0;
      g_block[gravity_x][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_T_POS_RIGHT;
    } else if (g_blockRotation == BLOCK_TYPE_T_POS_RIGHT) {
      if (!has1SpaceOnLeft()) {
        if (!moveBlockRight())
          return false;
      }

      gravity_x = getBlockLeftSide();
      gravity_y = getBlockTopSide() + 1;

      g_block[gravity_x][gravity_y - 1] = 0;
      g_block[gravity_x - 1][gravity_y] = 1;

      g_blockRotation = BLOCK_TYPE_T_POS_DOWN;
    } else if (g_blockRotation == BLOCK_TYPE_T_POS_DOWN) {
      gravity_x = getBlockLeftSide() + 1;
      gravity_y = getBlockTopSide();

      g_block[gravity_x + 1][gravity_y] = 0;
      g_block[gravity_x][gravity_y - 1] = 1;

      g_blockRotation = BLOCK_TYPE_T_POS_LEFT;
    } else  // BLOCK_TYPE_T_POS_LEFT
    {
      if (!has1SpaceOnRight()) {
        if (!moveBlockLeft())
          return false;
      }

      gravity_x = getBlockLeftSide() + 1;
      gravity_y = getBlockTopSide() + 1;

      g_block[gravity_x][gravity_y + 1] = 0;
      g_block[gravity_x + 1][gravity_y] = 1;

      g_blockRotation = BLOCK_TYPE_T_POS_UP;
    }
  }

  else if (g_currentBlockType == BLOCK_TYPE_J) {
    if (g_blockRotation == BLOCK_TYPE_J_POS_UP) {
      gravity_x = getBlockLeftSide() + 1;
      gravity_y = getBlockTopSide() + 1;

      g_block[gravity_x - 1][gravity_y - 1] = 0;
      g_block[gravity_x - 1][gravity_y] = 0;
      g_block[gravity_x + 1][gravity_y] = 0;

      g_block[gravity_x][gravity_y - 1] = 1;
      g_block[gravity_x + 1][gravity_y - 1] = 1;
      g_block[gravity_x][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_J_POS_RIGHT;
    } else if (g_blockRotation == BLOCK_TYPE_J_POS_RIGHT) {
      if (!has1SpaceOnLeft()) {
        if (!moveBlockRight())
          return false;
      }

      gravity_x = getBlockLeftSide();
      gravity_y = getBlockTopSide() + 1;

      g_block[gravity_x][gravity_y - 1] = 0;
      g_block[gravity_x + 1][gravity_y - 1] = 0;
      g_block[gravity_x][gravity_y + 1] = 0;

      g_block[gravity_x - 1][gravity_y] = 1;
      g_block[gravity_x + 1][gravity_y] = 1;
      g_block[gravity_x + 1][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_J_POS_DOWN;
    } else if (g_blockRotation == BLOCK_TYPE_J_POS_DOWN) {
      gravity_x = getBlockLeftSide() + 1;
      gravity_y = getBlockTopSide();

      g_block[gravity_x - 1][gravity_y] = 0;
      g_block[gravity_x + 1][gravity_y] = 0;
      g_block[gravity_x + 1][gravity_y + 1] = 0;

      g_block[gravity_x][gravity_y - 1] = 1;
      g_block[gravity_x][gravity_y + 1] = 1;
      g_block[gravity_x - 1][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_J_POS_LEFT;
    } else  // BLOCK_TYPE_J_POS_LEFT
    {
      if (!has1SpaceOnRight()) {
        if (!moveBlockLeft())
          return false;
      }

      gravity_x = getBlockLeftSide() + 1;
      gravity_y = getBlockTopSide() + 1;

      g_block[gravity_x][gravity_y - 1] = 0;
      g_block[gravity_x][gravity_y + 1] = 0;
      g_block[gravity_x - 1][gravity_y + 1] = 0;

      g_block[gravity_x - 1][gravity_y - 1] = 1;
      g_block[gravity_x - 1][gravity_y] = 1;
      g_block[gravity_x + 1][gravity_y] = 1;

      g_blockRotation = BLOCK_TYPE_J_POS_UP;
    }
  }

  else if (g_currentBlockType == BLOCK_TYPE_L) {
    if (g_blockRotation == BLOCK_TYPE_L_POS_UP) {
      gravity_x = getBlockLeftSide() + 1;
      gravity_y = getBlockTopSide() + 1;

      g_block[gravity_x + 1][gravity_y - 1] = 0;
      g_block[gravity_x - 1][gravity_y] = 0;
      g_block[gravity_x + 1][gravity_y] = 0;

      g_block[gravity_x][gravity_y - 1] = 1;
      g_block[gravity_x + 1][gravity_y + 1] = 1;
      g_block[gravity_x][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_L_POS_RIGHT;
    } else if (g_blockRotation == BLOCK_TYPE_L_POS_RIGHT) {
      if (!has1SpaceOnLeft()) {
        if (!moveBlockRight())
          return false;
      }

      gravity_x = getBlockLeftSide();
      gravity_y = getBlockTopSide() + 1;

      g_block[gravity_x][gravity_y - 1] = 0;
      g_block[gravity_x + 1][gravity_y + 1] = 0;
      g_block[gravity_x][gravity_y + 1] = 0;

      g_block[gravity_x - 1][gravity_y] = 1;
      g_block[gravity_x + 1][gravity_y] = 1;
      g_block[gravity_x - 1][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_L_POS_DOWN;
    } else if (g_blockRotation == BLOCK_TYPE_L_POS_DOWN) {
      gravity_x = getBlockLeftSide() + 1;
      gravity_y = getBlockTopSide();

      g_block[gravity_x - 1][gravity_y] = 0;
      g_block[gravity_x + 1][gravity_y] = 0;
      g_block[gravity_x - 1][gravity_y + 1] = 0;

      g_block[gravity_x - 1][gravity_y - 1] = 1;
      g_block[gravity_x][gravity_y - 1] = 1;
      g_block[gravity_x][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_L_POS_LEFT;
    } else  // BLOCK_TYPE_L_POS_LEFT
    {
      if (!has1SpaceOnRight()) {
        if (!moveBlockLeft())
          return false;
      }

      gravity_x = getBlockLeftSide() + 1;
      gravity_y = getBlockTopSide() + 1;

      g_block[gravity_x][gravity_y - 1] = 0;
      g_block[gravity_x][gravity_y + 1] = 0;
      g_block[gravity_x - 1][gravity_y - 1] = 0;

      g_block[gravity_x + 1][gravity_y - 1] = 1;
      g_block[gravity_x - 1][gravity_y] = 1;
      g_block[gravity_x + 1][gravity_y] = 1;

      g_blockRotation = BLOCK_TYPE_L_POS_UP;
    }
  }

  return true;
}


// ===========================================================================================================

// New
bool tryRotateBlockLeft() {

  byte xtreme_left = getBlockLeftSide();
  byte xtreme_up = getBlockTopSide();
  byte gravity_x;
  byte gravity_y;

  if (g_currentBlockType == BLOCK_TYPE_T) {
    if (g_blockRotation == BLOCK_TYPE_T_POS_UP) {
      gravity_x = xtreme_left + 1;
      gravity_y = xtreme_up + 1;

      g_block[gravity_x + 1][gravity_y] = 0;
      g_block[gravity_x][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_T_POS_LEFT;
    } else if (g_blockRotation == BLOCK_TYPE_T_POS_RIGHT) {
      if (!has1SpaceOnLeft()) {
        if (!moveBlockRight())
          return false;
        xtreme_left++;
      }

      gravity_x = xtreme_left;
      gravity_y = xtreme_up + 1;

      g_block[gravity_x][gravity_y + 1] = 0;
      g_block[gravity_x - 1][gravity_y] = 1;

      g_blockRotation = BLOCK_TYPE_T_POS_UP;
    } else if (g_blockRotation == BLOCK_TYPE_T_POS_DOWN) {
      gravity_x = xtreme_left + 1;
      gravity_y = xtreme_up;

      g_block[gravity_x - 1][gravity_y] = 0;
      g_block[gravity_x][gravity_y - 1] = 1;

      g_blockRotation = BLOCK_TYPE_T_POS_RIGHT;
    } else  // BLOCK_TYPE_T_POS_LEFT
    {
      if (!has1SpaceOnRight()) {
        if (!moveBlockLeft())
          return false;
        xtreme_left--;
      }

      gravity_x = xtreme_left + 1;
      gravity_y = xtreme_up + 1;

      g_block[gravity_x][gravity_y - 1] = 0;
      g_block[gravity_x + 1][gravity_y] = 1;

      g_blockRotation = BLOCK_TYPE_T_POS_DOWN;
    }
  }

  else if (g_currentBlockType == BLOCK_TYPE_J) {
    if (g_blockRotation == BLOCK_TYPE_J_POS_UP) {
      gravity_x = xtreme_left + 1;
      gravity_y = xtreme_up + 1;

      g_block[gravity_x - 1][gravity_y - 1] = 0;
      g_block[gravity_x - 1][gravity_y] = 0;
      g_block[gravity_x + 1][gravity_y] = 0;

      g_block[gravity_x][gravity_y - 1] = 1;
      g_block[gravity_x - 1][gravity_y + 1] = 1;
      g_block[gravity_x][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_J_POS_LEFT;
    } else if (g_blockRotation == BLOCK_TYPE_J_POS_RIGHT) {
      if (!has1SpaceOnLeft()) {
        if (!moveBlockRight())
          return false;
        xtreme_left++;
      }

      gravity_x = xtreme_left;
      gravity_y = xtreme_up + 1;

      g_block[gravity_x][gravity_y - 1] = 0;
      g_block[gravity_x + 1][gravity_y - 1] = 0;
      g_block[gravity_x][gravity_y + 1] = 0;

      g_block[gravity_x - 1][gravity_y - 1] = 1;
      g_block[gravity_x - 1][gravity_y] = 1;
      g_block[gravity_x + 1][gravity_y] = 1;

      g_blockRotation = BLOCK_TYPE_J_POS_UP;
    } else if (g_blockRotation == BLOCK_TYPE_J_POS_DOWN) {
      gravity_x = xtreme_left + 1;
      gravity_y = xtreme_up;


      g_block[gravity_x - 1][gravity_y] = 0;
      g_block[gravity_x + 1][gravity_y] = 0;
      g_block[gravity_x + 1][gravity_y + 1] = 0;

      g_block[gravity_x][gravity_y - 1] = 1;
      g_block[gravity_x + 1][gravity_y - 1] = 1;
      g_block[gravity_x][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_J_POS_RIGHT;
    } else  // BLOCK_TYPE_J_POS_LEFT
    {
      if (!has1SpaceOnRight()) {
        if (!moveBlockLeft())
          return false;
        xtreme_left--;
      }

      gravity_x = xtreme_left + 1;
      gravity_y = xtreme_up + 1;

      g_block[gravity_x][gravity_y - 1] = 0;
      g_block[gravity_x][gravity_y + 1] = 0;
      g_block[gravity_x - 1][gravity_y + 1] = 0;

      g_block[gravity_x - 1][gravity_y] = 1;
      g_block[gravity_x + 1][gravity_y] = 1;
      g_block[gravity_x + 1][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_J_POS_DOWN;
    }
  }

  else if (g_currentBlockType == BLOCK_TYPE_L) {
    if (g_blockRotation == BLOCK_TYPE_L_POS_UP) {
      gravity_x = xtreme_left + 1;
      gravity_y = xtreme_up + 1;

      g_block[gravity_x + 1][gravity_y - 1] = 0;
      g_block[gravity_x - 1][gravity_y] = 0;
      g_block[gravity_x + 1][gravity_y] = 0;

      g_block[gravity_x - 1][gravity_y - 1] = 1;
      g_block[gravity_x][gravity_y - 1] = 1;
      g_block[gravity_x][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_L_POS_LEFT;
    } else if (g_blockRotation == BLOCK_TYPE_L_POS_RIGHT) {
      if (!has1SpaceOnLeft()) {
        if (!moveBlockRight())
          return false;
        xtreme_left++;
      }

      gravity_x = xtreme_left;
      gravity_y = xtreme_up + 1;

      g_block[gravity_x][gravity_y - 1] = 0;
      g_block[gravity_x + 1][gravity_y + 1] = 0;
      g_block[gravity_x][gravity_y + 1] = 0;

      g_block[gravity_x + 1][gravity_y - 1] = 1;
      g_block[gravity_x - 1][gravity_y] = 1;
      g_block[gravity_x + 1][gravity_y] = 1;

      g_blockRotation = BLOCK_TYPE_L_POS_UP;
    } else if (g_blockRotation == BLOCK_TYPE_L_POS_DOWN) {
      gravity_x = xtreme_left + 1;
      gravity_y = xtreme_up;

      g_block[gravity_x - 1][gravity_y] = 0;
      g_block[gravity_x + 1][gravity_y] = 0;
      g_block[gravity_x - 1][gravity_y + 1] = 0;

      g_block[gravity_x][gravity_y - 1] = 1;
      g_block[gravity_x][gravity_y + 1] = 1;
      g_block[gravity_x + 1][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_L_POS_RIGHT;
    } else  // BLOCK_TYPE_L_POS_LEFT
    {
      if (!has1SpaceOnRight()) {
        if (!moveBlockLeft())
          return false;
        xtreme_left--;
      }

      gravity_x = xtreme_left + 1;
      gravity_y = xtreme_up + 1;

      g_block[gravity_x][gravity_y - 1] = 0;
      g_block[gravity_x][gravity_y + 1] = 0;
      g_block[gravity_x - 1][gravity_y - 1] = 0;

      g_block[gravity_x - 1][gravity_y] = 1;
      g_block[gravity_x + 1][gravity_y] = 1;
      g_block[gravity_x - 1][gravity_y + 1] = 1;

      g_blockRotation = BLOCK_TYPE_L_POS_DOWN;
    }
  }

  return true;
}

// ===========================================================================================================
bool moveBlockOneLineDownIfSpaceBelow() {
  if (isSpaceBelowCurrentBlock()) {
    for (short y = PLAYABLE_ROWS - 1; y >= 0; y--) {
      for (byte x = 0; x < PLAYABLE_COLS; x++) {
        g_block[x][y] = g_block[x][y - 1];
      }
    }
    resetBlockRow(0);
    return ONE_LINE_DOWN_SUCCESSFULL;
  }
  return ONE_LINE_DOWN_BLOCKED;
}

// ===========================================================================================================
bool mergeThenReduceThenGameOverOrNewBlock() {
  for (byte x = 0; x < PLAYABLE_COLS; x++) {
    for (byte y = 0; y < PLAYABLE_ROWS; y++) {
      if (g_block[x][y]) {
        g_pile[x][y] = 1;
        g_block[x][y] = 0;
      }
    }
  }
  return reduceThenGameOverOrNewBlock();
}

// ===========================================================================================================
bool pushMalusLine() {
  if (!isFirstLineEmpty()) return false;

  // No space below current g_block is same as looking if grid can be pushed upwards
  if (!isSpaceBelowCurrentBlock()) return false;

  for (byte y = 1; y < PLAYABLE_ROWS; y++) {
    for (byte x = 0; x < PLAYABLE_COLS; x++) {
      g_pile[x][y - 1] = g_pile[x][y];
    }
  }
  for (byte x = 0; x < PLAYABLE_COLS; x++) {
    g_pile[x][PLAYABLE_ROWS - 1] = random(0, 2);
  }

  updateLEDBuffer();
  return true;

  bool gameStillRunning = mergeThenReduceThenGameOverOrNewBlock();
  return gameStillRunning;
}

// ===========================================================================================================
bool moveBlockDown(bool naturalDown) {
  bool gameStillRunning = true;
  if (moveBlockOneLineDownIfSpaceBelow() == ONE_LINE_DOWN_SUCCESSFULL) {
    if (!naturalDown) {
      incScore(SCORE_PER_DOWN_PER_LINE);
    }
  } else {
    gameStillRunning = mergeThenReduceThenGameOverOrNewBlock();
  }
  updateLEDBuffer();
  return gameStillRunning;
}

// ===========================================================================================================
bool moveBlockDrop() {
  bool gameStillRunning;
  while (moveBlockOneLineDownIfSpaceBelow() == ONE_LINE_DOWN_SUCCESSFULL) {
    incScore(SCORE_PER_DROP_PER_LINE);
  };
  gameStillRunning = mergeThenReduceThenGameOverOrNewBlock();
  updateLEDBuffer();
  return gameStillRunning;
}

// ===========================================================================================================
boolean check_overlap() {
  for (byte y = 0; y < PLAYABLE_ROWS; y++) {
    for (byte x = 0; x < PLAYABLE_COLS - 1; x++) {
      if (g_block[x][y]) {
        if (g_pile[x][y])
          return false;
      }
    }
  }
  for (byte y = PLAYABLE_ROWS; y < PLAYABLE_ROWS + 2; y++) {
    for (byte x = 0; x < PLAYABLE_COLS - 1; x++) {
      if (g_block[x][y]) {
        return false;
      }
    }
  }
  return true;
}

// ===========================================================================================================
bool reduceAndCheckGameOver() {
  byte cnt = 0;
  byte nbLinesCompletedInSequence = 0;

  // Start with lower row
  for (short y = PLAYABLE_ROWS - 1; y >= 0; y--) {
    cnt = 0;
    for (byte x = 0; x < PLAYABLE_COLS; x++) {
      cnt += g_pile[x][y];
    }

    // A row is full
    if (cnt == PLAYABLE_COLS) {
      nbLinesCompletedInSequence++;
      scoreOneLineCompleted(nbLinesCompletedInSequence);

      for (byte x = 0; x < PLAYABLE_COLS; x++) {
        g_pile[x][y] = 0;
      }
      updateLEDBuffer();
      delay(50);

      // Collapse above rows one row below
      for (short k = y; k > 0; k--) {
        for (byte x = 0; x < PLAYABLE_COLS; x++) {
          g_pile[x][k] = g_pile[x][k - 1];
        }
      }
      for (byte x = 0; x < PLAYABLE_COLS; x++) {
        g_pile[x][0] = 0;
      }
      updateLEDBuffer();
      delay(50);
      // Restart looking for a full row with the same row level
      y++;
    }
  }

  // Game over if first line is touched
  bool gameStillRunning = isFirstLineEmpty();
  return gameStillRunning;
}

// ===========================================================================================================
bool isFirstLineEmpty() {
  for (byte x = 0; x < PLAYABLE_COLS; x++) {
    if (g_pile[x][0]) {
      return false;
    }
  }
  return true;
}

// ===========================================================================================================
void gotoGameOver() {
  setState(STATE_GAME_OVER);

  unsigned short score = getScore();
  if (score > g_highScores[g_modeNextBlock][g_modeMalusLines]) {
    g_highScores[g_modeNextBlock][g_modeMalusLines] = score;

#ifdef FLAG_DEV_ENABLE_LOGS
    Serial.print("New high score ");
    Serial.println(g_highScores[g_modeNextBlock][g_modeMalusLines]);
#endif
  }

  gameOverAnimation();

  resetScore();
  resetLevel();

  //disableInterrupts();

  setState(STATE_GAME_WAIT_START);
}


// ===========================================================================================================
void gameOverAnimation() {
  for (byte x = 0; x < PLAYABLE_COLS; x++) {
    for (byte y = 0; y < PLAYABLE_ROWS; y++) {
      if (y % 2) {
        g_disp[x][y] = 1;
      } else {
        g_disp[PLAYABLE_COLS - 1 - x][y] = 1;
      }
    }
    delay(60);
  }

  for (byte x = 0; x < PLAYABLE_COLS; x++) {
    for (byte y = 0; y < PLAYABLE_ROWS; y++) {
      if (y % 2) {
        g_disp[x][y] = 0;
      } else {
        g_disp[PLAYABLE_COLS - 1 - x][y] = 0;
      }
    }
    delay(60);
  }
  delay(GAME_OVER_POST_ANIMATION_PAUSE);
}

// ===========================================================================================================
bool reduceThenGameOverOrNewBlock() {
  bool gameStillRunning = reduceAndCheckGameOver();
  if (!gameStillRunning) return false;

  popNewBlock();
  return gameStillRunning;
}

// ===========================================================================================================
void popNewBlock() {
  // For first block of the game
  if (g_nextBlockType == BLOCK_TYPE_NONE) {
    g_nextBlockType = random(NB_BLOCK_TYPES);
  }
  g_currentBlockType = g_nextBlockType;
  g_nextBlockType = random(NB_BLOCK_TYPES);

  byte midColumn = floor((PLAYABLE_COLS) / 2) - 1;

  if (g_currentBlockType == BLOCK_TYPE_I) {
    g_blockRotation = BLOCK_TYPE_I_POS_V;

    g_block[midColumn][0] = 1;
    g_block[midColumn][1] = 1;
    g_block[midColumn][2] = 1;
    g_block[midColumn][3] = 1;
    return;
  }

  if (g_currentBlockType == BLOCK_TYPE_J) {
    // 0
    // 0 0 0
    g_blockRotation = BLOCK_TYPE_J_POS_UP;

    g_block[midColumn - 1][0] = 1;
    g_block[midColumn - 1][1] = 1;
    g_block[midColumn][1] = 1;
    g_block[midColumn + 1][1] = 1;
    return;
  }

  if (g_currentBlockType == BLOCK_TYPE_L) {
    //     0
    // 0 0 0
    g_blockRotation = BLOCK_TYPE_L_POS_UP;

    g_block[midColumn + 1][0] = 1;
    g_block[midColumn - 1][1] = 1;
    g_block[midColumn][1] = 1;
    g_block[midColumn + 1][1] = 1;
    return;
  }

  if (g_currentBlockType == BLOCK_TYPE_O) {
    // 0 0
    // 0 0
    g_block[midColumn][0] = 1;
    g_block[midColumn][1] = 1;
    g_block[midColumn + 1][0] = 1;
    g_block[midColumn + 1][1] = 1;
    return;
  }

  if (g_currentBlockType == BLOCK_TYPE_S) {
    //   0 0
    // 0 0
    g_blockRotation = BLOCK_TYPE_S_POS_H;

    g_block[midColumn][1] = 1;
    g_block[midColumn + 1][0] = 1;
    g_block[midColumn + 1][1] = 1;
    g_block[midColumn + 2][0] = 1;
    return;
  }

  if (g_currentBlockType == BLOCK_TYPE_T) {
    //   0
    // 0 0 0
    g_blockRotation = BLOCK_TYPE_T_POS_UP;

    g_block[midColumn][1] = 1;
    g_block[midColumn + 1][0] = 1;
    g_block[midColumn + 1][1] = 1;
    g_block[midColumn + 2][1] = 1;
    return;
  }

  if (g_currentBlockType == BLOCK_TYPE_Z) {
    // 0 0
    //   0 0
    g_blockRotation = BLOCK_TYPE_Z_POS_H;

    g_block[midColumn][0] = 1;
    g_block[midColumn + 1][0] = 1;
    g_block[midColumn + 1][1] = 1;
    g_block[midColumn + 2][1] = 1;
    return;
  }
}

// ===========================================================================================================
boolean isSpaceBelowCurrentBlock() {
  for (short y = PLAYABLE_ROWS - 1; y >= 0; y--) {
    for (byte x = 0; x < PLAYABLE_COLS; x++) {
      if (g_block[x][y]) {
        if (y == PLAYABLE_ROWS - 1)
          return false;
        if (g_pile[x][y + 1]) {
          return false;
        }
      }
    }
  }
  return true;
}

// ===========================================================================================================
boolean has1SpaceOnLeft() {
  for (short y = PLAYABLE_ROWS - 1; y >= 0; y--) {
    for (byte x = 0; x < PLAYABLE_COLS; x++) {
      if (g_block[x][y]) {
        if (x == 0 || g_pile[x - 1][y])
          return false;
      }
    }
  }
  return true;
}

// ===========================================================================================================
boolean has2SpacesOnLeft() {
  for (short y = PLAYABLE_ROWS - 1; y >= 0; y--) {
    for (byte x = 0; x < PLAYABLE_COLS; x++) {
      if (g_block[x][y]) {
        if (x == 0 || x == 1 || g_pile[x - 1][y] | g_pile[x - 2][y])
          return false;
      }
    }
  }
  return true;
}

// ===========================================================================================================
boolean has3SpacesOnLeft() {
  for (short y = PLAYABLE_ROWS - 1; y >= 0; y--) {
    for (byte x = 0; x < PLAYABLE_COLS; x++) {
      if (g_block[x][y]) {
        if (x == 0 || x == 1 || x == 2 || g_pile[x - 1][y] | g_pile[x - 2][y] | g_pile[x - 3][y])
          return false;
      }
    }
  }
  return true;
}

// ===========================================================================================================
boolean has1SpaceOnRight() {
  for (short y = PLAYABLE_ROWS - 1; y >= 0; y--) {
    for (byte x = 0; x < PLAYABLE_COLS; x++) {
      if (g_block[x][y]) {
        if (x == PLAYABLE_COLS - 1 || g_pile[x + 1][y])
          return false;
      }
    }
  }
  return true;
}

// ===========================================================================================================
boolean has2SpacesOnRight() {
  for (short y = PLAYABLE_ROWS - 1; y >= 0; y--) {
    for (byte x = 0; x < PLAYABLE_COLS; x++) {
      if (g_block[x][y]) {
        if (x == PLAYABLE_COLS - 1 || x == PLAYABLE_COLS - 2 || g_pile[x + 1][y] | g_pile[x + 2][y])
          return false;
      }
    }
  }
  return true;
}

// ===========================================================================================================
boolean has3SpacesOnRight() {
  for (short y = PLAYABLE_ROWS - 1; y >= 0; y--) {
    for (byte x = 0; x < PLAYABLE_COLS; x++) {
      if (g_block[x][y]) {
        if (x == PLAYABLE_COLS - 1 || x == PLAYABLE_COLS - 2 || x == PLAYABLE_COLS - 3 || g_pile[x + 1][y] | g_pile[x + 2][y] | g_pile[x + 3][y])
          return false;
      }
    }
  }
  return true;
}

// ===========================================================================================================

ISR(TIMER1_COMPA_vect) {  //change the 0 to 1 for timer1 and 2 for timer2
  if (!g_interruptsEnabled) return;

  if (g_matrix_refresh_iteration_count == 0) {
    // Uncomment to have the grid on Serial. Usefull when no electronic ready
    //showPlayableZoneOnSerial();

    if (g_gameState == STATE_GAME_RUNNING) {
      dispRefreshScore();
    }
    dispRefreshMatrix();
  }

  g_matrix_refresh_iteration_count++;
  if (g_matrix_refresh_iteration_count > MATRIX_REFRESH_RATE_REDUCE_FACTOR) {
    g_matrix_refresh_iteration_count = 0;
  }
}

// ===========================================================================================================
void updateLEDBuffer() {
  for (byte x = 0; x < PLAYABLE_COLS; x++) {
    for (byte y = 0; y < PLAYABLE_ROWS; y++) {
      g_disp[x][y] = g_block[x][y] | g_pile[x][y];
    }
  }
}

// ===========================================================================================================
void showPlayableZoneOnSerial() {
#ifdef FLAG_DEV_ENABLE_LOGS
  Serial.print("===== LVL:");
  Serial.print(g_level);
  Serial.print(" SPD:");
  Serial.print(g_current_delay_between_auto_down);
  Serial.print(" SCO:");
  Serial.println(g_score);

  for (byte y = 0; y < PLAYABLE_ROWS; y++) {
    for (byte x = 0; x < PLAYABLE_COLS; x++) {
      Serial.print(g_disp[x][y] == true ? 'X' : '.');
    }
    Serial.println("");
  }
#endif
}

// ===========================================================================================================
void resetScore() {
  g_score = 0;
}

// ===========================================================================================================
void scoreOneLineCompleted(byte nbLinesCompletedInSequence) {
  incScore(SCORE_PER_COMPLETED_LINE_FACTOR * nbLinesCompletedInSequence);

  g_nbLinesCompleted++;

  if (g_nbLinesCompleted >= g_level * LINES_PER_LEVEL) {
    animationLevelUp();
    incLevel();
  }
}

// ===========================================================================================================
void animationLevelUp() {
  for (byte y = 0; y < PLAYABLE_ROWS; y++) {
    for (byte x = 0; x < PLAYABLE_COLS; x++) {
      g_disp[x][y] = 1;
    }
  }
  delay(LEVELUP_FLASH_DELAY);

  // Restore normal grid content
  updateLEDBuffer();
}

// ===========================================================================================================
void incScore(unsigned short inc) {
  g_score += inc;
}

// ===========================================================================================================
unsigned short getScore() {
  return g_score;
}

// ===========================================================================================================
void dispRefreshScore() {
  g_matrix.fillScreen(PIXEL_OFF);

  unsigned int score = getScore();  // keep 'int' type for last modulo

  // Level on the left of the "8 7-segments digits g_display"
  g_dispScore.setDigit(MAX7219_SCORE_ID, 6, g_level % 10, DECIMAL_POINT_OFF);
  g_dispScore.setDigit(MAX7219_SCORE_ID, 7, g_level / 10, DECIMAL_POINT_OFF);

  // Score with digits on the right of the "8 7-segments digits g_display"
  g_dispScore.setDigit(MAX7219_SCORE_ID, 0, score % 10, DECIMAL_POINT_OFF);
  if (score >= 10) {
    g_dispScore.setDigit(MAX7219_SCORE_ID, 1, (score % 100) / 10, DECIMAL_POINT_OFF);
    if (score >= 100) {
      g_dispScore.setDigit(MAX7219_SCORE_ID, 2, (score % 1000) / 100, DECIMAL_POINT_OFF);
      if (score >= 1000) {
        g_dispScore.setDigit(MAX7219_SCORE_ID, 3, (score % 10000) / 1000, DECIMAL_POINT_OFF);
        if (score >= 10000) {
          g_dispScore.setDigit(MAX7219_SCORE_ID, 4, (score % 100000) / 10000, DECIMAL_POINT_OFF);
        }
      }
    }
  }
}


void dispRefreshMatrix() {
  g_matrix.fillScreen(PIXEL_OFF);

  unsigned int score = getScore();  // keep 'int' type for last modulo

  // Limit of playable grid within a 2x3 matrix
  g_matrix.drawFastVLine(10, 0, PLAYABLE_ROWS, PIXEL_ON);


  // ============= Malus mode
  if (g_modeMalusLines == MODE_MALUS_LINES_ON) {
    g_matrix.drawFastHLine(11, PLAYABLE_ROWS - 1, 5, PIXEL_ON);
  }

  // ============= Next block mode
  if (g_modeNextBlock == MODE_NEXT_BLOCK_ON) {
    // Horizontal limit for the zone
    g_matrix.drawFastHLine(11, 6, 5, PIXEL_ON);

    // Empty zone
    g_matrix.fillRect(NEXT_BLOCK_OFFSET_X, NEXT_BLOCK_OFFSET_Y, 3, 4, PIXEL_OFF);

    if (g_nextBlockType == BLOCK_TYPE_I) {
      g_matrix.drawFastVLine(NEXT_BLOCK_OFFSET_X + 1, NEXT_BLOCK_OFFSET_Y, 4, PIXEL_ON);
    } else if (g_nextBlockType == BLOCK_TYPE_J) {
      // 0
      // 0 0 0
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 0, NEXT_BLOCK_OFFSET_Y + 1, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 0, NEXT_BLOCK_OFFSET_Y + 2, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 1, NEXT_BLOCK_OFFSET_Y + 2, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 2, NEXT_BLOCK_OFFSET_Y + 2, PIXEL_ON);
    } else if (g_nextBlockType == BLOCK_TYPE_L) {
      //     0
      // 0 0 0
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 2, NEXT_BLOCK_OFFSET_Y + 1, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 0, NEXT_BLOCK_OFFSET_Y + 2, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 1, NEXT_BLOCK_OFFSET_Y + 2, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 2, NEXT_BLOCK_OFFSET_Y + 2, PIXEL_ON);
    } else if (g_nextBlockType == BLOCK_TYPE_S) {
      //   0 0
      // 0 0
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 1, NEXT_BLOCK_OFFSET_Y + 1, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 2, NEXT_BLOCK_OFFSET_Y + 1, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 0, NEXT_BLOCK_OFFSET_Y + 2, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 1, NEXT_BLOCK_OFFSET_Y + 2, PIXEL_ON);
    } else if (g_nextBlockType == BLOCK_TYPE_Z) {
      // 0 0
      //   0 0
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 0, NEXT_BLOCK_OFFSET_Y + 1, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 1, NEXT_BLOCK_OFFSET_Y + 1, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 1, NEXT_BLOCK_OFFSET_Y + 2, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 2, NEXT_BLOCK_OFFSET_Y + 2, PIXEL_ON);
    } else if (g_nextBlockType == BLOCK_TYPE_T) {
      //   0
      // 0 0 0
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 1, NEXT_BLOCK_OFFSET_Y + 1, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 0, NEXT_BLOCK_OFFSET_Y + 2, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 1, NEXT_BLOCK_OFFSET_Y + 2, PIXEL_ON);
      g_matrix.drawPixel(NEXT_BLOCK_OFFSET_X + 2, NEXT_BLOCK_OFFSET_Y + 2, PIXEL_ON);
    } else if (g_nextBlockType == BLOCK_TYPE_O) {
      g_matrix.fillRect(NEXT_BLOCK_OFFSET_X + 0, NEXT_BLOCK_OFFSET_Y + 1, 2, 2, PIXEL_ON);
    }
  }

  // Score in binary on right side of the playable zone
  for (byte b = 0; b < 10; b++) {
    g_matrix.drawPixel(15, PLAYABLE_ROWS - 3 - b, score & bit(b));
  }

  // Grid
  for (byte y = 0; y < PLAYABLE_ROWS; y++) {
    for (byte x = 0; x < PLAYABLE_COLS; x++) {
      g_matrix.drawPixel(x, y + OFFSET_ROW, g_disp[x][y] ? PIXEL_ON : PIXEL_OFF);
    }
  }
  g_matrix.write();
}


// START MUSIC ***************************************************************************************************************************************

#ifdef FLAG_ENABLE_MUSIC

// Beats per minute = speed of music
#define BPM (160.0)

// Time (in microseconds) to spend on each note while simulating polyphony
// If this is too small, low frequency notes will be inaudible.
#define POLY_DELTA (14400)

// A rest
#define _R (0)

// Note frequencies are based on http://www.phy.mtu.edu/~suits/notefreqs.html
// Frequencies have been divided by 10 and rounded because Nano does not have enough memory to store those notes values inside unsigned ints (0-64k)
#define HERTZ_DIVIDER 10

#define _GS1 (519)
#define _A1 (550)
#define _B1 (617)
#define _C2 (654)
#define _D2 (734)
#define _E2 (824)
#define _F2 (873)
#define _G2 (980)
#define _GS2 (1038)
#define _A2 (1100)
#define _B2 (1235)
#define _C3 (1308)
#define _D3 (1468)
#define _E3 (1648)
#define _F3 (1746)
#define _G3 (1960)
#define _GS3 (2077)
#define _A3 (2200)
#define _B3 (2469)
#define _C4 (2616)
#define _D4 (2937)
#define _E4 (3296)
#define _F4 (3492)
#define _G4 (3920)
#define _GS4 (4153)
#define _A4 (4400)
#define _B4 (4939)
#define _C5 (5233)
#define _D5 (5873)
#define _E5 (6593)
#define _F5 (6985)
#define _G5 (7840)
#define _A5 (8800)

unsigned short lead_symbolic_freqs[] = {
  // part 1
  _E5, _B4, _C5, _D5, _C5, _B4, _A4, _A4, _C5, _E5, _D5, _C5, _B4, _B4, _C5, _D5, _E5, _C5, _A4, _A4, _R,
  _D5, _F5, _A5, _G5, _F5, _E5, _C5, _E5, _D5, _C5, _B4, _B4, _C5, _D5, _E5, _C5, _A4, _A4, _R,

  // part 2
  _E4, _C4, _D4, _B3, _C4, _A3, _GS3, _B3,
  _E4, _C4, _D4, _B3, _C4, _E4, _A4, _A4, _GS4, _R
};

// Optimized 2 : divide memory from "Optimized 1" by 2
// How : 6 possible values 0, 0.5, 1, 1.5, 2, 3 ; so 3 bits
// For each byte : high bits, then low bits.
// 4 high bits : 0 = 0x80 ; 0.5 = 0x90 ; 1 = 0xA0 ; 1.5 = 0xB0 ; 2 = 0xC0 ; 3 = 0xD0
// 4 low  bits : 0 = 0x00 ; 0.5 = 0x01 ; 1 = 0x02 ; 1.5 = 0x03 ; 2 = 0x04 ; 3 = 0x05
//byte lead_symbolic_durations[] = {
// part 1
// To do
// part 2
// To do
//};

// Optimized 1 : Using byte per note to save memory. Divide each value by 2 to get real time.
#define NOTE_DURATION_DIVIDER 2
byte lead_symbolic_durations[] = {
  // part 1
  2, 1, 1, 2, 1, 1, 2, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 2, 2, 2,
  3, 1, 2, 1, 1, 3, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 2, 2, 2,

  // part 2
  4, 4, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 2, 2, 2, 2, 6, 2
};

unsigned short bass_symbolic_freqs[] = {
  // part 1
  _E2, _E3, _E2, _E3, _E2, _E3, _E2, _E3, _A1, _A2, _A1, _A2, _A1, _A2, _A1, _A2, _GS1, _GS2, _GS1, _GS2, _GS1, _GS2, _GS1, _GS2, _A1, _A2, _A1, _A2, _A1, _B2, _C3, _E3,
  _D2, _D3, _D2, _D3, _D2, _D3, _D2, _D3, _C2, _C3, _C2, _C3, _C2, _C3, _C2, _C3, _B1, _B2, _B1, _B2, _B1, _B2, _B1, _B2, _A1, _A2, _A1, _A2, _A1, _A2, _A1, _A2,

  // part 2
  _A1, _E2, _A1, _E2, _A1, _E2, _A1, _E2, _GS1, _E2, _GS1, _E2, _GS1, _E2, _GS1, _E2, _A1, _E2, _A1, _E2, _A1, _E2, _A1, _E2, _GS1, _E2, _GS1, _E2, _GS1, _E2, _GS1, _E2,
  _A1, _E2, _A1, _E2, _A1, _E2, _A1, _E2, _GS1, _E2, _GS1, _E2, _GS1, _E2, _GS1, _E2, _A1, _E2, _A1, _E2, _A1, _E2, _A1, _E2, _GS1, _E2, _GS1, _E2, _GS1, _E2, _GS1, _E2
};


// Since it's always the same value for bass notes, the array of durations is commented to save memory.
//float bass_symbolic_durations[] = { .... 1 ....... }
#define BASS_SYMBOLIC_CONSTANT_DURATION 1


void play_one_note(byte piezoPin, float frequency, unsigned long duration_usec) {
  unsigned long period = 1000000.0 / frequency;

  for (unsigned int cycles = duration_usec / period; cycles > 0; cycles--) {
    // half the time on
    digitalWrite(piezoPin, HIGH);
    delayMicroseconds(period / 2);

    // half the time off
    digitalWrite(piezoPin, LOW);
    delayMicroseconds(period / 2);
  }

  // If the duration wasn't a multiple of the period, delay the remainder
  delayMicroseconds(duration_usec % period);
}

void play_two_notes(byte piezoPin, float freq1, float freq2, unsigned long duration_usec) {
  for (unsigned long t = 0; t < duration_usec; t += 2 * POLY_DELTA) {
    play_one_note(piezoPin, freq1, POLY_DELTA);
    play_one_note(piezoPin, freq2, POLY_DELTA);
  }
}

byte lead_note_count = sizeof(lead_symbolic_freqs) / sizeof(unsigned short);
byte bass_note_count = sizeof(bass_symbolic_freqs) / sizeof(unsigned short);


void playTetrisTheme() {
#ifdef FLAG_DEV_ENABLE_LOGS
  Serial.println("Music");
  Serial.flush();
#endif

  pinMode(OUTPUT_PIN_PWM_PIEZO, OUTPUT);

  byte curr_lead_note_idx = 0, curr_bass_note_idx = 0;
  float lead_freq, bass_freq;

  float lead_symbolic_duration_remaining = lead_symbolic_durations[curr_lead_note_idx];
  float bass_symbolic_duration_remaining = BASS_SYMBOLIC_CONSTANT_DURATION;
  float curr_note_symbolic_duration_remaining;
  unsigned long curr_note_real_duration_usec;

  while (curr_lead_note_idx < lead_note_count && curr_bass_note_idx < bass_note_count) {
    lead_freq = ((float)lead_symbolic_freqs[curr_lead_note_idx]) / HERTZ_DIVIDER;
    bass_freq = ((float)bass_symbolic_freqs[curr_bass_note_idx]) / HERTZ_DIVIDER;

    curr_note_symbolic_duration_remaining = min(lead_symbolic_duration_remaining, bass_symbolic_duration_remaining);
    curr_note_real_duration_usec = (curr_note_symbolic_duration_remaining / (float)NOTE_DURATION_DIVIDER) * 1000000 * (60.0 / BPM);

    if (lead_freq > 0 && bass_freq > 0) {
      play_two_notes(OUTPUT_PIN_PWM_PIEZO, lead_freq, bass_freq, curr_note_real_duration_usec);
    } else if (lead_freq > 0) {
      play_one_note(OUTPUT_PIN_PWM_PIEZO, lead_freq, curr_note_real_duration_usec);
    } else if (bass_freq > 0) {
      play_one_note(OUTPUT_PIN_PWM_PIEZO, bass_freq, curr_note_real_duration_usec);
    } else {
      delay(curr_note_real_duration_usec / 1000);
    }

    // Advance lead note
    lead_symbolic_duration_remaining -= curr_note_symbolic_duration_remaining;
    if (lead_symbolic_duration_remaining < 0.001) {
      curr_lead_note_idx++;
      lead_symbolic_duration_remaining = lead_symbolic_durations[curr_lead_note_idx];
    }

    // Advance bass note
    bass_symbolic_duration_remaining -= curr_note_symbolic_duration_remaining;
    if (bass_symbolic_duration_remaining < 0.001) {
      curr_bass_note_idx++;
      bass_symbolic_duration_remaining = BASS_SYMBOLIC_CONSTANT_DURATION;
    }
  }

  // Apres un play, il reste un bruit de fond. Pour l'enlever :
  // https://forum.arduino.cc/t/piezo-is-making-a-soft-sound-after-having-played-a-tone-how-can-i-stop-it/251765
  pinMode(OUTPUT_PIN_PWM_PIEZO, INPUT);
}

#else

void playTetrisTheme() {
}

#endif  // FLAG_ENABLE_MUSIC


// END MUSIC ***************************************************************************************************************************************
