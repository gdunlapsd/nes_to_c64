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
#define SETTING_SNES_MODE (SETTING_AUTOFIRE_COUNT_MAX + sizeof(SETTING_AUTOFIRE_COUNT_MAX))

#define DEFAULT_AUTOFIRE_COUNT_MAX 30 // 300 millis?
#define AUTOFIRE_COUNT_MIN 15 // 150 millis?


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

#define JOYPORT_0_LED 12
#define JOYPORT_1_LED 13

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

// Logical buttons states for fire buttons 1 and 2
boolean nesFire1 = false;
boolean nesFire2 = false;

// Assign nesFire1 and nesFire2 per SNES button layout if true
boolean snesMode;

boolean fire2Up;
boolean autoFire;

// Variables for pins of currently-selected joystick port
int joyFire1;
int joyFire2;
int joyUp;
int joyDown;
int joyLeft;
int joyRight;

int autoFireCount;
int autoFireCountMax;
int autoFirePress;

int currentJoyPort;

typedef struct {
  boolean *specialButton;
  boolean *comboButton;
  boolean comboPressed = false;
  void (*handler)();
} SettingCombo;

const int NUM_COMBOS = 7;
SettingCombo settingCombos[NUM_COMBOS];

void setupSettingCombos() {
  settingCombos[0].specialButton = &nesStart;
  settingCombos[0].comboButton = &nesUp;
  settingCombos[0].handler = increasFireRate;

  settingCombos[1].specialButton = &nesStart;
  settingCombos[1].comboButton = &nesDown;
  settingCombos[1].handler = decreaseFireRate;

  settingCombos[2].specialButton = &nesStart;
  settingCombos[2].comboButton = &nesFire1;
  settingCombos[2].handler = toggleAutoFire;
  
  settingCombos[3].specialButton = &nesStart;
  settingCombos[3].comboButton = &nesFire2;
  settingCombos[3].handler = toggleFire2IsUp;

  settingCombos[4].specialButton = &nesSelect;
  settingCombos[4].comboButton = &nesLeft;
  settingCombos[4].handler = setJoyPort0;

  settingCombos[5].specialButton = &nesSelect;
  settingCombos[5].comboButton = &nesRight;
  settingCombos[5].handler = setJoyPort1;

  settingCombos[6].specialButton = &nesSelect;
  settingCombos[6].comboButton = &nesStart;
  settingCombos[6].handler = toggleSnesMode;
}

// Initialize joystick pin
// Simulate open collector; will be floating until set to output, which will
// cause it to be held low.
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
    digitalWrite(JOYPORT_0_LED, HIGH);
    digitalWrite(JOYPORT_1_LED, LOW);
  } else {
    joyPort = 1; // Ensure it's normalized as 0 or 1
    joyFire1 = JOY2_FIRE1;
    joyFire2 = JOY2_FIRE2;
    joyUp = JOY2_UP;
    joyDown = JOY2_DOWN;
    joyLeft = JOY2_LEFT;
    joyRight = JOY2_RIGHT;
    digitalWrite(JOYPORT_0_LED, LOW);
    digitalWrite(JOYPORT_1_LED, HIGH);
  }
  // Avoid unnecessary EEPROM writes
  if (currentJoyPort != joyPort) {
    currentJoyPort = joyPort;
    EEPROM.write(SETTING_JOYPORT, joyPort);
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
  
  pinMode(JOYPORT_0_LED, OUTPUT);
  pinMode(JOYPORT_1_LED, OUTPUT);
  
  // Settings from EEPROM
  currentJoyPort = EEPROM.read(SETTING_JOYPORT);
  fire2Up = EEPROM.read(SETTING_FIRE2_UP);
  autoFire = EEPROM.read(SETTING_AUTOFIRE);
  autoFireCountMax = EEPROM.read(SETTING_AUTOFIRE_COUNT_MAX);
  if (autoFireCountMax < AUTOFIRE_COUNT_MIN || autoFireCountMax > DEFAULT_AUTOFIRE_COUNT_MAX) {
    // Correct out of range value
    autoFireCountMax = DEFAULT_AUTOFIRE_COUNT_MAX;
    EEPROM.write(SETTING_AUTOFIRE_COUNT_MAX, DEFAULT_AUTOFIRE_COUNT_MAX);
  }
  snesMode = EEPROM.read(SETTING_SNES_MODE);

  setJoyPort(currentJoyPort);

  autoFireCount = AUTOFIRE_COUNT_MIN;
  autoFirePress = true;

  setupSettingCombos();

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

  // Handle special button combos
  for (int i = 0; i < NUM_COMBOS; i++) {
    SettingCombo *combo = &settingCombos[i];
    if (*(combo->specialButton) == true && (combo->comboButton == NULL || *(combo->comboButton) == true)) {
      combo->comboPressed = true;
      return;
    } else if (combo->comboPressed) {
      combo->comboPressed = false;
      combo->handler();
    }
  }
  
  if (autoFire) {
    if (nesFire1) {
      if (autoFireCount <= AUTOFIRE_COUNT_MIN) {
        autoFireCount = autoFireCountMax; // Restart the countdown
        joyState(joyFire1, autoFirePress);
        autoFirePress = !autoFirePress; // Toggle firing status
      }
      autoFireCount--;
    } else {
      autoFireCount = AUTOFIRE_COUNT_MIN;
      autoFirePress = true; // Enable fire pin on next NES fire button press
      joyRelease(joyFire1);
    }
  } else {
    joyState(joyFire1, nesFire1);
  }
  
  if (fire2Up) {
    joyRelease(joyFire2);
    joyState(joyUp, nesUp || nesFire2);
  } else {
    joyState(joyFire2, nesFire2);
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
  // Pin held low
  pinMode(pin, OUTPUT);
}

void joyRelease(int pin) {
  // Pin floating
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

  if (snesMode) {
    nesFire1 = snesB;
    nesFire2 = snesA;
  } else {
    nesFire1 = nesB;
    nesFire2 = nesA;
  }

  delay(10);
}

boolean nesRead() {
  // 0 means button pressed
  boolean status = !digitalRead(NES_DATA);
  nesClock();
  return status;
}

// Settings handlers

void toggleFire2IsUp() {
  fire2Up = !fire2Up;
  EEPROM.write(SETTING_FIRE2_UP, fire2Up);
}

void toggleAutoFire() {
  autoFire = !autoFire;
  EEPROM.write(SETTING_AUTOFIRE, autoFire);
  autoFireCount = AUTOFIRE_COUNT_MIN;
  autoFirePress = true;
  joyRelease(joyFire1);
}

void increasFireRate() {
  if (autoFireCountMax > AUTOFIRE_COUNT_MIN) {
    autoFireCountMax-=3;
    // Extra range checking to facilitate playing with limit values
    if (autoFireCountMax < AUTOFIRE_COUNT_MIN) {
      autoFireCountMax = AUTOFIRE_COUNT_MIN;
    }
    EEPROM.write(SETTING_AUTOFIRE_COUNT_MAX, autoFireCountMax);
  }
}

void decreaseFireRate() {
  if (autoFireCountMax < DEFAULT_AUTOFIRE_COUNT_MAX) {
    autoFireCountMax+=3;
    // Extra range checking to facilitate playing with limit values
    if (autoFireCountMax > DEFAULT_AUTOFIRE_COUNT_MAX) {
      autoFireCountMax = DEFAULT_AUTOFIRE_COUNT_MAX;
    }
    EEPROM.write(SETTING_AUTOFIRE_COUNT_MAX, autoFireCountMax);
  }
}

void setJoyPort0() {
  setJoyPort(0);
}
void setJoyPort1() {
  setJoyPort(1);
}

void toggleSnesMode() {
  snesMode = !snesMode;  
}
