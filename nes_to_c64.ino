#include <EEPROM.h>

/**
 * This interfaces an NES or SNES controller to Atari-style digital joystick ports.
 * 
 * The type of controller (NES or SNES) is autodetected; tested with genuine Nintendo
 * NES, SNES, and NES Advantage controllers.
 * Third party controllers have not been tested; YMMV.
 * 
 * Two joystick ports, autofire, and second fire button as UP are supported.
 * 
 * SELECT + LEFT switches to the first joystick port.
 * SELECT + RIGHT switches to the second joystick port.
 * START + a Fire button toggles autofire on/off for that button.
 * START + UP increases autofire rate.
 * START + DOWN descreases autofire rate.
 * SELECT + UP toggles Fire 2 as UP; when in that mode, autofire is disabled for that button.
 * SELETE + a FIRE button (A or B) will make that the primary fire button and the other one the secondary.
 * 
 * Settings are saved in EEPROM.
 * 
 */
 
// Define persistant setting addresses
#define SETTING_JOYPORT 0
#define SETTING_FIRE2_UP sizeof(SETTING_JOYPORT)
#define SETTING_AUTOFIRE1 (SETTING_FIRE2_UP + sizeof(SETTING_FIRE2_UP))
#define SETTING_AUTOFIRE2 (SETTING_AUTOFIRE1 + sizeof(SETTING_AUTOFIRE1))
#define SETTING_AUTOFIRE_RATE_MILLIS_MAX (SETTING_AUTOFIRE2 + sizeof(SETTING_AUTOFIRE2))
#define SETTING_FIRE_REVERSED (SETTING_AUTOFIRE_RATE_MILLIS_MAX + sizeof(SETTING_AUTOFIRE_RATE_MILLIS_MAX))

#define AUTOFIRE_RATE_MILLIS_MAX 150
#define AUTOFIRE_RATE_MILLIS_MIN 50
#define AUTOFIRE_RATE_ADJUST_DELTA 10
#define BLINK_MILLIS 50

// Define pins used
#define JOY1_UP 2
#define JOY1_DOWN 3
#define JOY1_LEFT 4
#define JOY1_RIGHT 5
#define JOY1_FIRE1 6
#define JOY1_FIRE2 7

#define JOY2_UP 8
#define JOY2_DOWN 9
#define JOY2_LEFT 10
#define JOY2_RIGHT 11
#define JOY2_FIRE1 12
#define JOY2_FIRE2 13

#define JOYPORT_0_LED A4
#define JOYPORT_1_LED A5

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

// Assign nesFire1 and nesFire2 per SNES button layout if true. Will be autodetected.
boolean snesMode = false;

// Fire buttons are reversed from default
boolean fireReversed;

// Variables for pins of currently-selected joystick port
int joyFire1;
int joyFire2;
int joyUp;
int joyDown;
int joyLeft;
int joyRight;

unsigned long autoFireRateMillis;

int currentJoyPort;
int currentJoyLED;

unsigned long blinkMillis = 0;

unsigned long currentMillis = 0;


typedef struct {
  boolean *specialButton;
  boolean *comboButton;
  boolean comboPressed = false;
  void (*handler)();
} SettingCombo;

const int NUM_COMBOS = 8;
SettingCombo settingCombos[NUM_COMBOS];

typedef struct {
  boolean *nesFireButton;
  int *joyFire;
  boolean fireIsUp;
  boolean autoFire;       // Current autofire setting
  boolean autoFirePress;
  unsigned long autoFireStateMillis;
  int autoFireSetting;    // EEPROM setting address
  boolean blinked = false;
} FireButton;

const int NUM_FIRE_BUTTONS = 2;
FireButton fireButtons[NUM_FIRE_BUTTONS];


void setupSettingCombos() {
  settingCombos[0].specialButton = &nesStart;
  settingCombos[0].comboButton = &nesUp;
  settingCombos[0].handler = increasFireRate;

  settingCombos[1].specialButton = &nesStart;
  settingCombos[1].comboButton = &nesDown;
  settingCombos[1].handler = decreaseFireRate;

  settingCombos[2].specialButton = &nesStart;
  settingCombos[2].comboButton = &nesFire1;
  settingCombos[2].handler = toggleAutoFire1;

  settingCombos[3].specialButton = &nesStart;
  settingCombos[3].comboButton = &nesFire2;
  settingCombos[3].handler = toggleAutoFire2;
  
  settingCombos[4].specialButton = &nesSelect;
  settingCombos[4].comboButton = &nesUp;
  settingCombos[4].handler = toggleFire2IsUp;

  settingCombos[5].specialButton = &nesSelect;
  settingCombos[5].comboButton = &nesLeft;
  settingCombos[5].handler = setJoyPort0;

  settingCombos[6].specialButton = &nesSelect;
  settingCombos[6].comboButton = &nesRight;
  settingCombos[6].handler = setJoyPort1;

  settingCombos[7].specialButton = &nesSelect;
  settingCombos[7].comboButton = &nesFire2;
  settingCombos[7].handler = toggleFireReversed;
}


void setupFireButtons() {
  fireButtons[0].autoFireSetting = SETTING_AUTOFIRE1;
  fireButtons[0].nesFireButton = &nesFire1;
  fireButtons[0].joyFire = &joyFire1;
  fireButtons[0].autoFire = EEPROM.read(fireButtons[0].autoFireSetting);
  fireButtons[0].autoFireStateMillis = 0;
  fireButtons[0].fireIsUp = false;

  fireButtons[1].autoFireSetting = SETTING_AUTOFIRE2;
  fireButtons[1].nesFireButton = &nesFire2;
  fireButtons[1].joyFire = &joyFire2;
  fireButtons[1].autoFire = EEPROM.read(fireButtons[1].autoFireSetting);
  fireButtons[1].autoFireStateMillis = 0;
  fireButtons[1].fireIsUp = EEPROM.read(SETTING_FIRE2_UP);
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
    currentJoyLED = JOYPORT_0_LED;
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
    currentJoyLED = JOYPORT_1_LED;
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
  fireReversed = EEPROM.read(SETTING_FIRE_REVERSED);

  autoFireRateMillis = EEPROM.read(SETTING_AUTOFIRE_RATE_MILLIS_MAX);
  if (autoFireRateMillis < AUTOFIRE_RATE_MILLIS_MIN) {
    autoFireRateMillis = AUTOFIRE_RATE_MILLIS_MIN;
    EEPROM.write(SETTING_AUTOFIRE_RATE_MILLIS_MAX, autoFireRateMillis);
  }  
  if (autoFireRateMillis > AUTOFIRE_RATE_MILLIS_MAX) {
    autoFireRateMillis = AUTOFIRE_RATE_MILLIS_MAX;
    EEPROM.write(SETTING_AUTOFIRE_RATE_MILLIS_MAX, autoFireRateMillis);
  }
  
  setJoyPort(currentJoyPort);

  // Initialize NES/SNES pins
  pinMode(NES_DATA, INPUT);
  digitalWrite(NES_DATA, HIGH);
  pinMode(NES_LATCH, OUTPUT);
  digitalWrite(NES_LATCH, LOW);
  pinMode(NES_CLOCK, OUTPUT);
  digitalWrite(NES_CLOCK, LOW);
  delay(100);

  setupSettingCombos();
  setupFireButtons();
}

void loop() {
  currentMillis = millis();

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

  for (int i=0; i < NUM_FIRE_BUTTONS; i++) {
    FireButton *fireButton = &fireButtons[i];
    if (!fireButton->fireIsUp) {
      if (fireButton->autoFire && *(fireButton->nesFireButton)) {
        if (fireButton->autoFireStateMillis == 0) {
          // Autofire just starting
          fireButton->autoFireStateMillis = currentMillis;
          fireButton->autoFirePress = true;
          joyFire(fireButton, true);
        } else if ((currentMillis - fireButton->autoFireStateMillis) >= autoFireRateMillis) {
          fireButton->autoFireStateMillis = currentMillis;
          fireButton->autoFirePress = !fireButton->autoFirePress; // Toggle firing status
          joyFire(fireButton, fireButton->autoFirePress);
        }
      } else {
        fireButton->autoFireStateMillis = 0;
        joyFire(fireButton, *(fireButton->nesFireButton));
      }
    }
  }
  
  if (fireButtons[1].fireIsUp) {
    joyRelease(joyFire2);
    joyState(joyUp, nesUp || nesFire2);
  } else {
    joyState(joyUp, nesUp);
  }
  
  joyState(joyDown, nesDown);
  joyState(joyLeft, nesLeft || snesL);
  joyState(joyRight, nesRight || snesR);

  endJoyBlink();
}

void startJoyBlink() {
  if (blinkMillis == 0) {
    digitalWrite(currentJoyLED, LOW);
    blinkMillis = currentMillis;
  }
}
void endJoyBlink() {
  if (blinkMillis > 0 && (currentMillis - blinkMillis >= BLINK_MILLIS)) {
    blinkMillis = 0;
    digitalWrite(currentJoyLED, HIGH);
  }
}

// Blink the LED when firing
void joyFire(FireButton *fireButton, boolean state) {
  joyState(*(fireButton->joyFire), state);
  if (state) {
    if (!fireButton->blinked) {
      fireButton->blinked = true;
      startJoyBlink();
    }
  } else {
    fireButton->blinked = false;
  }
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

  // NES controller will read these as all true
  snesMode = !(snesA && snesX && snesL && snesR);
  
  if (snesMode) {
    if (fireReversed) {
      nesFire1 = snesA;
      nesFire2 = snesB;
    } else {
      nesFire1 = snesB;
      nesFire2 = snesA;
    }
  } else {
    snesA = snesX = snesL = snesR = false;
    if (fireReversed) {
      nesFire1 = nesA;
      nesFire2 = nesB;
    } else {
      nesFire1 = nesB;
      nesFire2 = nesA;      
    }
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
  fireButtons[1].fireIsUp = !fireButtons[1].fireIsUp;
  EEPROM.write(SETTING_FIRE2_UP, fireButtons[1].fireIsUp);
}

void toggleAutoFire1() {
  toggleAutoFire(0);
}
void toggleAutoFire2() {
  toggleAutoFire(1);
}
void toggleAutoFire(int fireButton) {
  fireButtons[fireButton].autoFire = !fireButtons[fireButton].autoFire;
  EEPROM.write(fireButtons[fireButton].autoFireSetting, fireButtons[fireButton].autoFire);
  fireButtons[fireButton].autoFireStateMillis = 0;
  joyRelease(*(fireButtons[fireButton].joyFire));
}

void increasFireRate() {
  if (autoFireRateMillis > AUTOFIRE_RATE_MILLIS_MIN) {
    autoFireRateMillis-=AUTOFIRE_RATE_ADJUST_DELTA;
    if (autoFireRateMillis < AUTOFIRE_RATE_MILLIS_MIN) {
      autoFireRateMillis = AUTOFIRE_RATE_MILLIS_MIN;
    }
    EEPROM.write(SETTING_AUTOFIRE_RATE_MILLIS_MAX, autoFireRateMillis);
  }
}

void decreaseFireRate() {
  if (autoFireRateMillis < AUTOFIRE_RATE_MILLIS_MAX) {
    autoFireRateMillis+=AUTOFIRE_RATE_ADJUST_DELTA;
    if (autoFireRateMillis > AUTOFIRE_RATE_MILLIS_MAX) {
      autoFireRateMillis = AUTOFIRE_RATE_MILLIS_MAX;
    }
    EEPROM.write(SETTING_AUTOFIRE_RATE_MILLIS_MAX, autoFireRateMillis);
  }
}

void setJoyPort0() {
  setJoyPort(0);
}
void setJoyPort1() {
  setJoyPort(1);
}

void toggleFireReversed() {
  fireReversed = !fireReversed;
  EEPROM.write(SETTING_FIRE_REVERSED, fireReversed);
}
