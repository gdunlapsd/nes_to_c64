#include <EEPROM.h>

/**
 * This interfaces an NES or SNES controller to Atari- digital joystick ports.
 * 
 * Two joystick ports, autofire, and second button as UP are supported.
 * 
 * SELECT switches between joystick ports.
 * START + Fire (NES button B, SNES button Y) toggles autofire on/off
 * START + UP increases autofire rate
 * START + DOWN descreases autofire rate
 * START + Fire 2 (NES button A, SNES button X or B) toggles Fire 2 as UP
 * 
 * Settings are saved in EEPROM.
 * 
 */
 
// Define persistant setting addresses
#define SETTING_JOYPORT 0
#define SETTING_FIRE2_UP sizeof(SETTING_JOYPORT)
#define SETTING_AUTOFIRE (SETTING_FIRE2_UP + sizeof(SETTING_FIRE2_UP))
#define SETTING_AUTOFIRE_COUNT_MAX (SETTING_AUTOFIRE + sizeof(SETTING_AUTOFIRE))

#define DEFAULT_AUTOFIRE_COUNT_MAX 30
#define AUTOFIRE_COUNT_MIN 15


// Define pins used
#define JOY1_UP 0
#define JOY1_DOWN 1
#define JOY1_LEFT 2
#define JOY1_RIGHT 3
#define JOY1_FIRE1 4
#define JOY1_FIRE2 5

#define JOY2_UP 6
#define JOY2_DOWN 7
#define JOY2_LEFT 8
#define JOY2_RIGHT 9
#define JOY2_FIRE1 10
#define JOY2_FIRE2 11

#define NES_CLOCK A0
#define NES_LATCH A1
#define NES_DATA A2

// Variables for controller button states
boolean nesA = false;
boolean nesB = false;
boolean nesSelect = false;
boolean nesStart = false;
boolean nesUp = false;
boolean nesDown = false;
boolean nesLeft = false;
boolean nesRight = false;
boolean snesA = false;
boolean snesB = false;
boolean snesX = false;
boolean snesY = false;
boolean snesL = false;
boolean snesR = false;

// Debounce setting toggles
boolean selectPressed = false;
boolean startFire1Pressed = false;
boolean startFire2Pressed = false;
boolean startUpPressed = false;
boolean startDownPressed = false;

boolean fire2Up;
boolean autoFire;

// Variables for pins of currently-selected joystick port
int joyFire1;
int joyFire2;
int joyUp;
int joyDown;
int joyLeft;
int joyRight;

// Current joystick port (0 or 1)
int currentJoyPort;

int autoFireCount;
int autoFireCountMax;
int autoFirePress;

// Initialize joystick pin
void initJoyPin(int pin) {
  pinMode(pin, INPUT);
  digitalWrite(pin, LOW);
}

// Set current joystick port
void setJoyPort(int joyPort) {
  if (joyPort == 0) {
    joyFire1 = JOY1_FIRE1;
    joyFire2 = JOY1_FIRE2;
    joyUp = JOY1_UP;
    joyDown = JOY1_DOWN;
    joyLeft = JOY1_LEFT;
    joyRight = JOY1_RIGHT;
  } else {
    joyPort = 1; // Ensure it's normalized as 0 or 1
    joyFire1 = JOY2_FIRE1;
    joyFire2 = JOY2_FIRE2;
    joyUp = JOY2_UP;
    joyDown = JOY2_DOWN;
    joyLeft = JOY2_LEFT;
    joyRight = JOY2_RIGHT;
  }
  if (currentJoyPort != joyPort) {
    currentJoyPort = joyPort;
    EEPROM.write(SETTING_JOYPORT, currentJoyPort);
  }
}

void setup() {
  initJoyPin(JOY1_UP);
  initJoyPin(JOY1_DOWN);
  initJoyPin(JOY1_LEFT);
  initJoyPin(JOY1_RIGHT);
  initJoyPin(JOY1_FIRE1);
  initJoyPin(JOY1_FIRE2);

  initJoyPin(JOY2_UP);
  initJoyPin(JOY2_DOWN);
  initJoyPin(JOY2_LEFT);
  initJoyPin(JOY2_RIGHT);
  initJoyPin(JOY2_FIRE1);
  initJoyPin(JOY2_FIRE2);

  // Settings from EEPROM
  setJoyPort(EEPROM.read(SETTING_JOYPORT));
  fire2Up = EEPROM.read(SETTING_FIRE2_UP);
  autoFire = EEPROM.read(SETTING_AUTOFIRE);
  autoFireCountMax = EEPROM.read(SETTING_AUTOFIRE_COUNT_MAX);
  if (autoFireCountMax < AUTOFIRE_COUNT_MIN || autoFireCountMax > DEFAULT_AUTOFIRE_COUNT_MAX) {
    autoFireCountMax = DEFAULT_AUTOFIRE_COUNT_MAX;
    EEPROM.write(SETTING_AUTOFIRE_COUNT_MAX, DEFAULT_AUTOFIRE_COUNT_MAX);
  }
  
  autoFireCount = AUTOFIRE_COUNT_MIN;
  autoFirePress = true;
  
  // Initialize NES/SNES pins
  pinMode(NES_DATA, INPUT);
  digitalWrite(NES_DATA, HIGH);
  pinMode(NES_LATCH, OUTPUT);
  digitalWrite(NES_LATCH, LOW);
  pinMode(NES_CLOCK, OUTPUT);
  digitalWrite(NES_CLOCK, LOW);
  delay(100);
}

void loop() {
  nesReadButtons();

  // Use SELECT to switch joystick ports
  if (nesSelect) {
    selectPressed = true;
    return;
  } else if (selectPressed) {
    selectPressed = false;
    setJoyPort(!currentJoyPort);
  }

  if (nesStart && (nesA || snesX)) {
    startFire2Pressed = true;
    return;
  } else if (startFire2Pressed) {
    startFire2Pressed = false;
    fire2Up = !fire2Up;
    EEPROM.write(SETTING_FIRE2_UP, fire2Up);
  }

  if (nesStart && nesB) {
    startFire1Pressed = true;
    return;
  } else if (startFire1Pressed) {
    startFire1Pressed = false;
    autoFire = !autoFire;
    EEPROM.write(SETTING_AUTOFIRE, autoFire);
    autoFireCount = AUTOFIRE_COUNT_MIN;
    autoFirePress = true;
    joyRelease(joyFire1);
  }

  // Increase autofire rate by decreasing countdown
  if (nesStart && nesUp) {
    startUpPressed = true;
    return;
  } else if (startUpPressed) {
    startUpPressed = false;
    if (autoFireCountMax > AUTOFIRE_COUNT_MIN) {
      autoFireCountMax-=3;
      // Extra range checking to facilitate playing with limit values
      if (autoFireCountMax < AUTOFIRE_COUNT_MIN) {
        autoFireCountMax = AUTOFIRE_COUNT_MIN;
      }
      EEPROM.write(SETTING_AUTOFIRE_COUNT_MAX, autoFireCountMax);
    }
  }

  // descrease autofire rate by increasing countdown
  if (nesStart && nesDown) {
    startDownPressed = true;
    return;
  } else if (startDownPressed) {
    startDownPressed = false;
    if (autoFireCountMax < DEFAULT_AUTOFIRE_COUNT_MAX) {
      autoFireCountMax+=3;
      // Extra range checking to facilitate playing with limit values
      if (autoFireCountMax > DEFAULT_AUTOFIRE_COUNT_MAX) {
        autoFireCountMax = DEFAULT_AUTOFIRE_COUNT_MAX;
      }
      EEPROM.write(SETTING_AUTOFIRE_COUNT_MAX, autoFireCountMax);
    }
  }
  
  if (autoFire) {
    if (nesB) {
      if (autoFireCount <= AUTOFIRE_COUNT_MIN) {
        autoFireCount = autoFireCountMax; // Restart the countdown
        joyState(joyFire1, autoFirePress);
        autoFirePress = !autoFirePress; // Toggle fire pin
      }
      autoFireCount--;
    } else {
      autoFireCount = AUTOFIRE_COUNT_MIN;
      autoFirePress = true; // Enable fire pin on next NES fire button press
      joyRelease(joyFire1);
    }
  } else {
    joyState(joyFire1, nesB);
  }
  
  if (fire2Up) {
    joyRelease(joyFire2);
    joyState(joyUp, nesUp || nesA || snesX);
  } else {
    joyState(joyFire2, nesA || snesX);
    joyState(joyUp, nesUp);
  }
  
  joyState(joyDown, nesDown);
  joyState(joyLeft, nesLeft || snesL);
  joyState(joyRight, nesRight || snesR);
}

void joyState(int pin, boolean state) {
  if (state) {
    joyPress(pin);
  } else {
    joyRelease(pin);
  }
}

void joyPress(int pin) {
  pinMode(pin, OUTPUT);
}

void joyRelease(int pin) {
  pinMode(pin, INPUT);
}

void nesClock() {
  digitalWrite(NES_CLOCK, HIGH);
  delayMicroseconds(10);
  digitalWrite(NES_CLOCK, LOW);
  delayMicroseconds(10);
}

void nesReadButtons() {
  // Tell controller to start button read from beginning
  digitalWrite(NES_LATCH, HIGH);
  delayMicroseconds(30);
  digitalWrite(NES_LATCH, LOW);
  delayMicroseconds(10);
  
  // Button order on NES is A B Select Start Up Down Left Right
  // Button order on SNES is B Y Select Start Up Down Left Right A X L R
  
  nesA = snesB = nesRead();
  nesB = snesY = nesRead();
  nesSelect = nesRead();
  nesStart = nesRead();
  nesUp = nesRead();
  nesDown = nesRead();
  nesLeft = nesRead();
  nesRight = nesRead();
  snesA = nesRead();
  snesX = nesRead();
  snesL = nesRead();
  snesR = nesRead();

  delay(10);
}

boolean nesRead() {
  // 0 means button pressed
  boolean status = !digitalRead(NES_DATA);
  nesClock();
  return status;
}
