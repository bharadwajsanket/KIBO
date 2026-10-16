// Kibo firmware for ESP32-C3 Super Mini with SSD1306 OLED and TTP223 touch input.

#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "time.h"
#include <math.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include "qrcode.h"

struct SavedWifi {
  String ssid;
  String pass;
};

// 1. HARDWARE PINS & CONSTANTS
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

#define SDA_PIN_DEFAULT 0
#define SCL_PIN_DEFAULT 1
#define SDA_PIN_ALT     8
#define SCL_PIN_ALT     9

#define TOUCH_PIN 4
#define OLED_RESET -1
#define OLED_ADDR  0x3C
#define TOUCH_ACTIVE_HIGH 1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define KIBO_DEBUG 0
#if KIBO_DEBUG
  #define DBG(...) Serial.printf(__VA_ARGS__)
#else
  #define DBG(...) do {} while (0)
#endif

const unsigned long TOUCH_DEBOUNCE_MS          = 35;
const unsigned long TAP_WINDOW_MS              = 350;    // window to accumulate multi-taps
const unsigned long SHORT_PRESS_MAX_MS         = 500;
const unsigned long PETTING_HOLD_MS            = 600;    // 600ms+ hold = petting squish
const unsigned long WORLD_CLOCK_LONGPRESS_MS   = 800;    // 800ms+ hold = switch city / world clock
const unsigned long RAPID_TAP_WINDOW_MS        = 1500;
const int           RAPID_TAP_BURST_THRESHOLD  = 4;
const unsigned long ANGER_COOLDOWN_MS          = 8000;
const unsigned long DOZE_IDLE_MS               = 60000;
const unsigned long DOZE_TO_SLEEP_MS           = 120000;
const unsigned long TEASE_FAINT_RECOVERY_MS    = 18000;
const unsigned long SCREENOFF_NONFACE_MS       = 1800000;
const unsigned long GROGGY_DURATION_MS         = 3000;
const unsigned long YAWN_DURATION_MS           = 1500;
const unsigned long YAWN_COOLDOWN_MS           = 300000;
const unsigned long BIRTHDAY_COOLDOWN_MS       = 1800000;
const unsigned long MEMORY_SAVE_INTERVAL_MS    = 300000;
const unsigned long WEATHER_REFRESH_MS         = 600000;
const unsigned long WEATHER_HTTP_TIMEOUT_MS    = 3500;
const unsigned long SLEEP_ENTRY_GUARD_MS       = 800;
const unsigned long BOOT_TOUCH_BLACKOUT_MS     = 500;
const unsigned long HEAP_LOG_INTERVAL_MS       = 300000;
const unsigned long SAD_STREAK_TRIGGER_MS      = 86400000;
const unsigned long SAD_STREAK_DURATION_MS     = 7200000;
const unsigned long LIGHTNING_FLASH_MS         = 80;
const unsigned long GESTURE_ROLL_MIN_MS        = 15000;
const unsigned long GESTURE_ROLL_MAX_MS        = 45000;

// 2. BITMAP ASSETS
const unsigned char bmp_clear[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x00,0x01,0x80,0x00,0x00,0x01,0x80,0x00,0x00,0x01,0x80,0x00,
  0x00,0x00,0x00,0x00,0x01,0x03,0xc0,0x80,0x00,0x0f,0xf0,0x00,0x00,0x3f,0xfc,0x00,
  0x00,0x7f,0xfe,0x00,0x00,0xff,0xff,0x00,0x06,0xff,0xff,0x60,0x06,0xff,0xff,0x60,
  0x06,0xff,0xff,0x60,0x00,0xff,0xff,0x00,0x3e,0xff,0xff,0x7c,0x3e,0xff,0xff,0x7c,
  0x3e,0xff,0xff,0x7c,0x00,0xff,0xff,0x00,0x06,0xff,0xff,0x60,0x06,0xff,0xff,0x60,
  0x06,0xff,0xff,0x60,0x00,0xff,0xff,0x00,0x00,0x7f,0xfe,0x00,0x00,0x3f,0xfc,0x00,
  0x01,0x0f,0xf0,0x80,0x00,0x03,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x80,0x00,
  0x00,0x01,0x80,0x00,0x00,0x01,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
const unsigned char bmp_clouds[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0xe0,0x00,
  0x00,0x0f,0xf8,0x00,0x00,0x1f,0xfc,0x00,0x00,0x3f,0xfe,0x00,0x00,0x3f,0xff,0x00,
  0x00,0x7f,0xff,0x80,0x00,0xff,0xff,0xc0,0x00,0xff,0xff,0xe0,0x01,0xff,0xff,0xf0,
  0x03,0xff,0xff,0xf8,0x07,0xff,0xff,0xfc,0x07,0xff,0xff,0xfc,0x0f,0xff,0xff,0xfe,
  0x0f,0xff,0xff,0xfe,0x1f,0xff,0xff,0xff,0x1f,0xff,0xff,0xff,0x1f,0xff,0xff,0xff,
  0x1f,0xff,0xff,0xff,0x1f,0xff,0xff,0xff,0x1f,0xff,0xff,0xff,0x0f,0xff,0xff,0xfe,
  0x07,0xff,0xff,0xfc,0x03,0xff,0xff,0xf8,0x00,0xff,0xff,0xe0,0x00,0x3f,0xff,0x80,
  0x00,0x0f,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
const unsigned char bmp_rain[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x00,0x03,0xe0,0x00,0x00,0x0f,0xf8,0x00,0x00,0x1f,0xfc,0x00,
  0x00,0x3f,0xfe,0x00,0x00,0x7f,0xff,0x80,0x00,0xff,0xff,0xc0,0x01,0xff,0xff,0xf0,
  0x03,0xff,0xff,0xf8,0x07,0xff,0xff,0xfc,0x0f,0xff,0xff,0xfe,0x1f,0xff,0xff,0xff,
  0x1f,0xff,0xff,0xff,0x1f,0xff,0xff,0xff,0x1f,0xff,0xff,0xff,0x0f,0xff,0xff,0xfe,
  0x07,0xff,0xff,0xfc,0x03,0xff,0xff,0xf8,0x00,0xff,0xff,0xe0,0x00,0x3f,0xff,0x80,
  0x00,0x0f,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x60,0x0c,0x00,0x00,0x60,0x0c,0x00,
  0x00,0xe0,0x1c,0x00,0x00,0xc0,0x18,0x00,0x03,0x80,0x70,0x00,0x03,0x80,0x70,0x00,
  0x03,0x00,0x60,0x00,0x02,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
const unsigned char mini_sun[] PROGMEM = {
  0x00,0x00,0x01,0x80,0x00,0x00,0x10,0x08,0x04,0x20,0x03,0xc0,0x27,0xe4,0x07,0xe0,
  0x07,0xe0,0x27,0xe4,0x03,0xc0,0x04,0x20,0x10,0x08,0x00,0x00,0x01,0x80,0x00,0x00
};
const unsigned char mini_cloud[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x01,0xc0,0x07,0xe0,0x0f,0xf0,0x1f,0xf8,0x1f,0xf8,0x3f,0xfc,
  0x3f,0xfc,0x7f,0xfe,0x3f,0xfe,0x1f,0xfc,0x0f,0xf0,0x00,0x00,0x00,0x00,0x00,0x00
};
const unsigned char mini_rain[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x01,0xc0,0x07,0xe0,0x0f,0xf0,0x1f,0xf8,0x1f,0xf8,0x3f,0xfc,
  0x3f,0xfc,0x7f,0xfe,0x3f,0xfe,0x1f,0xfc,0x00,0x00,0x44,0x44,0x22,0x22,0x11,0x11
};
const unsigned char bmp_tiny_drop[] PROGMEM = { 0x10,0x38,0x7c,0xfe,0xfe,0x7c,0x38,0x00 };
const unsigned char bmp_heart_small[] PROGMEM = { 0x66,0xff,0xff,0xff,0x7e,0x3c,0x18,0x00 };
const unsigned char bmp_heart_medium[] PROGMEM = {
  0x00,0x00,0x1e,0x78,0x3f,0xfc,0x7f,0xfe,0x7f,0xfe,0x7f,0xfe,
  0x3f,0xfc,0x1f,0xf8,0x0f,0xf0,0x07,0xe0,0x03,0xc0,0x01,0x80
};
const unsigned char bmp_zzz[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3c,0x00,0x0c,0x00,0x18,0x00,0x30,0x00,0x7e,
  0x00,0x00,0x3c,0x00,0x0c,0x00,0x18,0x00,0x30,0x00,0x7c,0x00,0x00,0x00,0x00,0x00
};
const unsigned char bmp_anger[] PROGMEM = {
  0x00,0x00,0x11,0x10,0x2a,0x90,0x44,0x40,0x80,0x20,0x80,0x20,0x44,0x40,0x2a,0x90,
  0x11,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
const unsigned char bmp_sweat[] PROGMEM = { 0x04,0x0c,0x1e,0x3f,0x3f,0x1e,0x0c,0x00 };

// 3. BEHAVIOUR STATE
enum BehaviorState : uint8_t {
  BEHAVIOR_AWAKE, BEHAVIOR_CURIOUS, BEHAVIOR_HAPPY, BEHAVIOR_CONTENT,
  BEHAVIOR_PLAYFUL, BEHAVIOR_SURPRISED, BEHAVIOR_THINKING, BEHAVIOR_BORED,
  BEHAVIOR_SLEEPY, BEHAVIOR_SLEEPING, BEHAVIOR_WAKING, BEHAVIOR_GROGGY,
  BEHAVIOR_DIZZY, BEHAVIOR_ANNOYED, BEHAVIOR_SAD, BEHAVIOR_CRYING,
  BEHAVIOR_RECOVERING, BEHAVIOR_CELEBRATING, BEHAVIOR_YAWNING,
  BEHAVIOR_EXCITED, BEHAVIOR_SHY, BEHAVIOR_LOW_ENERGY, BEHAVIOR_OVER_EXCITED,
  BEHAVIOR_FED_UP, BEHAVIOR_ANGRY, BEHAVIOR_IGNORING, BEHAVIOR_DONT_CARE
};

enum FaceGesture : uint8_t {
  FG_NONE, FG_STRETCH, FG_YAWN, FG_LOOK_AROUND, FG_DOZE_DIP, FG_SHIVER,
  FG_SQUINT_UV, FG_HAPPY_SQUINT, FG_WORRIED_LOOKUP, FG_FEAR_SQUEEZE,
  FG_CONTENT_WIGGLE, FG_MICRO_FIDGET, FG_SLOW_BLINK, FG_SIDE_EYE,
  FG_CONFUSED, FG_WINK, FG_LAUGH, FG_HEART_EYES, FG_CURIOUS_PEEK, FG_SHOCK
};

static int behaviorPriority(BehaviorState s) {
  switch (s) {
    case BEHAVIOR_SLEEPING:     return 100;
    case BEHAVIOR_WAKING:       return 95;
    case BEHAVIOR_DIZZY:        return 90;
    case BEHAVIOR_ANGRY:        return 89;
    case BEHAVIOR_ANNOYED:      return 88;
    case BEHAVIOR_CRYING:       return 85;
    case BEHAVIOR_CELEBRATING:  return 80;
    case BEHAVIOR_CONTENT:      return 75;
    case BEHAVIOR_OVER_EXCITED: return 72;
    case BEHAVIOR_YAWNING:      return 70;
    case BEHAVIOR_SURPRISED:    return 65;
    case BEHAVIOR_EXCITED:      return 62;
    case BEHAVIOR_PLAYFUL:      return 60;
    case BEHAVIOR_HAPPY:        return 55;
    case BEHAVIOR_FED_UP:       return 52;
    case BEHAVIOR_IGNORING:     return 51;
    case BEHAVIOR_RECOVERING:   return 50;
    case BEHAVIOR_DONT_CARE:    return 48;
    case BEHAVIOR_THINKING:     return 45;
    case BEHAVIOR_SHY:          return 42;
    case BEHAVIOR_CURIOUS:      return 40;
    case BEHAVIOR_GROGGY:       return 35;
    case BEHAVIOR_BORED:        return 25;
    case BEHAVIOR_LOW_ENERGY:   return 22;
    case BEHAVIOR_SLEEPY:       return 20;
    case BEHAVIOR_SAD:          return 15;
    case BEHAVIOR_AWAKE:        default: return 10;
  }
}
static bool isTemporaryBehavior(BehaviorState s) {
  return s != BEHAVIOR_AWAKE && s != BEHAVIOR_SLEEPING;
}

// 4. EYE ENGINE
struct Eye {
  float x, y, w, h, targetX, targetY, targetW, targetH;
  float pupilX, pupilY, targetPupilX, targetPupilY;
  float velX, velY, velW, velH, pVelX, pVelY;
  float k = 0.12f, d = 0.60f, pk = 0.08f, pd = 0.50f;
  bool blinking = false;
  unsigned long lastBlink = 0, nextBlinkTime = 0;
  void init(float _x, float _y, float _w, float _h) {
    x = targetX = _x; y = targetY = _y; w = targetW = _w; h = targetH = _h;
    pupilX = targetPupilX = 0; pupilY = targetPupilY = 0;
    velX = velY = velW = velH = 0; pVelX = pVelY = 0;
    nextBlinkTime = millis() + random(1800, 4800);
  }
  void update() {
    float ax = (targetX - x) * k, ay = (targetY - y) * k;
    float aw = (targetW - w) * k, ah = (targetH - h) * k;
    velX = (velX + ax) * d; velY = (velY + ay) * d;
    velW = (velW + aw) * d; velH = (velH + ah) * d;
    x += velX; y += velY; w += velW; h += velH;
    float pax = (targetPupilX - pupilX) * pk, pay = (targetPupilY - pupilY) * pk;
    pVelX = (pVelX + pax) * pd; pVelY = (pVelY + pay) * pd;
    pupilX += pVelX; pupilY += pVelY;
    if (w < 4) w = 4; if (h < 2) h = 2;
    if (x < 2) { x = 2; velX = 0; }
    if (y < 6) { y = 6; velY = 0; }
    if (x + w > SCREEN_WIDTH - 2) { x = SCREEN_WIDTH - 2 - w; velX = 0; }
    if (y + h > SCREEN_HEIGHT - 4) { y = SCREEN_HEIGHT - 4 - h; velY = 0; }
  }
};
Eye leftEye, rightEye;
unsigned long lastSaccade = 0, saccadeInterval = 3000;
float breathVal = 0;
unsigned long lastFidgetTime = 0;

struct TearDrop { float x, y, vy; bool active; };
TearDrop tears[4];
struct FloatingHeart { float x, y, vx, vy; uint8_t size; bool active; unsigned long spawnTime; };
FloatingHeart hearts[4];
struct Sparkle { float x, y, vx, vy; uint8_t life; bool active; };
Sparkle sparkles[8];

#define RAIN_DROPS 18
struct RainDrop { float x, y, vy; bool active; };
RainDrop raindrops[RAIN_DROPS];
bool rainOverlayActive = false;
bool lightningActive = false;
unsigned long lightningStart = 0;
int lightningFlashesRemaining = 0;

void triggerSparklesBurst(float cx, float cy);

// 5. PERSONALITY & STORAGE
struct Personality {
  uint8_t energy = 80, curiosity = 35, irritability = 0, contentment = 50;
  uint16_t familiarity = 0;
  uint32_t totalTouches = 0, petCount = 0;
  uint8_t wakeEarliness = 0;
  uint32_t lastCelebrationDay = 0;
  uint8_t lastBehavior = 0;
  uint32_t lastTouchEpoch = 0;
  uint32_t lastSadStreakEpoch = 0;
};
Personality personality;
Preferences prefs;
Preferences bdayPrefs;
bool personalityDirty = false;
unsigned long lastMemorySave = 0;

#define MAX_BIRTHDAYS 10
struct BirthdayEntry { char name[24]; uint8_t month, day; bool active; };
BirthdayEntry userBirthdays[MAX_BIRTHDAYS];
uint8_t birthdayCount = 0;

void setBehavior(BehaviorState next, unsigned long durationMs = 0, bool force = false);
void triggerBirthdayCelebration();
void spawnHeartsBurst();
void playReactionBasedOnContext(unsigned long offDuration);

// 6. STATE MACHINE INSTANCE
BehaviorState behaviorState = BEHAVIOR_AWAKE;
BehaviorState behaviorFallback = BEHAVIOR_AWAKE;
unsigned long behaviorEnteredAt = 0, behaviorDurationMs = 0;
unsigned long lastInteractionMillis = 0, lastEnergyTick = 0;
unsigned long manualWakeUntil = 0, lastManualWakeActivity = 0;
unsigned long lastRapidOverloadTime = 0, lastYawnTime = 0, lastCelebrationTime = 0;
unsigned long sleepEnteredAt = 0, bootMillis = 0;
bool isSleepingNow = false, isDozedNow = false, scheduledSleepActive = false;
unsigned long dozeEnteredAt = 0, lastFaceActiveAt = 0;
bool isBeingPetted = false, petWokeFromDoze = false;
unsigned long petStartTime = 0, lastHeartSpawn = 0;
bool doubleTapInteractive = false;
unsigned long rainShowerEndAt = 0, nextRainShowerAt = 0;

int currentPage = 0;
bool highBrightness = true, analogClock = false, useFahrenheit = false;
bool screenOffDueToIdle = false;

enum MiniGame : uint8_t { GAME_FLAPPY, GAME_BLINK_REFLEX, GAME_COUNT };
enum GamePhase : uint8_t { GAME_MENU, GAME_READY, GAME_PLAYING, GAME_OVER, GAME_RESULT };
MiniGame selectedGame = GAME_FLAPPY;
GamePhase gamePhase = GAME_MENU;
bool gameRunning = false;
bool gameTouchPressedBeforeGo = false;
uint8_t reflexLightsOn = 0;
unsigned long gameLastFrameAt = 0, gamePhaseAt = 0, gameStartedAt = 0;
float gameBirdY = 28.0f, gameBirdVelocity = 0.0f, gamePipeX = 112.0f;
int gamePipeGapY = 30, gameScore = 0, gameBestFlappyScore = 0;
unsigned long gameLastFlapAt = 0;
int gameReactionMs = 0, gameBestReactionMs = 0;
bool gamePipeScored = false;
int lastRenderedPage = -1;
unsigned long pageTransitionAt = 0;
uint8_t ambientArt = 0;
unsigned long ambientArtUntil = 0, nextAmbientArtAt = 0;

void startSelectedGame();
void handleGameTap();
void updateMiniGame();
void drawMiniGamePage();
void drawAmbientPixelArt();

void returnToFacePage() {
  if (currentPage == 5 && gameRunning) {
    gameRunning = false;
    gamePhase = GAME_MENU;
  }
  currentPage = 0;
  lastFaceActiveAt = millis();
  lastInteractionMillis = lastFaceActiveAt;
}

int tapCounter = 0;
unsigned long lastTapTime = 0, pressStartTime = 0;
bool isLongPressHandled = false;
int rapidTapWindowCount = 0;
unsigned long rapidTapWindowStart = 0;
int rapidTapBurstOriginPage = -1;
bool pendingThreeTapSleep = false;
uint32_t awaySecondsAtLastTouch = 0;

FaceGesture currentGesture = FG_NONE;
unsigned long gestureStartedAt = 0, gestureDurationMs = 0, nextGestureRoll = 0;
FaceGesture recentGestures[5] = { FG_NONE, FG_NONE, FG_NONE, FG_NONE, FG_NONE };
uint8_t recentGestureIdx = 0;

bool ntpSynced = false;
unsigned long lastNtpAttempt = 0, lastHttpTimeSyncAttempt = 0;
uint32_t ntpAttemptsCount = 0;
bool pendingTouchEpochUpdate = false;

enum WifiState : uint8_t { WIFI_STA_IDLE, WIFI_STA_SCANNING, WIFI_STA_CONNECTING, WIFI_STA_CONNECTED };
WifiState wifiStaState = WIFI_STA_IDLE;
unsigned long wifiStaStart = 0, lastWifiReconnectAttempt = 0;

enum WeatherFetchState : uint8_t { W_IDLE, W_GEOCODING, W_FETCHING_CURRENT, W_FETCHING_FORECAST };
enum WeatherRequestType : uint8_t { WX_GEOCODE, WX_CURRENT, WX_FORECAST };
WeatherFetchState weatherFetchState = W_IDLE;
unsigned long weatherFetchStart = 0, lastWeatherUpdate = 0;
bool weatherDataValid = false, weatherForecastValid = false;
int weatherHttpCode = 0;
int weatherForecastError = 0;
uint32_t weatherGeneration = 0;
SemaphoreHandle_t weatherResultMutex = nullptr;
bool weatherRequestRunning = false, weatherResultReady = false;
int weatherResultCode = 0;
WeatherRequestType weatherResultType = WX_CURRENT;
uint32_t weatherResultGeneration = 0;
String weatherResultPayload;
unsigned long weatherRefreshInterval = WEATHER_REFRESH_MS;
float temperature = 0.0, feelsLike = 0.0;
int humidity = 0;
long sunriseEpoch = 0, sunsetEpoch = 0;
String weatherMain = "Loading", weatherDesc = "Wait...";
struct ForecastDay { String dayName; int temp; String iconType; };
ForecastDay fcast[3];

unsigned long sadStreakUntil = 0, lastHeapLog = 0;

#define CONFIG_AP_SSID   "Kibo-Setup"
#define CONFIG_HOLD_MS   2500

WebServer server(80);
bool inSetupScreen = false;
String city, countryCode, apiKey, tzString;
String apPassword = "12345678";
const char* ntpServer = "pool.ntp.org";

const int MAX_SAVED_WIFI = 5;
SavedWifi savedWifiList[MAX_SAVED_WIFI];
uint8_t savedWifiCount = 0;
uint8_t currentWifiSlot = 0;
String currentConnectedSsid = "";
#define WIFI_RETRY_INTERVAL_MS 300000UL // 5 minutes between background reconnection sweeps

const int MAX_WEATHER_CITIES = 4;
String weatherCities[MAX_WEATHER_CITIES];
String weatherCityCountries[MAX_WEATHER_CITIES];
uint8_t weatherCityCount = 0;
uint8_t currentCityIndex = 0;
float weatherLatitude = 0.0f, weatherLongitude = 0.0f;
bool weatherCoordinatesValid = false;
bool weatherGeocodeRetriedWithoutCountry = false;
unsigned long citySwitchNotificationUntil = 0;

void parseCities(String raw) {
  raw.trim();
  weatherCityCount = 0;
  currentCityIndex = 0;
  if (raw.isEmpty()) {
    city = "";
    return;
  }

  const char separator = raw.indexOf(';') >= 0 ? ';' : ',';
  String inlineCountry = "";
  if (separator == ',') {
    int lastComma = raw.lastIndexOf(',');
    if (lastComma >= 0) {
      String suffix = raw.substring(lastComma + 1);
      suffix.trim();
      bool alphaCode = suffix.length() == 2 &&
        ((suffix[0] >= 'A' && suffix[0] <= 'Z') || (suffix[0] >= 'a' && suffix[0] <= 'z')) &&
        ((suffix[1] >= 'A' && suffix[1] <= 'Z') || (suffix[1] >= 'a' && suffix[1] <= 'z'));
      if (alphaCode) {
        inlineCountry = suffix;
        inlineCountry.toUpperCase();
        raw = raw.substring(0, lastComma);
      }
    }
  }
  int start = 0;
  while (start < raw.length() && weatherCityCount < MAX_WEATHER_CITIES) {
    int split = raw.indexOf(separator, start);
    String entry = split == -1 ? raw.substring(start) : raw.substring(start, split);
    entry.trim();
    String cityName = entry;
    String cityCountry = inlineCountry;
    int countrySep = entry.lastIndexOf('|');
    if (countrySep >= 0) {
      cityName = entry.substring(0, countrySep);
      cityCountry = entry.substring(countrySep + 1);
    } else if (separator == ';') {
      int commaSep = entry.lastIndexOf(',');
      if (commaSep >= 0) {
        String possibleCountry = entry.substring(commaSep + 1);
        possibleCountry.trim();
        if (possibleCountry.length() == 2) {
          cityName = entry.substring(0, commaSep);
          cityCountry = possibleCountry;
        }
      }
    }
    if (countrySep >= 0 || !cityCountry.isEmpty()) {
      cityName.trim();
      cityCountry.trim();
      cityCountry.toUpperCase();
      if (cityCountry.length() != 2) cityCountry = "";
    }

    bool duplicate = false;
    String effectiveCountry = cityCountry.isEmpty() ? countryCode : cityCountry;
    for (int i = 0; i < weatherCityCount; i++) {
      String savedCountry = weatherCityCountries[i].isEmpty() ? countryCode : weatherCityCountries[i];
      if (weatherCities[i].equalsIgnoreCase(cityName) &&
          savedCountry.equalsIgnoreCase(effectiveCountry)) {
        duplicate = true;
        break;
      }
    }
    if (!cityName.isEmpty() && !duplicate) {
      weatherCities[weatherCityCount] = cityName;
      weatherCityCountries[weatherCityCount] = cityCountry;
      weatherCityCount++;
    }
    if (split == -1) break;
    start = split + 1;
  }
  city = weatherCityCount > 0 ? weatherCities[0] : "";
}

String weatherCountryForCurrentCity() {
  if (currentCityIndex < weatherCityCount && !weatherCityCountries[currentCityIndex].isEmpty())
    return weatherCityCountries[currentCityIndex];
  return countryCode;
}

void selectWeatherCity(uint8_t index, unsigned long now) {
  if (weatherCityCount == 0 || index >= weatherCityCount) return;
  currentCityIndex = index;
  city = weatherCities[currentCityIndex];
  weatherCoordinatesValid = false;
  weatherGeocodeRetriedWithoutCountry = false;
  weatherLatitude = weatherLongitude = 0.0f;
  weatherGeneration++;
  weatherFetchState = W_IDLE;
  weatherDataValid = false;
  weatherForecastValid = false;
  weatherForecastError = 0;
  weatherHttpCode = 0;
  weatherMain = "Loading";
  weatherDesc = "Fetching weather";
  for (int i = 0; i < 3; i++) {
    fcast[i].dayName = "";
    fcast[i].iconType = "Clouds";
    fcast[i].temp = 0;
  }
  lastWeatherUpdate = 0;
  citySwitchNotificationUntil = now + 1600;
}

// 7. STORAGE
void loadBirthdays() {
  bdayPrefs.begin("kibo_bday", true);
  birthdayCount = bdayPrefs.getUChar("count", 0);
  if (birthdayCount > MAX_BIRTHDAYS) birthdayCount = MAX_BIRTHDAYS;
  for (uint8_t i = 0; i < birthdayCount; i++) {
    char kN[10], kM[10], kD[10];
    sprintf(kN, "n%u", i); sprintf(kM, "m%u", i); sprintf(kD, "d%u", i);
    String n = bdayPrefs.getString(kN, "");
    strncpy(userBirthdays[i].name, n.c_str(), sizeof(userBirthdays[i].name) - 1);
    userBirthdays[i].name[sizeof(userBirthdays[i].name) - 1] = '\0';
    userBirthdays[i].month = bdayPrefs.getUChar(kM, 1);
    userBirthdays[i].day = bdayPrefs.getUChar(kD, 1);
    userBirthdays[i].active = true;
  }
  bdayPrefs.end();
}
void saveBirthdays() {
  bdayPrefs.begin("kibo_bday", false);
  bdayPrefs.clear();
  bdayPrefs.putUChar("count", birthdayCount);
  for (uint8_t i = 0; i < birthdayCount; i++) {
    char kN[10], kM[10], kD[10];
    sprintf(kN, "n%u", i); sprintf(kM, "m%u", i); sprintf(kD, "d%u", i);
    bdayPrefs.putString(kN, String(userBirthdays[i].name));
    bdayPrefs.putUChar(kM, userBirthdays[i].month);
    bdayPrefs.putUChar(kD, userBirthdays[i].day);
  }
  bdayPrefs.end();
}
void loadPersonality() {
  prefs.begin("kibo_p", true);
  personality.energy = prefs.getUChar("energy", 80);
  personality.curiosity = prefs.getUChar("curiosity", 35);
  personality.irritability = prefs.getUChar("irrita", 0);
  personality.contentment = prefs.getUChar("content", 50);
  personality.familiarity = prefs.getUShort("familiar", 0);
  personality.totalTouches = prefs.getUInt("touches", 0);
  personality.petCount = prefs.getUInt("petCount", 0);
  personality.wakeEarliness = prefs.getUChar("wakeEarly", 0);
  personality.lastCelebrationDay = prefs.getUInt("lastBday", 0);
  personality.lastBehavior = prefs.getUChar("lastBeh", 0);
  personality.lastTouchEpoch = prefs.getUInt("lastTouch", 0);
  personality.lastSadStreakEpoch = prefs.getUInt("lastSad", 0);
  prefs.end();
  if (personality.energy > 100) personality.energy = 80;
  if (personality.curiosity > 100) personality.curiosity = 35;
  if (personality.irritability > 100) personality.irritability = 0;
  if (personality.contentment > 100) personality.contentment = 50;
  if (personality.familiarity > 9999) personality.familiarity = 0;
  if (personality.wakeEarliness > 30) personality.wakeEarliness = 0;
  if (personality.lastBehavior > 26) personality.lastBehavior = 0;
  personalityDirty = false;
}
void savePersonality(bool force = false) {
  if (!personalityDirty) return;
  if (!force && millis() - lastMemorySave < MEMORY_SAVE_INTERVAL_MS) return;
  prefs.begin("kibo_p", false);
  prefs.putUChar("schema", 4);
  prefs.putUChar("energy", personality.energy);
  prefs.putUChar("curiosity", personality.curiosity);
  prefs.putUChar("irrita", personality.irritability);
  prefs.putUChar("content", personality.contentment);
  prefs.putUShort("familiar", personality.familiarity);
  prefs.putUInt("touches", personality.totalTouches);
  prefs.putUInt("petCount", personality.petCount);
  prefs.putUChar("wakeEarly", personality.wakeEarliness);
  prefs.putUInt("lastBday", personality.lastCelebrationDay);
  prefs.putUChar("lastBeh", (uint8_t)behaviorState);
  prefs.putUInt("lastTouch", personality.lastTouchEpoch);
  prefs.putUInt("lastSad", personality.lastSadStreakEpoch);
  prefs.end();
  lastMemorySave = millis();
  personalityDirty = false;
}
void loadConfig() {
  prefs.begin("kibo", true);
  savedWifiCount = 0;
  for (int i = 0; i < MAX_SAVED_WIFI; i++) {
    String sKey = "w_s" + String(i);
    String pKey = "w_p" + String(i);
    String s = prefs.getString(sKey.c_str(), "");
    String p = prefs.getString(pKey.c_str(), "");
    if (s.isEmpty() && i == 0) {
      s = prefs.getString("ssid", "");
      p = prefs.getString("pass", "");
    } else if (s.isEmpty() && i == 1) {
      s = prefs.getString("ssid2", "");
      p = prefs.getString("pass2", "");
    }
    savedWifiList[i].ssid = s;
    savedWifiList[i].pass = p;
    if (!s.isEmpty()) savedWifiCount++;
  }
  apiKey = prefs.getString("apikey", "");
  String rawCities = prefs.getString("cities", "");
  if (rawCities.isEmpty()) rawCities = prefs.getString("city", "");
  countryCode = prefs.getString("country", "IN");
  tzString = prefs.getString("tz", "IST-5:30");
  apPassword = prefs.getString("appass", "12345678");
  prefs.end();
  parseCities(rawCities);
  if (countryCode.isEmpty()) countryCode = "IN";
  countryCode.toUpperCase();
  if (tzString.isEmpty()) tzString = "IST-5:30";
  if (apPassword.length() < 8) apPassword = "12345678";
}
void saveConfig(const SavedWifi* wifis, int numWifis,
                const String& ak, const String& cty, const String& ctry,
                const String& tz, const String& ap) {
  prefs.begin("kibo", false);
  savedWifiCount = 0;
  for (int i = 0; i < MAX_SAVED_WIFI; i++) {
    String sKey = "w_s" + String(i);
    String pKey = "w_p" + String(i);
    if (i < numWifis && !wifis[i].ssid.isEmpty()) {
      prefs.putString(sKey.c_str(), wifis[i].ssid);
      prefs.putString(pKey.c_str(), wifis[i].pass);
      savedWifiList[i].ssid = wifis[i].ssid;
      savedWifiList[i].pass = wifis[i].pass;
      savedWifiCount++;
    } else {
      prefs.putString(sKey.c_str(), "");
      prefs.putString(pKey.c_str(), "");
      savedWifiList[i].ssid = "";
      savedWifiList[i].pass = "";
    }
  }
  prefs.putString("ssid", savedWifiList[0].ssid);
  prefs.putString("pass", savedWifiList[0].pass);
  prefs.putString("ssid2", savedWifiList[1].ssid);
  prefs.putString("pass2", savedWifiList[1].pass);

  prefs.putString("apikey", ak); prefs.putString("city", cty);
  prefs.putString("cities", cty);
  prefs.putString("country", ctry); prefs.putString("tz", tz);
  prefs.putString("appass", ap);
  prefs.end();
  parseCities(cty);
}

// 8. QR SETUP
void drawQRCode(const char* text, const char* title, const char* sub1, const char* sub2) {
  display.clearDisplay();
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  int8_t res = qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, text);
  if (res < 0) {
    display.setFont(NULL); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 4);
    display.println("Kibo Setup");
    display.println("SSID: Kibo-Setup");
    display.println("Pass: " + apPassword);
    display.println("IP: 192.168.4.1");
    display.display();
    return;
  }
  const int scale = 2, startX = 4, startY = 3;
  display.fillRect(startX - 2, startY - 2, (qrcode.size * scale) + 4, (qrcode.size * scale) + 4, SSD1306_WHITE);
  for (uint8_t y = 0; y < qrcode.size; y++)
    for (uint8_t x = 0; x < qrcode.size; x++)
      if (qrcode_getModule(&qrcode, x, y))
        display.fillRect(startX + (x * scale), startY + (y * scale), scale, scale, SSD1306_BLACK);
  display.setFont(NULL); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(68, 4);  display.print(title);
  display.setCursor(68, 20); display.print(sub1);
  display.setCursor(68, 36); display.print(sub2);
  display.setCursor(68, 52); display.print("192.168.4.1");
  display.display();
}
void renderQRSetupScreen() {
  static int lastClientCount = -1;
  int clients = WiFi.softAPgetStationNum();
  if (clients != lastClientCount) {
    lastClientCount = clients;
    if (clients == 0) {
      char payload[80];
      sprintf(payload, "WIFI:S:Kibo-Setup;T:WPA;P:%s;;", apPassword.c_str());
      drawQRCode(payload, "SCAN WIFI", "SSID:", "Kibo-Setup");
    } else {
      drawQRCode("http://192.168.4.1/", "CONNECTED!", "Scan to", "Configure");
    }
  }
}

// 9. WEB PORTAL
void handlePortalRoot() {
  prefs.begin("kibo", true);
  String sApik = prefs.getString("apikey", "");
  String sCities = prefs.getString("cities", "");
  if (sCities.isEmpty()) sCities = prefs.getString("city", "");
  String sCtry = prefs.getString("country", "IN");
  String sTz = prefs.getString("tz", "IST-5:30");
  prefs.end();
  String html = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b1017"><meta name="color-scheme" content="dark">
<title>Kibo</title>
<style>
:root{--bg:#0b1017;--surface:#121923;--field:#0d131c;--line:#212c3b;--line-2:#2c3a4f;--text:#e6edf5;--muted:#8795a8;--accent:#4da3ff;--accent-press:#2f86e0;--danger:#ef5350;--ok:#43a047;}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent;}
html{-webkit-text-size-adjust:100%;}
body{margin:0;background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;font-size:15px;line-height:1.45;padding:max(16px,env(safe-area-inset-top)) max(16px,env(safe-area-inset-right)) max(24px,env(safe-area-inset-bottom)) max(16px,env(safe-area-inset-left));}
.wrap{max-width:480px;margin:0 auto;}
.i{width:20px;height:20px;flex:none;fill:none;stroke:currentColor;stroke-width:1.5;stroke-linecap:round;stroke-linejoin:round;}
header{display:flex;align-items:center;gap:12px;margin:4px 0 20px;}
.logo{width:40px;height:40px;border-radius:10px;background:var(--surface);border:1px solid var(--line);display:flex;align-items:center;justify-content:center;color:var(--accent);}
.logo .i{width:24px;height:24px;}
h1{margin:0;font-size:20px;font-weight:600;letter-spacing:-.01em;line-height:1.2;}
.subtitle{color:var(--muted);font-size:13px;}
.layout{display:grid;grid-template-columns:1fr;gap:14px;}
.layout>form{display:contents;}
.card{background:var(--surface);border:1px solid var(--line);border-radius:12px;padding:16px;}
.card-title{display:flex;align-items:center;gap:8px;font-size:14px;font-weight:600;margin:0 0 4px;color:var(--text);}
.card-title .i{color:var(--accent);}
.hint{color:var(--muted);font-size:12.5px;margin:0 0 12px;}
.net{padding-top:12px;margin-top:12px;border-top:1px solid var(--line);}
.net:first-of-type{border-top:0;margin-top:0;padding-top:0;}
.net-title{font-size:11.5px;font-weight:600;letter-spacing:.06em;text-transform:uppercase;color:var(--muted);margin-bottom:2px;}
label{display:block;margin-top:10px;color:var(--muted);font-size:12.5px;}
input{width:100%;min-height:44px;padding:10px 12px;margin:4px 0 0;border:1px solid var(--line-2);border-radius:8px;background:var(--field);color:var(--text);font:inherit;font-size:16px;outline:none;transition:border-color .15s;}
input::placeholder{color:#5d6b7e;}
input:focus{border-color:var(--accent);}
.row{display:grid;grid-template-columns:1fr 1fr;gap:10px;}
.row.stack{grid-template-columns:1fr;gap:0;}
button{font:inherit;cursor:pointer;}
button:focus-visible,input:focus-visible{outline:2px solid var(--accent);outline-offset:2px;}
button.primary{width:100%;min-height:46px;margin-top:14px;display:flex;align-items:center;justify-content:center;gap:8px;background:var(--accent);color:#06101c;border:0;border-radius:8px;font-size:15px;font-weight:600;transition:background .15s;}
button.primary:active{background:var(--accent-press);}
button.primary.add{background:transparent;color:var(--ok);border:1px solid var(--ok);}
button.primary.add:active{background:rgba(67,160,71,.15);}
.btn-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:4px;}
.btn-mood{min-height:46px;display:flex;align-items:center;gap:10px;padding:8px 12px;background:var(--field);color:var(--text);border:1px solid var(--line-2);border-radius:8px;font-size:13.5px;text-align:left;transition:border-color .15s,background .15s;}
.btn-mood .i{color:var(--accent);}
.btn-mood:active{background:var(--accent);border-color:var(--accent);color:#06101c;}
.btn-mood:active .i{color:#06101c;}
#mstat{color:var(--accent);font-size:12.5px;margin-top:10px;min-height:18px;}
.bday-item{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:8px 0;border-bottom:1px solid var(--line);min-height:48px;}
.bday-item .bn{display:flex;align-items:center;gap:8px;min-width:0;word-break:break-word;}
.bday-item .bn .i{color:var(--muted);}
.bday-item .bd{color:var(--muted);font-variant-numeric:tabular-nums;font-size:13px;margin-left:auto;}
.bday-item form{margin:0;}
.icon-btn{width:40px;height:40px;display:flex;align-items:center;justify-content:center;background:transparent;border:1px solid var(--line-2);border-radius:8px;color:var(--muted);}
.icon-btn.danger:active{background:var(--danger);border-color:var(--danger);color:#fff;}
.muted{color:var(--muted);font-size:13px;margin:4px 0 0;}
@media(hover:hover){
.btn-mood:hover{border-color:var(--accent);}
button.primary:hover{background:var(--accent-press);}
button.primary.add:hover{background:rgba(67,160,71,.12);}
.icon-btn.danger:hover{border-color:var(--danger);color:var(--danger);}
}
@media(min-width:900px){
body{padding-top:32px;}
.wrap{max-width:1040px;}
.layout{grid-template-columns:minmax(0,1.1fr) minmax(0,1fr);gap:16px;align-items:start;}
.layout>form{display:block;grid-row:1 / span 2;}
.layout>form .card{margin-bottom:16px;}
.row.stack{grid-template-columns:1fr 1fr;gap:10px;}
.btn-grid{grid-template-columns:1fr 1fr;}
}
</style></head><body>
<svg width="0" height="0" style="position:absolute" aria-hidden="true" focusable="false"><defs>
<symbol id="i-face" viewBox="0 0 24 24"><path d="M15.182 15.182a4.5 4.5 0 0 1-6.364 0M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0ZM9.75 9.75c0 .414-.168.75-.375.75S9 10.164 9 9.75 9.168 9 9.375 9s.375.336.375.75Zm-.375 0h.008v.015h-.008V9.75Zm5.625 0c0 .414-.168.75-.375.75s-.375-.336-.375-.75.168-.75.375-.75.375.336.375.75Zm-.375 0h.008v.015h-.008V9.75Z"/></symbol>
<symbol id="i-frown" viewBox="0 0 24 24"><path d="M15.182 16.318A4.486 4.486 0 0 0 12.016 15a4.486 4.486 0 0 0-3.198 1.318M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0ZM9.75 9.75c0 .414-.168.75-.375.75S9 10.164 9 9.75 9.168 9 9.375 9s.375.336.375.75Zm-.375 0h.008v.015h-.008V9.75Zm5.625 0c0 .414-.168.75-.375.75s-.375-.336-.375-.75.168-.75.375-.75.375.336.375.75Zm-.375 0h.008v.015h-.008V9.75Z"/></symbol>
<symbol id="i-sparkles" viewBox="0 0 24 24"><path d="M9.813 15.904 9 18.75l-.813-2.846a4.5 4.5 0 0 0-3.09-3.09L2.25 12l2.846-.813a4.5 4.5 0 0 0 3.09-3.09L9 5.25l.813 2.846a4.5 4.5 0 0 0 3.09 3.09L15.75 12l-2.846.813a4.5 4.5 0 0 0-3.09 3.09ZM18.259 8.715 18 9.75l-.259-1.035a3.375 3.375 0 0 0-2.455-2.456L14.25 6l1.036-.259a3.375 3.375 0 0 0 2.455-2.456L18 2.25l.259 1.035a3.375 3.375 0 0 0 2.456 2.456L21.75 6l-1.035.259a3.375 3.375 0 0 0-2.456 2.456ZM16.894 20.567 16.5 21.75l-.394-1.183a2.25 2.25 0 0 0-1.423-1.423L13.5 18.75l1.183-.394a2.25 2.25 0 0 0 1.423-1.423l.394-1.183.394 1.183a2.25 2.25 0 0 0 1.423 1.423l1.183.394-1.183.394a2.25 2.25 0 0 0-1.423 1.423Z"/></symbol>
<symbol id="i-eye-slash" viewBox="0 0 24 24"><path d="M3.98 8.223A10.477 10.477 0 0 0 1.934 12C3.226 16.338 7.244 19.5 12 19.5c.993 0 1.953-.138 2.863-.395M6.228 6.228A10.451 10.451 0 0 1 12 4.5c4.756 0 8.773 3.162 10.065 7.498a10.522 10.522 0 0 1-4.293 5.774M6.228 6.228 3 3m3.228 3.228 3.65 3.65m7.894 7.894L21 21m-3.228-3.228-3.65-3.65m0 0a3 3 0 1 0-4.243-4.243m4.242 4.242L9.88 9.88"/></symbol>
<symbol id="i-battery" viewBox="0 0 24 24"><path d="M21 10.5h.375c.621 0 1.125.504 1.125 1.125v2.25c0 .621-.504 1.125-1.125 1.125H21M3.75 18h15A2.25 2.25 0 0 0 21 15.75v-6a2.25 2.25 0 0 0-2.25-2.25h-15A2.25 2.25 0 0 0 1.5 9.75v6A2.25 2.25 0 0 0 3.75 18Z"/></symbol>
<symbol id="i-bolt" viewBox="0 0 24 24"><path d="m3.75 13.5 10.5-11.25L12 10.5h8.25L9.75 21.75 12 13.5H3.75Z"/></symbol>
<symbol id="i-no" viewBox="0 0 24 24"><path d="M18.364 18.364A9 9 0 0 0 5.636 5.636m12.728 12.728A9 9 0 0 1 5.636 5.636m12.728 12.728L5.636 5.636"/></symbol>
<symbol id="i-fire" viewBox="0 0 24 24"><path d="M15.362 5.214A8.252 8.252 0 0 1 12 21 8.25 8.25 0 0 1 6.038 7.047 8.287 8.287 0 0 0 9 9.601a8.983 8.983 0 0 1 3.361-6.867 8.21 8.21 0 0 0 3 2.48Z"/><path d="M12 18a3.75 3.75 0 0 0 .495-7.468 5.99 5.99 0 0 0-1.925 3.547 5.975 5.975 0 0 1-2.133-1.001A3.75 3.75 0 0 0 12 18Z"/></symbol>
<symbol id="i-speaker-x" viewBox="0 0 24 24"><path d="M17.25 9.75 19.5 12m0 0 2.25 2.25M19.5 12l2.25-2.25M19.5 12l-2.25 2.25m-10.5-6 4.72-4.72a.75.75 0 0 1 1.28.53v15.88a.75.75 0 0 1-1.28.53l-4.72-4.72H4.51c-.88 0-1.704-.507-1.938-1.354A9.009 9.009 0 0 1 2.25 12c0-.83.112-1.633.322-2.396C2.806 8.756 3.63 8.25 4.51 8.25H6.75Z"/></symbol>
<symbol id="i-minus-circle" viewBox="0 0 24 24"><path d="M15 12H9m12 0a9 9 0 1 1-18 0 9 9 0 0 1 18 0Z"/></symbol>
<symbol id="i-wifi" viewBox="0 0 24 24"><path d="M8.288 15.038a5.25 5.25 0 0 1 7.424 0M5.106 11.856c3.807-3.808 9.98-3.808 13.788 0M1.924 8.674c5.565-5.565 14.587-5.565 20.152 0M12.53 18.22l-.53.53-.53-.53a.75.75 0 0 1 1.06 0Z"/></symbol>
<symbol id="i-cloud" viewBox="0 0 24 24"><path d="M2.25 15a4.5 4.5 0 0 0 4.5 4.5H18a3.75 3.75 0 0 0 1.332-7.257 3 3 0 0 0-3.758-3.848 5.25 5.25 0 0 0-10.233 2.33A4.502 4.502 0 0 0 2.25 15Z"/></symbol>
<symbol id="i-cake" viewBox="0 0 24 24"><path d="M12 8.25v-1.5m0 1.5c-1.355 0-2.697.056-4.024.166C6.845 8.51 6 9.473 6 10.608v2.513m6-4.871c1.355 0 2.697.056 4.024.166C17.155 8.51 18 9.473 18 10.608v2.513M15 8.25v-1.5m-6 1.5v-1.5m12 9.75-1.5.75a3.354 3.354 0 0 1-3 0 3.354 3.354 0 0 0-3 0 3.354 3.354 0 0 1-3 0 3.354 3.354 0 0 0-3 0 3.354 3.354 0 0 1-3 0L3 16.5m15-3.379a48.474 48.474 0 0 0-6-.371c-2.032 0-4.034.126-6 .371m12 0c.39.049.777.102 1.163.16 1.07.16 1.837 1.094 1.837 2.175v5.169c0 .621-.504 1.125-1.125 1.125H4.125A1.125 1.125 0 0 1 3 20.625v-5.17c0-1.08.768-2.014 1.837-2.174A47.78 47.78 0 0 1 6 13.12M12.265 3.11a.375.375 0 1 1-.53 0L12 2.845l.265.265Zm-3 0a.375.375 0 1 1-.53 0L9 2.845l.265.265Zm6 0a.375.375 0 1 1-.53 0L15 2.845l.265.265Z"/></symbol>
<symbol id="i-face-smile-sm" viewBox="0 0 24 24"><path d="M15.182 15.182a4.5 4.5 0 0 1-6.364 0M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0Z"/></symbol>
<symbol id="i-trash" viewBox="0 0 24 24"><path d="m14.74 9-.346 9m-4.788 0L9.26 9m9.968-3.21c.342.052.682.107 1.022.166m-1.022-.165L18.16 19.673a2.25 2.25 0 0 1-2.244 2.077H8.084a2.25 2.25 0 0 1-2.244-2.077L4.772 5.79m14.456 0a48.108 48.108 0 0 0-3.478-.397m-12 .562c.34-.059.68-.114 1.022-.165m0 0a48.11 48.11 0 0 1 3.478-.397m7.5 0v-.916c0-1.18-.91-2.164-2.09-2.201a51.964 51.964 0 0 0-3.32 0c-1.18.037-2.09 1.022-2.09 2.201v.916m7.5 0a48.667 48.667 0 0 0-7.5 0"/></symbol>
<symbol id="i-plus" viewBox="0 0 24 24"><path d="M12 4.5v15m7.5-7.5h-15"/></symbol>
<symbol id="i-refresh" viewBox="0 0 24 24"><path d="M16.023 9.348h4.992v-.001M2.985 19.644v-4.992m0 0h4.992m-4.993 0 3.181 3.183a8.25 8.25 0 0 0 13.803-3.7M4.031 9.865a8.25 8.25 0 0 1 13.803-3.7l3.181 3.182m0-4.991v4.99"/></symbol>
<symbol id="i-face-bolt" viewBox="0 0 24 24"><path d="M15.182 15.182a4.5 4.5 0 0 1-6.364 0M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0Z"/></symbol>
</defs></svg>
<div class="wrap">
<header><div class="logo"><svg class="i"><use href="#i-face"/></svg></div><div><h1>Kibo</h1><div class="subtitle">Configuration Portal</div></div></header>
<main class="layout">
<form action="/save" method="POST">
<div class="card"><div class="card-title"><svg class="i"><use href="#i-wifi"/></svg>Saved Wi-Fi Networks</div>
<p class="hint">Up to 5. Kibo scans the air and connects to the first matched network in range.</p>
)rawliteral";
  for (int i = 0; i < MAX_SAVED_WIFI; i++) {
    html += "<div class='net'><div class='net-title'>Network " + String(i + 1) + "</div>";
    html += "<div class='row stack'><div><label>SSID</label><input name='w_s" + String(i) + "' value='" + savedWifiList[i].ssid + "' autocapitalize='none' autocomplete='off' spellcheck='false'></div>";
    html += "<div><label>Password</label><input name='w_p" + String(i) + "' type='password' placeholder='leave blank to keep' autocomplete='new-password'></div></div></div>";
  }
  html += R"rawliteral(
<div class="net"><div class="net-title">Access Point</div>
<label>AP Password (min 8)</label><input name="appass" type="password" placeholder="leave blank to keep" minlength="8" autocomplete="new-password"></div>
</div>
<div class="card"><div class="card-title"><svg class="i"><use href="#i-cloud"/></svg>Weather &amp; Time</div>
<label>OWM API Key</label><input name="apikey" autocapitalize="none" autocomplete="off" spellcheck="false" value=")rawliteral";
  html += sApik;
  html += R"rawliteral(">
<label>Weather cities (up to 4; hold touch on a weather page to switch)</label>
<p class="hint">Use semicolons for mixed countries: Bhubaneswar|IN; Cuttack|IN; Tokyo|JP. Cities without a code use the shared country below.</p>
<input name="cities" autocomplete="off" value=")rawliteral";
  html += sCities;
  html += R"rawliteral(" placeholder="e.g. London|GB; Tokyo|JP; New York|US">
<div class="row">
<div><label>Country Code</label><input name="country" autocapitalize="characters" autocomplete="off" value=")rawliteral";
  html += sCtry;
  html += R"rawliteral("></div>
<div><label>Timezone</label><input name="tz" autocapitalize="none" autocomplete="off" value=")rawliteral";
  html += sTz;
  html += R"rawliteral("></div>
</div>
</div>
<button type="submit" class="primary"><svg class="i"><use href="#i-refresh"/></svg>Save &amp; Reboot</button>
</form>

<div class="card"><div class="card-title"><svg class="i"><use href="#i-face"/></svg>Live Emotions &amp; Reactions</div>
<p class="hint">Tap to trigger an immediate facial reaction on Kibo.</p>
<div class="btn-grid">
<button type="button" class="btn-mood" onclick="sendMood('happy')"><svg class="i"><use href="#i-face"/></svg>Happy</button>
<button type="button" class="btn-mood" onclick="sendMood('excited')"><svg class="i"><use href="#i-sparkles"/></svg>Excited</button>
<button type="button" class="btn-mood" onclick="sendMood('shy')"><svg class="i"><use href="#i-eye-slash"/></svg>Shy</button>
<button type="button" class="btn-mood" onclick="sendMood('crying')"><svg class="i"><use href="#i-frown"/></svg>Crying</button>
<button type="button" class="btn-mood" onclick="sendMood('low_energy')"><svg class="i"><use href="#i-battery"/></svg>Low Energy</button>
<button type="button" class="btn-mood" onclick="sendMood('over_excited')"><svg class="i"><use href="#i-bolt"/></svg>Over Excited</button>
<button type="button" class="btn-mood" onclick="sendMood('fed_up')"><svg class="i"><use href="#i-no"/></svg>Fed Up</button>
<button type="button" class="btn-mood" onclick="sendMood('angry')"><svg class="i"><use href="#i-fire"/></svg>Angry</button>
<button type="button" class="btn-mood" onclick="sendMood('ignoring')"><svg class="i"><use href="#i-speaker-x"/></svg>Ignoring</button>
<button type="button" class="btn-mood" onclick="sendMood('dont_care')"><svg class="i"><use href="#i-minus-circle"/></svg>Don't Care</button>
</div>
<div id="mstat" role="status" aria-live="polite"></div>
</div>

<div class="card"><div class="card-title"><svg class="i"><use href="#i-cake"/></svg>Birthdays</div>
)rawliteral";
  if (birthdayCount == 0) html += "<p class='muted'>None yet.</p>";
  else for (uint8_t i = 0; i < birthdayCount; i++) {
    html += "<div class='bday-item'><div class='bn'>" + String(userBirthdays[i].name) +
            "</div><div class='bd'>" + String(userBirthdays[i].day) + "/" + String(userBirthdays[i].month) +
            "</div><form action='/del_bday' method='POST'><input type='hidden' name='idx' value='" +
            String(i) + "'><button type='submit' class='icon-btn danger' aria-label='Delete'><svg class='i'><use href='#i-trash'/></svg></button></form></div>";
  }
  if (birthdayCount < MAX_BIRTHDAYS) {
    html += R"rawliteral(<form action="/add_bday" method="POST" style="margin-top:14px;">
<input name="bname" placeholder="Name" maxlength="20" autocomplete="off" required>
<div class="row">
<div><label>Month</label><input name="bmonth" type="number" inputmode="numeric" min="1" max="12" required></div>
<div><label>Day</label><input name="bday" type="number" inputmode="numeric" min="1" max="31" required></div>
</div><button type="submit" class="primary add"><svg class="i"><use href="#i-plus"/></svg>Add</button></form>)rawliteral";
  }
  html += R"rawliteral(</div>
</main>
</div>
<script>
function sendMood(m){
  var s=document.getElementById('mstat');
  s.textContent='Triggering '+m+'...';
  fetch('/mood?m='+m).then(function(r){return r.text();}).then(function(t){
    s.textContent='Reaction set: '+m;
    setTimeout(function(){s.textContent='';},3000);
  }).catch(function(){s.textContent='Failed to trigger';});
}
try{
  var ep=Math.floor(Date.now()/1000);
  fetch('/set_time?epoch='+ep).catch(function(){});
}catch(e){}
</script>
</body></html>)rawliteral";
  server.send(200, "text/html", html);
}
void handlePortalSave() {
  SavedWifi w[MAX_SAVED_WIFI];
  int nW = 0;
  prefs.begin("kibo", true);
  for (int i = 0; i < MAX_SAVED_WIFI; i++) {
    String sArg = "w_s" + String(i);
    String pArg = "w_p" + String(i);
    String s = server.hasArg(sArg) ? server.arg(sArg) : "";
    String p = server.hasArg(pArg) ? server.arg(pArg) : "";
    s.trim();
    if (!s.isEmpty()) {
      if (p.isEmpty()) {
        p = prefs.getString(("w_p" + String(i)).c_str(), "");
        if (p.isEmpty() && i == 0) p = prefs.getString("pass", "");
        if (p.isEmpty() && i == 1) p = prefs.getString("pass2", "");
      }
      w[nW].ssid = s;
      w[nW].pass = p;
      nW++;
    }
  }
  prefs.end();

  if (nW == 0) {
    server.send(400, "text/plain", "At least one Wi-Fi SSID is required"); return;
  }

  String ak = server.arg("apikey");
  String cty = server.arg("cities");
  if (cty.isEmpty()) cty = server.arg("city");
  String ctr = server.arg("country"), tz = server.arg("tz");
  String ap = server.arg("appass");
  if (ctr.isEmpty()) ctr = "IN";
  if (tz.isEmpty()) tz = "IST-5:30";
  if (ap.isEmpty()) ap = apPassword;
  saveConfig(w, nW, ak, cty, ctr, tz, ap);
  server.send(200, "text/html",
    "<html><body style='font-family:sans-serif;background:#0b1320;color:#e2edf8;padding:40px;'>"
    "<h2 style='color:#64b5f6'>Saved!</h2><p>Rebooting...</p></body></html>");
  delay(1500);
  ESP.restart();
}
void handleAddBirthday() {
  if (birthdayCount >= MAX_BIRTHDAYS) { server.send(400, "text/plain", "Full"); return; }
  String n = server.arg("bname"); int m = server.arg("bmonth").toInt(); int d = server.arg("bday").toInt();
  n.trim();
  if (n.isEmpty() || m < 1 || m > 12 || d < 1 || d > 31) { server.send(400, "text/plain", "Invalid"); return; }
  strncpy(userBirthdays[birthdayCount].name, n.c_str(), sizeof(userBirthdays[birthdayCount].name) - 1);
  userBirthdays[birthdayCount].name[sizeof(userBirthdays[birthdayCount].name) - 1] = '\0';
  userBirthdays[birthdayCount].month = (uint8_t)m;
  userBirthdays[birthdayCount].day = (uint8_t)d;
  userBirthdays[birthdayCount].active = true;
  birthdayCount++;
  saveBirthdays();
  server.sendHeader("Location", "/"); server.send(303);
}
void handleDeleteBirthday() {
  if (!server.hasArg("idx")) { server.send(400, "text/plain", "Missing"); return; }
  int idx = server.arg("idx").toInt();
  if (idx < 0 || idx >= birthdayCount) { server.send(400, "text/plain", "Bad"); return; }
  for (int i = idx; i < birthdayCount - 1; i++) userBirthdays[i] = userBirthdays[i + 1];
  birthdayCount--;
  saveBirthdays();
  server.sendHeader("Location", "/"); server.send(303);
}
void handleSetMood() {
  if (!server.hasArg("m")) { server.send(400, "text/plain", "Missing m"); return; }
  String m = server.arg("m");
  m.toLowerCase();
  returnToFacePage();
  if (m == "happy") setBehavior(BEHAVIOR_HAPPY, 5000, true);
  else if (m == "excited") { triggerSparklesBurst(64.0f, 24.0f); setBehavior(BEHAVIOR_EXCITED, 5000, true); }
  else if (m == "shy") setBehavior(BEHAVIOR_SHY, 5000, true);
  else if (m == "crying") setBehavior(BEHAVIOR_CRYING, 5000, true);
  else if (m == "low_energy" || m == "lowenergy") setBehavior(BEHAVIOR_LOW_ENERGY, 5000, true);
  else if (m == "over_excited" || m == "overexcited") { triggerSparklesBurst(64.0f, 24.0f); setBehavior(BEHAVIOR_OVER_EXCITED, 5000, true); }
  else if (m == "fed_up" || m == "fedup") setBehavior(BEHAVIOR_FED_UP, 5000, true);
  else if (m == "angry") setBehavior(BEHAVIOR_ANGRY, 5000, true);
  else if (m == "ignoring") setBehavior(BEHAVIOR_IGNORING, 5000, true);
  else if (m == "dont_care" || m == "dontcare") setBehavior(BEHAVIOR_DONT_CARE, 5000, true);
  else { server.send(400, "text/plain", "Unknown mood"); return; }
  server.send(200, "text/plain", "OK: " + m);
}
void handleSetTime() {
  if (server.hasArg("epoch")) {
    long ep = server.arg("epoch").toInt();
    if (ep > 1700000000) {
      struct timeval tv = { .tv_sec = ep, .tv_usec = 0 };
      settimeofday(&tv, NULL);
      ntpSynced = true;
      DBG("[TIME] Synced via portal browser epoch=%ld\n", ep);
      server.send(200, "text/plain", "OK");
      return;
    }
  }
  server.send(400, "text/plain", "Bad epoch");
}
void setupWebServerRoutes() {
  server.on("/", HTTP_GET, handlePortalRoot);
  server.on("/save", HTTP_POST, handlePortalSave);
  server.on("/add_bday", HTTP_POST, handleAddBirthday);
  server.on("/del_bday", HTTP_POST, handleDeleteBirthday);
  server.on("/mood", HTTP_GET, handleSetMood);
  server.on("/set_time", HTTP_GET, handleSetTime);
}

// 10. BEHAVIOUR TRANSITIONS
void setBehavior(BehaviorState next, unsigned long durationMs, bool force) {
  if (next == behaviorState && !force) return;
  if (!force && isTemporaryBehavior(behaviorState) &&
      (millis() - behaviorEnteredAt < behaviorDurationMs) &&
      (behaviorPriority(next) < behaviorPriority(behaviorState))) return;
  if (isTemporaryBehavior(next) && !isTemporaryBehavior(behaviorState))
    behaviorFallback = behaviorState;
  DBG("[BEH] %u -> %u\n", (uint8_t)behaviorState, (uint8_t)next);
  behaviorState = next;
  behaviorEnteredAt = millis();
  behaviorDurationMs = durationMs;
  if (next != BEHAVIOR_AWAKE) {
    currentGesture = FG_NONE;
    leftEye.targetPupilX = rightEye.targetPupilX = 0;
    leftEye.targetPupilY = rightEye.targetPupilY = 0;
  }
  if (next == BEHAVIOR_AWAKE) {
    leftEye.targetPupilX = rightEye.targetPupilX = 0;
    leftEye.targetPupilY = rightEye.targetPupilY = 0;
    currentGesture = FG_NONE;
    nextGestureRoll = millis() + random(GESTURE_ROLL_MIN_MS, GESTURE_ROLL_MAX_MS);
  }
  lastSaccade = 0;
  if (behaviorState == BEHAVIOR_CRYING) {
    tears[0] = { 32.0f, 36.0f, 0.45f, true };
    tears[1] = { 88.0f, 38.0f, 0.40f, true };
    tears[2] = { 30.0f, 42.0f, 0.50f, true };
    tears[3] = { 90.0f, 44.0f, 0.45f, true };
  }
}

// 11. HEARTS, SPARKLES, RAIN, LIGHTNING
void spawnHeartsBurst() {
  hearts[0].x = 114.0f; hearts[0].y = 44.0f;
  hearts[0].vx = 0.05f; hearts[0].vy = -0.40f;
  hearts[0].size = 0; hearts[0].active = true;
  hearts[0].spawnTime = millis();

  hearts[1].x = 6.0f; hearts[1].y = 44.0f;
  hearts[1].vx = -0.05f; hearts[1].vy = -0.40f;
  hearts[1].size = 0; hearts[1].active = true;
  hearts[1].spawnTime = millis() + 600;

  hearts[2].active = false;
  hearts[3].active = false;
}
void updateFloatingHearts() {
  unsigned long now = millis();
  for (int i = 0; i < 2; i++) {
    if (!hearts[i].active || now < hearts[i].spawnTime) continue;
    hearts[i].y += hearts[i].vy;
    hearts[i].x += hearts[i].vx + (sin(now / 200.0f + i) * 0.15f);
    if (hearts[i].y < -10.0f) hearts[i].active = false;
  }
}
void drawFloatingHearts() {
  unsigned long now = millis();
  for (int i = 0; i < 2; i++) {
    if (!hearts[i].active || now < hearts[i].spawnTime) continue;
    int ix = (int)hearts[i].x, iy = (int)hearts[i].y;
    display.drawBitmap(ix, iy, bmp_heart_small, 8, 8, SSD1306_WHITE);
  }
}
void triggerSparklesBurst(float cx, float cy) {
  for (int i = 0; i < 8; i++) {
    float angle = (i * 3.14159f / 4.0f) + (random(-2, 3) * 0.1f);
    sparkles[i].x = cx; sparkles[i].y = cy;
    sparkles[i].vx = cos(angle) * (1.1f + random(0, 5) / 10.0f);
    sparkles[i].vy = sin(angle) * (0.9f + random(0, 5) / 10.0f);
    sparkles[i].life = 35; sparkles[i].active = true;
  }
}
void triggerBirthdayCelebration() {
  setBehavior(BEHAVIOR_CELEBRATING, 5000, true);
  triggerSparklesBurst(64.0f, 28.0f);
  lastCelebrationTime = millis();
}
void updateSparkles() {
  for (int i = 0; i < 8; i++) {
    if (!sparkles[i].active) continue;
    sparkles[i].x += sparkles[i].vx; sparkles[i].y += sparkles[i].vy;
    if (sparkles[i].life > 0) sparkles[i].life--;
    if (sparkles[i].life == 0 || sparkles[i].x < 0 || sparkles[i].x > 127 ||
        sparkles[i].y < 0 || sparkles[i].y > 63) sparkles[i].active = false;
  }
}
void drawSparkles() {
  for (int i = 0; i < 8; i++) {
    if (!sparkles[i].active) continue;
    int sx = (int)sparkles[i].x, sy = (int)sparkles[i].y;
    display.drawPixel(sx, sy, SSD1306_WHITE);
    if (sparkles[i].life > 15) {
      display.drawPixel(sx + 1, sy, SSD1306_WHITE);
      display.drawPixel(sx, sy + 1, SSD1306_WHITE);
    }
  }
}
void updateRainOverlay() {
  if (!rainOverlayActive) return;
  for (int i = 0; i < RAIN_DROPS; i++) {
    if (!raindrops[i].active) continue;
    raindrops[i].y += raindrops[i].vy;
    raindrops[i].x += 0.3f * sin(millis() / 300.0f + i);
    if (raindrops[i].y > 64.0f) { raindrops[i].y = -4.0f; raindrops[i].x = random(0, SCREEN_WIDTH); }
  }
}
void drawRainOverlay() {
  if (!rainOverlayActive) return;
  for (int i = 0; i < RAIN_DROPS; i++) {
    if (!raindrops[i].active) continue;
    int rx = (int)raindrops[i].x, ry = (int)raindrops[i].y;
    display.drawLine(rx, ry, rx, ry + 3, SSD1306_WHITE);
    display.drawPixel(rx, ry + 4, SSD1306_WHITE);
  }
}
void startRainOverlay() {
  for (int i = 0; i < RAIN_DROPS; i++) {
    raindrops[i].x = random(0, SCREEN_WIDTH);
    raindrops[i].y = random(-40, 0);
    raindrops[i].vy = 1.6f + random(0, 20) / 20.0f;
    raindrops[i].active = true;
  }
  rainOverlayActive = true;
}
void stopRainOverlay() {
  rainOverlayActive = false;
  for (int i = 0; i < RAIN_DROPS; i++) raindrops[i].active = false;
}
void triggerLightning() { lightningActive = true; lightningStart = millis(); lightningFlashesRemaining = 3; }
void updateLightning() {
  if (!lightningActive) return;
  unsigned long elapsed = millis() - lightningStart;
  if (elapsed / LIGHTNING_FLASH_MS >= (unsigned long)(lightningFlashesRemaining * 2)) lightningActive = false;
}
void drawLightning() {
  if (!lightningActive) return;
  unsigned long elapsed = millis() - lightningStart;
  if ((elapsed / LIGHTNING_FLASH_MS) % 2 == 0) display.fillScreen(SSD1306_WHITE);
}

// 12. SLEEP, BIRTHDAYS, WEATHER EFFECTS
bool isScheduledSleepTime() {
  if (!ntpSynced) return false;
  time_t nowSec = time(nullptr);
  if (nowSec < 1700000000) return false;
  struct tm t;
  if (!getLocalTime(&t, 5)) return false;
  int mins = t.tm_hour * 60 + t.tm_min;
  int endMins = 360 - (int)personality.wakeEarliness;
  if (endMins < 330) endMins = 330;
  return (mins >= 60 && mins < endMins);
}
void checkBirthdayEligibility() {
  struct tm t;
  if (!getLocalTime(&t, 5)) return;
  uint32_t todayPacked = (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
  if (personality.lastCelebrationDay == todayPacked) return;
  bool isKibo = (t.tm_mon + 1 == 4 && t.tm_mday == 5);
  bool isUser = false;
  for (uint8_t i = 0; i < birthdayCount; i++)
    if (userBirthdays[i].month == (t.tm_mon + 1) && userBirthdays[i].day == t.tm_mday) { isUser = true; break; }
  if ((isKibo || isUser) && !isSleepingNow) {
    if (millis() - lastCelebrationTime > BIRTHDAY_COOLDOWN_MS) {
      triggerBirthdayCelebration();
      personality.lastCelebrationDay = todayPacked;
      personalityDirty = true;
    }
  }
}
void updateWeatherEffects() {
  unsigned long now = millis();
  bool isRainy = (weatherMain == "Rain" || weatherMain == "Drizzle" || weatherMain == "Thunderstorm");
  if (isRainy) {
    if (rainOverlayActive) {
      if (now >= rainShowerEndAt) {
        stopRainOverlay();
        nextRainShowerAt = now + random(300000UL, 480000UL); // 5-8 min pause between showers
      }
    } else {
      if (nextRainShowerAt == 0) nextRainShowerAt = now + random(10000UL, 25000UL);
      if (now >= nextRainShowerAt) {
        startRainOverlay();
        rainShowerEndAt = now + random(5000, 9000); // 5-9s shower duration
      }
    }
  } else {
    if (rainOverlayActive) stopRainOverlay();
    nextRainShowerAt = 0;
    rainShowerEndAt = 0;
  }
  static unsigned long nextLightningAt = 0;
  if (weatherMain == "Thunderstorm") {
    if (nextLightningAt == 0) nextLightningAt = now + random(300000UL, 480000UL);
    if (!lightningActive && now >= nextLightningAt) {
      triggerLightning();
      nextLightningAt = now + random(300000UL, 480000UL); // 5-8 min pause between lightning
    }
  } else {
    nextLightningAt = 0;
  }
}

// 13. FACE GESTURE BANK
static bool gestureWasRecent(FaceGesture g) { for (int i = 0; i < 5; i++) if (recentGestures[i] == g) return true; return false; }
static void rememberGesture(FaceGesture g) { recentGestures[recentGestureIdx] = g; recentGestureIdx = (recentGestureIdx + 1) % 5; }
static bool pickGestureFromPool(FaceGesture* out) {
  struct tm t;
  bool haveTime = getLocalTime(&t, 5);
  int hour = haveTime ? t.tm_hour : -1;
  bool mor = (hour >= 6 && hour < 12), aft = (hour >= 12 && hour < 17);
  bool eve = (hour >= 17 && hour < 21), nig = (hour >= 21 || hour < 6);
  bool hot = (temperature > 30.0f), cld = (temperature < 10.0f);
  bool rain = (weatherMain == "Rain" || weatherMain == "Drizzle"), storm = (weatherMain == "Thunderstorm");
  FaceGesture pool[20]; int n = 0;
  if (storm) { pool[n++] = FG_FEAR_SQUEEZE; pool[n++] = FG_WORRIED_LOOKUP; }
  if (rain) pool[n++] = FG_WORRIED_LOOKUP;
  if (hot) { pool[n++] = FG_DOZE_DIP; pool[n++] = FG_SLOW_BLINK; }
  if (cld) pool[n++] = FG_SHIVER;
  if (mor) { pool[n++] = FG_STRETCH; pool[n++] = FG_YAWN; pool[n++] = FG_LOOK_AROUND; pool[n++] = FG_CURIOUS_PEEK; }
  if (aft) { pool[n++] = FG_MICRO_FIDGET; pool[n++] = FG_LOOK_AROUND; pool[n++] = FG_DOZE_DIP; pool[n++] = FG_CONFUSED; }
  if (eve) { pool[n++] = FG_HAPPY_SQUINT; pool[n++] = FG_CONTENT_WIGGLE; pool[n++] = FG_SLOW_BLINK; pool[n++] = FG_LAUGH; }
  if (nig) { pool[n++] = FG_SLOW_BLINK; pool[n++] = FG_YAWN; pool[n++] = FG_DOZE_DIP; }
  pool[n++] = FG_LOOK_AROUND; pool[n++] = FG_LOOK_AROUND;
  pool[n++] = FG_MICRO_FIDGET; pool[n++] = FG_SLOW_BLINK;
  if (personality.irritability > 45) pool[n++] = FG_SIDE_EYE;
  if (personality.contentment > 60) { pool[n++] = FG_LAUGH; pool[n++] = FG_HEART_EYES; }
  if (personality.curiosity > 55 || personality.familiarity > 25) {
    pool[n++] = FG_CONFUSED; pool[n++] = FG_CURIOUS_PEEK;
  }
  if (random(0, 100) < 18) {
    const FaceGesture rare[] = { FG_WINK, FG_HEART_EYES, FG_CONFUSED, FG_SHOCK };
    pool[n++] = rare[random(0, 4)];
  }
  for (int k = 0; k < 6; k++) {
    FaceGesture g = pool[random(0, n)];
    if (!gestureWasRecent(g)) { *out = g; return true; }
  }
  *out = pool[random(0, n)]; return true;
}
static unsigned long gestureDurationFor(FaceGesture g) {
  switch (g) {
    case FG_STRETCH: return 2000; case FG_YAWN: return 1500;
    case FG_LOOK_AROUND: return 3000; case FG_DOZE_DIP: return 2000;
    case FG_SHIVER: return 1000; case FG_SQUINT_UV: return 3000;
    case FG_HAPPY_SQUINT: return 2000; case FG_WORRIED_LOOKUP: return 2200;
    case FG_FEAR_SQUEEZE: return 1000; case FG_CONTENT_WIGGLE: return 2000;
    case FG_MICRO_FIDGET: return 500; case FG_SLOW_BLINK: return 1500;
    case FG_SIDE_EYE: return 1700; case FG_CONFUSED: return 1800;
    case FG_WINK: return 650; case FG_LAUGH: return 1200;
    case FG_HEART_EYES: return 1800; case FG_CURIOUS_PEEK: return 1600;
    case FG_SHOCK: return 700;
    default: return 1000;
  }
}
void updateGestureBank() {
  unsigned long now = millis();
  if (currentPage == 0 && !isSleepingNow && !isDozedNow) {
    if (ambientArtUntil != 0 && now >= ambientArtUntil) {
      ambientArt = 0;
      ambientArtUntil = 0;
    }
    if (nextAmbientArtAt == 0) nextAmbientArtAt = now + random(12000UL, 26000UL);
    if (ambientArt == 0 && now >= nextAmbientArtAt) {
      ambientArt = random(1, 6);
      ambientArtUntil = now + random(1200UL, 2300UL);
      nextAmbientArtAt = ambientArtUntil + random(35000UL, 85000UL);
    }
  } else if (ambientArt != 0) {
    ambientArt = 0;
    ambientArtUntil = 0;
  }
  if (currentPage != 0 || isSleepingNow || behaviorState != BEHAVIOR_AWAKE || isBeingPetted) return;
  if (currentGesture == FG_NONE) {
    if (now >= nextGestureRoll) {
      FaceGesture g;
      if (pickGestureFromPool(&g)) {
        currentGesture = g; gestureStartedAt = now;
        gestureDurationMs = gestureDurationFor(g);
        rememberGesture(g);
      }
      nextGestureRoll = now + random(GESTURE_ROLL_MIN_MS, GESTURE_ROLL_MAX_MS);
    }
  } else {
    if (now - gestureStartedAt >= gestureDurationMs) {
      currentGesture = FG_NONE;
      leftEye.targetPupilX = rightEye.targetPupilX = 0;
      leftEye.targetPupilY = rightEye.targetPupilY = 0;
    }
  }
}

static uint8_t teaseStreak = 0;
static bool teaseFaintRecoveryPending = false;
static BehaviorState lastIdleMood = BEHAVIOR_AWAKE;

static BehaviorState chooseNaturalIdleMood() {
  int roll = random(0, 100);
  BehaviorState choice = BEHAVIOR_CONTENT;
  if (personality.energy < 25) {
    choice = roll < 55 ? BEHAVIOR_SLEEPY : BEHAVIOR_LOW_ENERGY;
  } else if (personality.irritability > 55) {
    if (roll < 35) choice = BEHAVIOR_FED_UP;
    else if (roll < 65) choice = BEHAVIOR_IGNORING;
    else choice = BEHAVIOR_ANNOYED;
  } else if (personality.contentment > 72) {
    if (roll < 35) choice = BEHAVIOR_CONTENT;
    else if (roll < 65) choice = BEHAVIOR_HAPPY;
    else choice = BEHAVIOR_PLAYFUL;
  } else if (personality.curiosity > 60 || personality.familiarity > 100) {
    choice = roll < 55 ? BEHAVIOR_CURIOUS : BEHAVIOR_THINKING;
  } else {
    if (roll < 25) choice = BEHAVIOR_CURIOUS;
    else if (roll < 45) choice = BEHAVIOR_HAPPY;
    else if (roll < 65) choice = BEHAVIOR_SHY;
    else if (roll < 82) choice = BEHAVIOR_DONT_CARE;
    else choice = BEHAVIOR_CONTENT;
  }
  if (choice == lastIdleMood) {
    if (choice == BEHAVIOR_CONTENT) choice = BEHAVIOR_CURIOUS;
    else if (choice == BEHAVIOR_CURIOUS) choice = BEHAVIOR_HAPPY;
    else choice = BEHAVIOR_CONTENT;
  }
  lastIdleMood = choice;
  return choice;
}

void triggerTeaseReaction() {
  // Let one dizzy/faint sequence finish; repeated taps cannot restart it.
  if (behaviorState == BEHAVIOR_DIZZY || isDozedNow) return;
  unsigned long now = millis();
  teaseStreak = 5;

  personality.irritability = (uint8_t)min(100, (int)personality.irritability + 12);
  personality.energy = (personality.energy > 5) ? personality.energy - 5 : 0;
  personalityDirty = true;
  lastFaceActiveAt = now;
  lastInteractionMillis = now;
  setBehavior(BEHAVIOR_DIZZY, 3500, true);
  DBG("[TEASE] rapid burst -> dizzy faint\n");
}

static void enterTripleTapSleep(unsigned long now) {
  pendingThreeTapSleep = false;
  if (isSleepingNow || isDozedNow || scheduledSleepActive) return;
  savePersonality(true);
  isSleepingNow = true;
  isDozedNow = false;
  teaseFaintRecoveryPending = false;
  scheduledSleepActive = false;
  sleepEnteredAt = now;
  returnToFacePage();
  tapCounter = 0;
  rapidTapWindowCount = 0;
  rapidTapBurstOriginPage = -1;
  setBehavior(BEHAVIOR_SLEEPING, 0, true);
  DBG("[TOUCH] delayed 3tap sleep\n");
}

// 14. TOUCH INPUT  (REWRITTEN: rising edge = tap, duration decides tap vs hold)
void handleTouchInput() {
  static bool lastStable = false;
  static unsigned long lastEdge = 0;

  const bool raw = TOUCH_ACTIVE_HIGH ? (digitalRead(TOUCH_PIN) == HIGH)
                                     : (digitalRead(TOUCH_PIN) == LOW);
  const unsigned long now = millis();

  if (now - bootMillis < BOOT_TOUCH_BLACKOUT_MS) return;
  if (isSleepingNow && behaviorState == BEHAVIOR_SLEEPING &&
      (now - sleepEnteredAt < SLEEP_ENTRY_GUARD_MS)) return;

  // Wait out the burst window before treating three taps as sleep, so tap four
  // still gets a chance to complete the tease/faint sequence.
  if (pendingThreeTapSleep && rapidTapBurstOriginPage == 0 && rapidTapWindowCount == 3 &&
      (now - rapidTapWindowStart > RAPID_TAP_WINDOW_MS)) {
    enterTripleTapSleep(now);
    return;
  }

  // Debounced rising edge
  if (raw != lastStable) {
    if (now - lastEdge >= TOUCH_DEBOUNCE_MS) {
      lastEdge = now;
      bool rising = (raw == true);

      if (rising) {
        // Touch begins
        pressStartTime = now;
        isLongPressHandled = false;
        isBeingPetted = false;
        gameTouchPressedBeforeGo = currentPage == 5 && gameRunning &&
          selectedGame == GAME_BLINK_REFLEX && gamePhase == GAME_READY;
        lastInteractionMillis = now;
        if (currentPage == 0) lastFaceActiveAt = now;
        time_t touchEpoch = time(nullptr);
        awaySecondsAtLastTouch = 0;
        if (touchEpoch > 1600000000) {
          uint32_t currentEpoch = (uint32_t)touchEpoch;
          if (personality.lastTouchEpoch > 0 && currentEpoch >= personality.lastTouchEpoch)
            awaySecondsAtLastTouch = currentEpoch - personality.lastTouchEpoch;
          personality.lastTouchEpoch = (uint32_t)touchEpoch;
          personality.lastSadStreakEpoch = 0;
          personalityDirty = true;
          pendingTouchEpochUpdate = false;
        } else {
          pendingTouchEpochUpdate = true;
        }
        if (manualWakeUntil != 0) lastManualWakeActivity = now;

        DBG("[TOUCH] down t=%lu page=%d sleep=%d\n", now, currentPage, isSleepingNow);

        // Wake from sleep
        if (isSleepingNow || behaviorState == BEHAVIOR_SLEEPING) {
          manualWakeUntil = now + 300000UL;
          lastManualWakeActivity = now;
          isSleepingNow = false;
          isDozedNow = false;
          scheduledSleepActive = false;
          behaviorFallback = BEHAVIOR_AWAKE;
          returnToFacePage();
          tapCounter = 0;
          rapidTapWindowCount = 0;
          rapidTapWindowStart = now;
          isLongPressHandled = true;
          setBehavior(BEHAVIOR_WAKING, 2200, true);
          DBG("[TOUCH] wake from sleep\n");
          lastStable = raw;
          return;
        }
        if (screenOffDueToIdle) {
          screenOffDueToIdle = false;
          returnToFacePage();
          setBehavior(BEHAVIOR_SURPRISED, 900, true);
        } else if (currentPage == 0 && behaviorState == BEHAVIOR_SAD) {
          sadStreakUntil = 0;
          behaviorFallback = BEHAVIOR_AWAKE;
          setBehavior(BEHAVIOR_RECOVERING, 1800, true);
        }
      } else {
        // Touch released
        unsigned long held = now - pressStartTime;
        DBG("[TOUCH] up held=%lu\n", held);

        if (isBeingPetted) {
          isBeingPetted = false;
          uint8_t irritationBeforePet = personality.irritability;
          personality.petCount++;
          personality.contentment = (uint8_t)min(100, (int)personality.contentment + 25);
          if (personality.irritability > 15) personality.irritability -= 15;
          personality.familiarity = (uint16_t)min(9999, (int)personality.familiarity + 3);
          personalityDirty = true;
          if (isDozedNow && personality.energy >= 60) {
            isDozedNow = false;
            teaseFaintRecoveryPending = false;
            dozeEnteredAt = 0;
            triggerSparklesBurst(64.0f, 24.0f);
            setBehavior(BEHAVIOR_HAPPY, 3000, true);
            DBG("[DOZE] woke up after petting\n");
          } else if (!isDozedNow && !petWokeFromDoze) {
            triggerSparklesBurst(64.0f, 24.0f);
            if (personality.petCount % 10 == 0) {
              setBehavior(BEHAVIOR_CELEBRATING, 3000, true);
              DBG("[PET] tenth-pet milestone\n");
            } else if (irritationBeforePet >= 60) {
              setBehavior(BEHAVIOR_RECOVERING, 1800, true);
              DBG("[PET] soothed after irritation\n");
            } else if (personality.familiarity < 12) {
              setBehavior(BEHAVIOR_SHY, 2200, true);
              DBG("[PET] shy with a new friend\n");
            } else if (personality.familiarity >= 30 && personality.contentment >= 80) {
              setBehavior(BEHAVIOR_OVER_EXCITED, 3000, true);
              DBG("[PET] familiar joyful reaction\n");
            } else if (personality.contentment >= 55) {
              setBehavior(BEHAVIOR_HAPPY, 2600, true);
            } else {
              setBehavior(BEHAVIOR_CONTENT, 2200, true);
            }
          }
          petWokeFromDoze = false;
        } else if (!isLongPressHandled) {
          if (behaviorState == BEHAVIOR_DIZZY) {
            DBG("[TEASE] tap ignored during faint animation\n");
          } else if (isDozedNow && teaseFaintRecoveryPending) {
            DBG("[TEASE] tap ignored during faint recovery; pet to wake sooner\n");
          } else if (isDozedNow) {
            isDozedNow = false;
            teaseFaintRecoveryPending = false;
            dozeEnteredAt = 0;
            lastFaceActiveAt = now;
            lastInteractionMillis = now;
            setBehavior(BEHAVIOR_GROGGY, 2200, true);
            DBG("[DOZE] tap while dozed -> wakes groggy\n");
          } else {
            if (currentPage == 5 && gameRunning) {
              personality.totalTouches++;
              personalityDirty = true;
              handleGameTap();
              DBG("[GAME] tap input\n");
            } else {
              tapCounter++;
              lastTapTime = now;
              DBG("[TOUCH] tap++ =%d\n", tapCounter);
              if (rapidTapWindowCount == 0 || now - rapidTapWindowStart > RAPID_TAP_WINDOW_MS) {
                rapidTapWindowStart = now; rapidTapWindowCount = 0;
                rapidTapBurstOriginPage = currentPage;
              }
              rapidTapWindowCount++;
              if (rapidTapWindowCount == 2 && rapidTapBurstOriginPage == 0 && currentPage != 0) {
                returnToFacePage();
                tapCounter = 0;
              }
              if (rapidTapBurstOriginPage == 0 && rapidTapWindowCount == 2 &&
                  !isSleepingNow && !isDozedNow && behaviorState != BEHAVIOR_DIZZY) {
                setBehavior(BEHAVIOR_PLAYFUL, 1400, true);
              }
              if (rapidTapBurstOriginPage == 0 && rapidTapWindowCount == 3 &&
                  !isSleepingNow && !isDozedNow && behaviorState != BEHAVIOR_DIZZY) {
                pendingThreeTapSleep = true;
                tapCounter = 0;
                if (currentPage != 0) returnToFacePage();
                teaseStreak = 3;
                personality.irritability = (uint8_t)min(100, (int)personality.irritability + 8);
                personalityDirty = true;
                setBehavior(BEHAVIOR_FED_UP, 1500, true);
                DBG("[TEASE] 3-tap burst -> fed up; waiting for tap four\n");
              }
              if (rapidTapBurstOriginPage == 0 && rapidTapWindowCount >= RAPID_TAP_BURST_THRESHOLD &&
                  !isSleepingNow && currentPage != 5) {
                pendingThreeTapSleep = false;
                rapidTapWindowCount = 0; rapidTapBurstOriginPage = -1; tapCounter = 0;
                if (currentPage != 0) returnToFacePage();
                triggerTeaseReaction();
              }
            }
          }
        }
      }
      lastStable = raw;
    }
  }

  // Holding the touch keeps Flappy moving with gentle, spaced flaps. A longer
  // hold still exits the game, so this remains usable with a single touch pad.
  if (lastStable && currentPage == 5 && gameRunning &&
      selectedGame == GAME_FLAPPY && gamePhase == GAME_PLAYING &&
      now - pressStartTime >= 260UL &&
      (gameLastFlapAt == 0 || now - gameLastFlapAt >= 300UL)) {
    handleGameTap();
  }

  // Continuous hold -> petting (face only, awake or dozed)
  if (lastStable && !isSleepingNow && currentPage == 0) {
    unsigned long holdMs = now - pressStartTime;
    if (!isBeingPetted && !isLongPressHandled && holdMs >= PETTING_HOLD_MS) {
      isBeingPetted = true;
      petWokeFromDoze = false;
      isLongPressHandled = true;
      petStartTime = now;
      lastHeartSpawn = now;
      tapCounter = 0;
      rapidTapWindowCount = 0;
      rapidTapBurstOriginPage = -1;
      pendingThreeTapSleep = false;
      if (!isDozedNow) setBehavior(BEHAVIOR_CONTENT, 0, true);
      spawnHeartsBurst();
      DBG("[TOUCH] petting start\n");
    }
    if (isBeingPetted) {
      if (now - lastHeartSpawn > 1400) {
        lastHeartSpawn = now;
        spawnHeartsBurst();
      }
      if (isDozedNow) {
        static unsigned long lastPetCharge = 0;
        if (now - lastPetCharge > 350) {
          lastPetCharge = now;
          personality.energy = (uint8_t)min(100, (int)personality.energy + 12);
          personalityDirty = true;
          DBG("[DOZE] fast-charging via pet: energy=%u\n", personality.energy);
          if (personality.energy >= 70) {
            isDozedNow = false;
            teaseFaintRecoveryPending = false;
            dozeEnteredAt = 0;
            petWokeFromDoze = true;
            lastFaceActiveAt = now;
            lastInteractionMillis = now;
            triggerSparklesBurst(64.0f, 24.0f);
            setBehavior(BEHAVIOR_EXCITED, 3000, true);
            DBG("[DOZE] fast-charge complete! Woke up excited\n");
          }
        }
      }
    }
  }

  // Continuous hold on non-face page -> long-press actions
  if (lastStable && !isLongPressHandled && !isSleepingNow && currentPage != 0) {
    unsigned long holdMs = now - pressStartTime;
    unsigned long longPressThreshold = (currentPage == 5 && gameRunning) ? 1200UL : WORLD_CLOCK_LONGPRESS_MS;
    if (holdMs >= longPressThreshold) {
      isLongPressHandled = true;
      tapCounter = 0;
      if (currentPage == 1) {
        if (!ntpSynced) {
          wifiStaStart = now;
          lastNtpAttempt = 0;
          DBG("[TOUCH] time hold -> force immediate wifi scan & sync\n");
        } else {
          currentPage = 4; DBG("[TOUCH] time->world\n");
        }
      } else if (currentPage == 2 || currentPage == 3) {
        if (weatherCityCount > 1) {
          uint8_t nextCity = (currentCityIndex + 1) % weatherCityCount;
          selectWeatherCity(nextCity, now);
          DBG("[TOUCH] switched city -> %s\n", city.c_str());
        } else {
          citySwitchNotificationUntil = now + 1200;
          DBG("[TOUCH] single city -> %s\n", city.c_str());
        }
      } else if (currentPage == 4) {
        currentPage = 1; DBG("[TOUCH] world->time\n");
      } else if (currentPage == 5) {
        if (gameRunning) {
          gameRunning = false;
          gamePhase = GAME_MENU;
          if (selectedGame == GAME_FLAPPY) {
            if (gameScore >= 5) setBehavior(BEHAVIOR_CELEBRATING, 3200, true);
            else if (gameScore >= 2) setBehavior(BEHAVIOR_HAPPY, 2600, true);
            else setBehavior(BEHAVIOR_CURIOUS, 2200, true);
          } else if (gameReactionMs >= 0 && gameReactionMs <= 250) {
            setBehavior(BEHAVIOR_EXCITED, 2800, true);
          } else {
            setBehavior(BEHAVIOR_CONTENT, 2400, true);
          }
          currentPage = 5;
          DBG("[GAME] exit to arcade menu\n");
        } else {
          returnToFacePage();
          DBG("[GAME] leave game shelf\n");
        }
      }
    }
  }

  // Tap resolution after window closes
  if (tapCounter > 0 && (now - lastTapTime > TAP_WINDOW_MS)) {
    int taps = tapCounter;
    tapCounter = 0;
    if (isSleepingNow || behaviorState == BEHAVIOR_SLEEPING || isDozedNow) return;
    personality.totalTouches += taps;
    if (currentPage == 0) {
      personality.curiosity = (uint8_t)min(100, (int)personality.curiosity + min(taps, 2));
    }
    personalityDirty = true;
    DBG("[TOUCH] resolve taps=%d page=%d\n", taps, currentPage);

    if (taps == 1) {
      if (currentPage == 0) {
        if (awaySecondsAtLastTouch >= 300) {
          doubleTapInteractive = false;
          if (awaySecondsAtLastTouch >= 3600) playReactionBasedOnContext(3600001UL);
          else playReactionBasedOnContext(300001UL);
          DBG("[MEMORY] welcome back after %lu sec\n", (unsigned long)awaySecondsAtLastTouch);
        } else {
          doubleTapInteractive = true;
          triggerSparklesBurst(64.0f, 24.0f);
          setBehavior(BEHAVIOR_SURPRISED, 700, true);
          DBG("[TOUCH] single tap play interaction\n");
        }
      }
      else if (currentPage == 1) {
        analogClock = !analogClock;
        DBG("[TOUCH] time display analog=%d\n", analogClock);
      }
      else if (currentPage == 2) {
        useFahrenheit = !useFahrenheit;
        DBG("[TOUCH] weather units F=%d\n", useFahrenheit);
      }
      else if (currentPage == 4) currentPage = 1;
      else if (currentPage == 5) {
        if (!gameRunning) startSelectedGame();
        else handleGameTap();
      }
      DBG("[TOUCH] -> page %d\n", currentPage);
    } else if (taps == 2) {
      if (currentPage == 0) {
        currentPage = 1;
        DBG("[TOUCH] double tap -> time page\n");
      }
      else if (currentPage == 1) { currentPage = 2; DBG("[TOUCH] time -> weather\n"); }
      else if (currentPage == 2) { currentPage = 3; DBG("[TOUCH] weather -> forecast\n"); }
      else if (currentPage == 3) { currentPage = 5; DBG("[TOUCH] forecast -> arcade\n"); }
      else if (currentPage == 5 && !gameRunning) {
        selectedGame = (MiniGame)((selectedGame + 1) % GAME_COUNT);
        DBG("[GAME] selected %u\n", (uint8_t)selectedGame);
      }
    } else if (taps == 3 && rapidTapWindowCount < 4) {
      if (currentPage != 0) {
        if (currentPage == 5) { gameRunning = false; gamePhase = GAME_MENU; }
        returnToFacePage(); DBG("[TOUCH] 3tap -> face\n");
      }
    }
  }
}

// 15. BEHAVIOUR ENGINE
void updateBehaviorEngine() {
  unsigned long now = millis();
  bool sched = isScheduledSleepTime();
  if (manualWakeUntil != 0) {
    if (lastManualWakeActivity != 0 && (now - lastManualWakeActivity < 300000UL))
      manualWakeUntil = lastManualWakeActivity + 300000UL;
    if ((long)(manualWakeUntil - now) <= 0) manualWakeUntil = 0;
  }
  bool manual = (manualWakeUntil != 0);
  if (sched && !manual) {
    if (gameRunning) { gameRunning = false; gamePhase = GAME_MENU; }
    if (behaviorState != BEHAVIOR_SLEEPING) {
      savePersonality(true);
      setBehavior(BEHAVIOR_SLEEPING, 0, true);
      isSleepingNow = true; isDozedNow = false; dozeEnteredAt = 0;
      sleepEnteredAt = now;
    }
    scheduledSleepActive = true;
    isSleepingNow = true; currentPage = 0; return;
  }
  if (!sched && scheduledSleepActive) {
    scheduledSleepActive = false;
    isSleepingNow = false;
    isDozedNow = false;
    behaviorFallback = BEHAVIOR_AWAKE;
    returnToFacePage();
    setBehavior(BEHAVIOR_WAKING, 2200, true);
    DBG("[SLEEP] scheduled wake\n");
    return;
  }
  if (behaviorState == BEHAVIOR_WAKING && (now - behaviorEnteredAt >= behaviorDurationMs)) {
    setBehavior(BEHAVIOR_GROGGY, GROGGY_DURATION_MS, true); return;
  }
  if (behaviorState == BEHAVIOR_GROGGY && (now - behaviorEnteredAt >= behaviorDurationMs)) {
    behaviorFallback = BEHAVIOR_AWAKE; setBehavior(BEHAVIOR_AWAKE, 0, true); return;
  }
  if (isTemporaryBehavior(behaviorState) && behaviorDurationMs > 0 &&
      (now - behaviorEnteredAt >= behaviorDurationMs)) {
    if (behaviorState == BEHAVIOR_SURPRISED && doubleTapInteractive) {
      doubleTapInteractive = false;
      triggerSparklesBurst(64.0f, 24.0f);
      setBehavior(BEHAVIOR_EXCITED, 2800, true);
    } else if (behaviorState == BEHAVIOR_EXCITED || behaviorState == BEHAVIOR_OVER_EXCITED) {
      setBehavior(BEHAVIOR_HAPPY, 2500, true);
    } else if (behaviorState == BEHAVIOR_PLAYFUL) {
      setBehavior(BEHAVIOR_CONTENT, 2000, true);
    } else if (behaviorState == BEHAVIOR_DIZZY) {
      bool teaseFaint = teaseStreak >= 5;
      isDozedNow = true;
      dozeEnteredAt = now;
      teaseFaintRecoveryPending = teaseFaint;
      personality.energy = 10;
      personalityDirty = true;
      teaseStreak = 0;
      currentPage = 0;
      setBehavior(BEHAVIOR_LOW_ENERGY, 0, true);
      DBG("[BEHAVIOR] dizzy done -> dozed up!\n");
    } else if (behaviorState == BEHAVIOR_ANNOYED) {
      if (teaseStreak >= 3) setBehavior(BEHAVIOR_FED_UP, 1600, true);
      else setBehavior(BEHAVIOR_RECOVERING, 1000, true);
    } else if (behaviorState == BEHAVIOR_FED_UP) {
      if (teaseStreak >= 4) setBehavior(BEHAVIOR_IGNORING, 1800, true);
      else setBehavior(BEHAVIOR_RECOVERING, 1000, true);
    } else if (behaviorState == BEHAVIOR_IGNORING || behaviorState == BEHAVIOR_DONT_CARE || behaviorState == BEHAVIOR_ANGRY) {
      setBehavior(BEHAVIOR_RECOVERING, 1200, true);
    } else {
      if (behaviorState != BEHAVIOR_RECOVERING && behaviorFallback != BEHAVIOR_SLEEPING) {
        setBehavior(BEHAVIOR_RECOVERING, 650, true);
      } else {
        if (behaviorFallback == BEHAVIOR_SLEEPING) behaviorFallback = BEHAVIOR_AWAKE;
        setBehavior(behaviorFallback, 0, true);
      }
    }
  }
  if (lastEnergyTick == 0) lastEnergyTick = now;
  if (now - lastEnergyTick >= 60000UL) {
    lastEnergyTick = now;
    if (personality.irritability > 5) personality.irritability -= 5;
    if (personality.energy > 0 && !isSleepingNow) personality.energy--;
    if (isSleepingNow && personality.energy < 100) personality.energy += 3;
    personalityDirty = true;
  }
  if (sadStreakUntil > 0 && now >= sadStreakUntil) sadStreakUntil = 0;

  if (isDozedNow && !isSleepingNow) {
    static unsigned long lastSelfChargeTick = 0;
    if (now - lastSelfChargeTick >= 2500UL) {
      lastSelfChargeTick = now;
      if (personality.energy < 100) {
        personality.energy = (uint8_t)min(100, (int)personality.energy + 4);
        personalityDirty = true;
      }
    }
  }

  if (isDozedNow && teaseFaintRecoveryPending &&
      now - dozeEnteredAt >= TEASE_FAINT_RECOVERY_MS) {
    isDozedNow = false;
    teaseFaintRecoveryPending = false;
    personality.energy = (uint8_t)max(50, (int)personality.energy);
    personality.irritability = personality.irritability > 20 ? personality.irritability - 20 : 0;
    personalityDirty = true;
    lastFaceActiveAt = now;
    lastInteractionMillis = now;
    behaviorFallback = BEHAVIOR_AWAKE;
    setBehavior(BEHAVIOR_GROGGY, GROGGY_DURATION_MS, true);
    DBG("[TEASE] recovered from faint\n");
  }

  if (currentPage == 0 && !isSleepingNow) {
    unsigned long idleOnFace = now - lastFaceActiveAt;
    if (!isDozedNow && idleOnFace >= DOZE_IDLE_MS) {
      isDozedNow = true; dozeEnteredAt = now; DBG("[DOZE] enter\n");
    } else if (isDozedNow) {
      if (now - dozeEnteredAt >= DOZE_TO_SLEEP_MS) {
        savePersonality(true);
        isDozedNow = false;
        isSleepingNow = true;
        sleepEnteredAt = now;
        setBehavior(BEHAVIOR_SLEEPING, 0, true);
        DBG("[DOZE] -> sleep\n");
      }
    }
  } else if (!isDozedNow) isDozedNow = false;

  if (currentPage != 0 && !isSleepingNow) {
    if (now - lastInteractionMillis > 60000UL) {
      returnToFacePage(); DBG("[UI] non-face idle -> back to face\n");
      lastFaceActiveAt = now - DOZE_IDLE_MS;
    }
  }

  if (currentPage == 0 && !isSleepingNow && !isDozedNow && behaviorState == BEHAVIOR_AWAKE) {
    unsigned long idle = now - lastInteractionMillis;
    if (idle > 20000UL && (now - lastFidgetTime > 25000UL)) {
      lastFidgetTime = now;
      BehaviorState idleMood = chooseNaturalIdleMood();
      unsigned long moodDuration = random(2400UL, 3900UL);
      if (idleMood == BEHAVIOR_PLAYFUL && random(0, 100) < 45)
        triggerSparklesBurst(64.0f, 24.0f);
      setBehavior(idleMood, moodDuration);
    }
    if (personality.energy < 25 && (now - lastYawnTime > YAWN_COOLDOWN_MS)) {
      if (random(0, 100) < 15) { lastYawnTime = now; setBehavior(BEHAVIOR_YAWNING, YAWN_DURATION_MS, true); }
    }
    if (personality.lastTouchEpoch > 0) {
      time_t nowEpoch = time(nullptr);
      if (nowEpoch > 1600000000 && nowEpoch >= personality.lastTouchEpoch) {
        uint32_t awaySeconds = (uint32_t)nowEpoch - personality.lastTouchEpoch;
        uint32_t sinceSadSeconds = personality.lastSadStreakEpoch == 0 || nowEpoch < personality.lastSadStreakEpoch
          ? 0xFFFFFFFFUL : (uint32_t)nowEpoch - personality.lastSadStreakEpoch;
        if (awaySeconds > (SAD_STREAK_TRIGGER_MS / 1000) &&
            sinceSadSeconds > (SAD_STREAK_TRIGGER_MS / 1000) && sadStreakUntil == 0) {
          sadStreakUntil = now + SAD_STREAK_DURATION_MS;
          personality.lastSadStreakEpoch = (uint32_t)nowEpoch;
          personalityDirty = true;
          setBehavior(BEHAVIOR_SAD, SAD_STREAK_DURATION_MS, true);
        }
      }
    }
  }
  checkBirthdayEligibility();
  updateWeatherEffects();
}

// 16. RENDERING
void drawEyelidMask(float x, float y, float w, float h, BehaviorState s, bool isLeft) {
  int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
  if (isBeingPetted) {
    display.fillRect(ix, iy + ih - 5, iw, 6, SSD1306_BLACK);
    display.fillCircle(ix + iw / 2, iy + ih + 10, iw * 0.60f, SSD1306_BLACK);
    return;
  }
  if (isDozedNow) {
    float dozePhase = ((millis() - dozeEnteredAt) % 3600) / 3600.0f;
    float coverRatio = 0.52f;
    if (dozePhase < 0.65f) {
      float p = dozePhase / 0.65f;
      coverRatio = 0.52f + 0.38f * (p * p);
    } else if (dozePhase < 0.78f) {
      float p = (dozePhase - 0.65f) / 0.13f;
      coverRatio = 0.90f - 0.38f * p;
    }
    float dozeEntry = constrain((float)(millis() - dozeEnteredAt) / 1000.0f, 0.0f, 1.0f);
    int coverH = (int)(ih * coverRatio * dozeEntry);
    display.fillRect(ix, iy, iw, coverH, SSD1306_BLACK);
    display.fillCircle(ix + iw / 2, iy + coverH - 47, 50, SSD1306_BLACK);
  } else if (s == BEHAVIOR_ANGRY) {
    if (isLeft) for (int i = 0; i < 14; i++) display.drawLine(ix, iy - 6 + i, ix + iw, iy + 6 + i, SSD1306_BLACK);
    else for (int i = 0; i < 14; i++) display.drawLine(ix, iy + 6 + i, ix + iw, iy - 6 + i, SSD1306_BLACK);
  } else if (s == BEHAVIOR_ANNOYED) {
    display.fillRect(ix, iy, iw, ih / 3 + 1, SSD1306_BLACK);
  } else if (s == BEHAVIOR_SAD || s == BEHAVIOR_CRYING) {
    if (isLeft) for (int i = 0; i < 14; i++) display.drawLine(ix, iy + 6 + i, ix + iw, iy - 6 + i, SSD1306_BLACK);
    else for (int i = 0; i < 14; i++) display.drawLine(ix, iy - 6 + i, ix + iw, iy + 6 + i, SSD1306_BLACK);
  } else if (s == BEHAVIOR_HAPPY || s == BEHAVIOR_CONTENT || s == BEHAVIOR_PLAYFUL ||
             s == BEHAVIOR_CELEBRATING || s == BEHAVIOR_EXCITED || s == BEHAVIOR_OVER_EXCITED) {
    display.fillRect(ix, iy + ih - 10, iw, 12, SSD1306_BLACK);
    display.fillCircle(ix + iw / 2, iy + ih + 6, iw / 1.3f, SSD1306_BLACK);
  } else if (s == BEHAVIOR_SLEEPY || s == BEHAVIOR_GROGGY) {
    display.fillRect(ix, iy, iw, ih / 2 + 2, SSD1306_BLACK);
  } else if (s == BEHAVIOR_LOW_ENERGY) {
    int coverH = (int)(ih * 0.56f);
    display.fillRect(ix, iy, iw, coverH, SSD1306_BLACK);
    display.fillCircle(ix + iw / 2, iy + coverH - 47, 50, SSD1306_BLACK);
  } else if (s == BEHAVIOR_DONT_CARE) {
    display.fillRect(ix, iy, iw, 6, SSD1306_BLACK);
  } else if (s == BEHAVIOR_FED_UP) {
    display.fillRect(ix, iy, iw, ih / 3, SSD1306_BLACK);
    display.fillRect(ix, iy + (2 * ih) / 3, iw, ih / 3 + 1, SSD1306_BLACK);
  } else if (s == BEHAVIOR_SHY) {
    if (isLeft) for (int i = 0; i < 8; i++) display.drawLine(ix, iy + i, ix + iw, iy + 3 + i, SSD1306_BLACK);
    else for (int i = 0; i < 8; i++) display.drawLine(ix, iy + 3 + i, ix + iw, iy + i, SSD1306_BLACK);
    display.fillRect(ix, iy + ih - 6, iw, 8, SSD1306_BLACK);
  } else if (s == BEHAVIOR_IGNORING) {
    display.fillRect(ix, iy, iw, ih / 2, SSD1306_BLACK);
  } else if (s == BEHAVIOR_CURIOUS) {
    if (isLeft) display.fillRect(ix, iy, iw, 4, SSD1306_BLACK);
    else display.fillRect(ix, iy + ih - 6, iw, 6, SSD1306_BLACK);
  }
}
void drawEye(bool isLeft) {
  Eye& e = isLeft ? leftEye : rightEye;
  int ix = (int)e.x, iy = (int)e.y, iw = (int)e.w, ih = (int)e.h;
  if (ix < 0) ix = 0; if (iy < 0) iy = 0;
  if (ix + iw > SCREEN_WIDTH) iw = SCREEN_WIDTH - ix;
  if (iy + ih > SCREEN_HEIGHT) ih = SCREEN_HEIGHT - iy;
  if (iw < 4 || ih < 2) return;
  int r = (iw < 20) ? 3 : 8;
  display.fillRoundRect(ix, iy, iw, ih, r, SSD1306_WHITE);
  int cx = ix + iw / 2, cy = iy + ih / 2;
  int pw = iw / 2; if (pw < 2) pw = 2;
  int ph = ih / 2; if (ph < 2) ph = 2;
  int px = cx + (int)e.pupilX - (pw / 2);
  int py = cy + (int)e.pupilY - (ph / 2);
  if (px < ix + 1) px = ix + 1;
  if (px + pw > ix + iw - 1) px = ix + iw - 1 - pw;
  if (py < iy + 1) py = iy + 1;
  if (py + ph > iy + ih - 1) py = iy + ih - 1 - ph;
  display.fillRoundRect(px, py, pw, ph, r / 2, SSD1306_BLACK);
  if (iw > 16 && ih > 16) display.fillCircle(px + pw - 4, py + 4, 2, SSD1306_WHITE);
  drawEyelidMask(ix, iy, iw, ih, behaviorState, isLeft);
}

void updatePhysics() {
  unsigned long now = millis();
  breathVal = sin(now / 800.0f) * 1.5f;
  if (behaviorState == BEHAVIOR_GROGGY) {
    leftEye.k = 0.05f; leftEye.d = 0.80f; rightEye.k = 0.05f; rightEye.d = 0.80f;
  } else {
    leftEye.k = 0.12f; leftEye.d = 0.60f; rightEye.k = 0.12f; rightEye.d = 0.60f;
  }
  if (now > leftEye.nextBlinkTime && behaviorState != BEHAVIOR_SLEEPING &&
      behaviorState != BEHAVIOR_YAWNING && !isBeingPetted && !isDozedNow) {
    leftEye.blinking = true; leftEye.lastBlink = now; rightEye.blinking = true;
    leftEye.nextBlinkTime = now + ((behaviorState == BEHAVIOR_GROGGY || behaviorState == BEHAVIOR_SLEEPY)
                             ? random(1200, 2800) : random(2000, 6000));
  }
  if (leftEye.blinking) {
    leftEye.targetH = 2; rightEye.targetH = 2;
    if (now - leftEye.lastBlink > ((behaviorState == BEHAVIOR_GROGGY) ? 220 : 120)) {
      leftEye.blinking = false; rightEye.blinking = false;
    }
  }
  if (!leftEye.blinking && now - lastSaccade > saccadeInterval &&
      behaviorState != BEHAVIOR_SLEEPING && behaviorState != BEHAVIOR_DIZZY && !isBeingPetted) {
    lastSaccade = now; saccadeInterval = random(700, 3200);
    int dir = random(0, 10);
    float lx = 0, ly = 0;
    if (dir == 4) { lx = -6; ly = -3; } else if (dir == 5) { lx = 6; ly = -3; }
    else if (dir == 6) { lx = -6; ly = 3; } else if (dir == 7) { lx = 6; ly = 3; }
    else if (dir == 8) { lx = 8; ly = 0; } else if (dir == 9) { lx = -8; ly = 0; }
    leftEye.targetPupilX = lx; leftEye.targetPupilY = ly;
    rightEye.targetPupilX = lx; rightEye.targetPupilY = ly;
    leftEye.targetX = 18 + (lx * 0.25f); leftEye.targetY = 14 + (ly * 0.25f);
    rightEye.targetX = 74 + (lx * 0.25f); rightEye.targetY = 14 + (ly * 0.25f);
  }
  if (!leftEye.blinking && behaviorState == BEHAVIOR_AWAKE && !isBeingPetted &&
      now - lastFidgetTime > 25000UL && random(0, 100) < 3) {
    lastFidgetTime = now;
    float fx = (random(-2, 3));
    float fy = (random(-1, 2));
    leftEye.targetPupilX += fx;
    rightEye.targetPupilX += fx;
    leftEye.targetPupilY += fy;
    rightEye.targetPupilY += fy;
  }
  if (isBeingPetted) {
    unsigned long petDuration = millis() - petStartTime;
    if (petDuration < 1200) {
      float osc = sin(petDuration / 160.0f) * 1.5f;
      leftEye.targetW = 38.0f; leftEye.targetH = 30.0f + osc;
      rightEye.targetW = 38.0f; rightEye.targetH = 30.0f + osc;
      leftEye.targetX = 17; leftEye.targetY = 17;
      rightEye.targetX = 73; rightEye.targetY = 17;
      leftEye.targetPupilX = 0; leftEye.targetPupilY = -2.0f;
      rightEye.targetPupilX = 0; rightEye.targetPupilY = -2.0f;
    } else if (petDuration < 2800) {
      float nuzzle = sin(petDuration / 200.0f) * 1.8f;
      leftEye.targetW = 40.0f; leftEye.targetH = 26.0f;
      rightEye.targetW = 40.0f; rightEye.targetH = 26.0f;
      leftEye.targetX = 16 + nuzzle; leftEye.targetY = 19;
      rightEye.targetX = 72 + nuzzle; rightEye.targetY = 19;
      leftEye.targetPupilX = 0; leftEye.targetPupilY = 1.0f;
      rightEye.targetPupilX = 0; rightEye.targetPupilY = 1.0f;
    } else {
      float wiggle = sin(petDuration / 70.0f) * 1.5f;
      leftEye.targetW = 42.0f; leftEye.targetH = 24.0f;
      rightEye.targetW = 42.0f; rightEye.targetH = 24.0f;
      leftEye.targetX = 15 + wiggle; leftEye.targetY = 20;
      rightEye.targetX = 71 - wiggle; rightEye.targetY = 20;
      leftEye.targetPupilX = 0; leftEye.targetPupilY = 1.0f;
      rightEye.targetPupilX = 0; rightEye.targetPupilY = 1.0f;
    }
    leftEye.update(); rightEye.update(); return;
  }
  if (isDozedNow) {
    float dozePhase = ((millis() - dozeEnteredAt) % 3600) / 3600.0f;
    float nodY = 0;
    if (dozePhase < 0.65f) {
      float p = dozePhase / 0.65f;
      nodY = 5.0f * (p * p);
    } else if (dozePhase < 0.78f) {
      float p = (dozePhase - 0.65f) / 0.13f;
      nodY = 5.0f * (1.0f - p);
    }
    leftEye.targetW = 38; leftEye.targetH = 26;
    rightEye.targetW = 38; rightEye.targetH = 26;
    leftEye.targetX = 18; leftEye.targetY = 18 + nodY;
    rightEye.targetX = 74; rightEye.targetY = 18 + nodY;
    leftEye.targetPupilX = 0; leftEye.targetPupilY = 3.0f;
    rightEye.targetPupilX = 0; rightEye.targetPupilY = 3.0f;
    leftEye.update(); rightEye.update(); return;
  }
  if (behaviorState == BEHAVIOR_DIZZY) {
    float elapsed = (float)(now - behaviorEnteredAt);
    float faint = constrain((elapsed - 2100.0f) / 900.0f, 0.0f, 1.0f);
    float angle = elapsed / 260.0f;
    leftEye.targetPupilX = sin(angle) * 8.0f * (1.0f - faint);
    leftEye.targetPupilY = cos(angle) * 6.0f * (1.0f - faint);
    rightEye.targetPupilX = sin(angle + 3.14f) * 8.0f * (1.0f - faint);
    rightEye.targetPupilY = cos(angle + 3.14f) * 6.0f * (1.0f - faint);
    leftEye.targetW = 36; rightEye.targetW = 36;
    leftEye.targetH = 34.0f - faint * 29.0f; rightEye.targetH = 34.0f - faint * 29.0f;
    leftEye.targetX = 18; rightEye.targetX = 74;
    leftEye.targetY = 14.0f + faint * 15.0f; rightEye.targetY = 14.0f + faint * 15.0f;
    leftEye.update(); rightEye.update(); return;
  }
  if (behaviorState == BEHAVIOR_YAWNING) {
    leftEye.targetW = 38; leftEye.targetH = 3; rightEye.targetW = 38; rightEye.targetH = 3;
    leftEye.targetX = 18; leftEye.targetY = 28; rightEye.targetX = 74; rightEye.targetY = 28;
    leftEye.targetPupilX = 0; leftEye.targetPupilY = 0;
    rightEye.targetPupilX = 0; rightEye.targetPupilY = 0;
    leftEye.update(); rightEye.update(); return;
  }
  if (currentGesture != FG_NONE) {
    if (currentGesture == FG_STRETCH) {
      leftEye.targetW = 32; leftEye.targetH = 40; rightEye.targetW = 32; rightEye.targetH = 40;
      leftEye.targetX = 18; leftEye.targetY = 12; rightEye.targetX = 74; rightEye.targetY = 12;
      leftEye.targetPupilY = -2; rightEye.targetPupilY = -2;
    } else if (currentGesture == FG_LOOK_AROUND) {
      float t = (millis() - gestureStartedAt) / 500.0f;
      leftEye.targetPupilX = sin(t) * 6.0f; rightEye.targetPupilX = sin(t) * 6.0f;
      leftEye.targetPupilY = 0; rightEye.targetPupilY = 0;
    } else if (currentGesture == FG_SHIVER) {
      float sx = (random(-2, 3));
      leftEye.targetPupilX = sx; rightEye.targetPupilX = sx;
    } else if (currentGesture == FG_HAPPY_SQUINT) {
      leftEye.targetW = 40; leftEye.targetH = 28; rightEye.targetW = 40; rightEye.targetH = 28;
    } else if (currentGesture == FG_WORRIED_LOOKUP) {
      leftEye.targetPupilY = -4; rightEye.targetPupilY = -4;
      leftEye.targetPupilX = 0; rightEye.targetPupilX = 0;
    } else if (currentGesture == FG_FEAR_SQUEEZE) {
      leftEye.targetW = 34; leftEye.targetH = 6; rightEye.targetW = 34; rightEye.targetH = 6;
    } else if (currentGesture == FG_CONTENT_WIGGLE) {
      leftEye.targetW = 40; leftEye.targetH = 30; rightEye.targetW = 40; rightEye.targetH = 30;
      leftEye.targetPupilX = sin(millis() / 200.0f) * 1.5f;
      rightEye.targetPupilX = -sin(millis() / 200.0f) * 1.5f;
    } else if (currentGesture == FG_DOZE_DIP) {
      leftEye.targetH = 20; rightEye.targetH = 20;
    }
  }
  if (!leftEye.blinking) {
    float bw = 36, bh = 36 + breathVal;
    leftEye.targetX = 18; leftEye.targetY = 14; rightEye.targetX = 74; rightEye.targetY = 14;
    switch (behaviorState) {
      case BEHAVIOR_AWAKE:
        leftEye.targetW = bw; leftEye.targetH = bh; rightEye.targetW = bw; rightEye.targetH = bh;
        break;
      case BEHAVIOR_WAKING: {
        float progress = behaviorDurationMs == 0 ? 1.0f :
          constrain((float)(millis() - behaviorEnteredAt) / behaviorDurationMs, 0.0f, 1.0f);
        float ease = progress * progress * (3.0f - 2.0f * progress);
        float openHeight = 5.0f + 31.0f * ease;
        leftEye.targetW = 36; leftEye.targetH = openHeight;
        rightEye.targetW = 36; rightEye.targetH = openHeight;
        leftEye.targetY = 29.0f - 15.0f * ease;
        rightEye.targetY = 29.0f - 15.0f * ease;
        leftEye.targetPupilY = 3.0f * (1.0f - ease);
        rightEye.targetPupilY = 3.0f * (1.0f - ease);
        break;
      }
      case BEHAVIOR_HAPPY: case BEHAVIOR_CONTENT:
        leftEye.targetW = 40; leftEye.targetH = 30; rightEye.targetW = 40; rightEye.targetH = 30;
        leftEye.targetPupilX = 0; leftEye.targetPupilY = 0;
        rightEye.targetPupilX = 0; rightEye.targetPupilY = 0;
        break;
      case BEHAVIOR_EXCITED: {
        float pt = (millis() - behaviorEnteredAt) / 110.0f;
        leftEye.targetW = 34; leftEye.targetH = 40; rightEye.targetW = 34; rightEye.targetH = 40;
        leftEye.targetY = 12 + sin(pt) * 2.0f; rightEye.targetY = 12 + sin(pt) * 2.0f;
        leftEye.targetPupilX = 0; rightEye.targetPupilX = 0;
        leftEye.targetPupilY = sin(pt * 1.5f) * 2.5f; rightEye.targetPupilY = sin(pt * 1.5f) * 2.5f;
        break;
      }
      case BEHAVIOR_OVER_EXCITED: case BEHAVIOR_PLAYFUL: case BEHAVIOR_CELEBRATING: {
        float pt = (millis() - behaviorEnteredAt) / 90.0f;
        leftEye.targetW = 40; leftEye.targetH = 32; rightEye.targetW = 40; rightEye.targetH = 32;
        leftEye.targetY = 12 + sin(pt) * 2.5f; rightEye.targetY = 12 + sin(pt) * 2.5f;
        leftEye.targetPupilX = cos(pt * 0.7f) * 3.5f; rightEye.targetPupilX = cos(pt * 0.7f) * 3.5f;
        leftEye.targetPupilY = sin(pt) * 3.0f; rightEye.targetPupilY = sin(pt) * 3.0f;
        break;
      }
      case BEHAVIOR_SHY: {
        leftEye.targetW = 32; leftEye.targetH = 26; rightEye.targetW = 32; rightEye.targetH = 26;
        leftEye.targetY = 19; rightEye.targetY = 19;
        leftEye.targetPupilX = 3.0f; rightEye.targetPupilX = -3.0f;
        leftEye.targetPupilY = 4.0f; rightEye.targetPupilY = 4.0f;
        break;
      }
      case BEHAVIOR_SURPRISED:
        leftEye.targetW = 30; leftEye.targetH = 42; rightEye.targetW = 30; rightEye.targetH = 42;
        leftEye.targetY = 12; rightEye.targetY = 12;
        leftEye.targetPupilX = 0; leftEye.targetPupilY = 0; break;
      case BEHAVIOR_LOW_ENERGY:
        leftEye.targetW = 36; leftEye.targetH = 18; rightEye.targetW = 36; rightEye.targetH = 18;
        leftEye.targetY = 28; rightEye.targetY = 28;
        leftEye.targetPupilX = 0; leftEye.targetPupilY = 2.0f;
        rightEye.targetPupilX = 0; rightEye.targetPupilY = 2.0f; break;
      case BEHAVIOR_SLEEPY: case BEHAVIOR_GROGGY:
        leftEye.targetW = 38; leftEye.targetH = 22; rightEye.targetW = 38; rightEye.targetH = 22;
        leftEye.targetY = 20; rightEye.targetY = 20;
        leftEye.targetPupilX = 0; leftEye.targetPupilY = 2.0f;
        rightEye.targetPupilX = 0; rightEye.targetPupilY = 2.0f; break;
      case BEHAVIOR_FED_UP:
        leftEye.targetW = 36; leftEye.targetH = 18; rightEye.targetW = 36; rightEye.targetH = 18;
        leftEye.targetY = 23; rightEye.targetY = 23;
        leftEye.targetPupilX = 6.0f; rightEye.targetPupilX = 6.0f;
        leftEye.targetPupilY = 0; rightEye.targetPupilY = 0; break;
      case BEHAVIOR_ANGRY:
        leftEye.targetW = 36; leftEye.targetH = 26; rightEye.targetW = 36; rightEye.targetH = 26;
        leftEye.targetY = 17; rightEye.targetY = 17;
        leftEye.targetPupilX = 2.0f; rightEye.targetPupilX = -2.0f;
        leftEye.targetPupilY = 1.0f; rightEye.targetPupilY = 1.0f; break;
      case BEHAVIOR_ANNOYED:
        leftEye.targetW = 34; leftEye.targetH = 22; rightEye.targetW = 34; rightEye.targetH = 22;
        leftEye.targetY = 18; rightEye.targetY = 18;
        leftEye.targetPupilX = 0; leftEye.targetPupilY = 0;
        rightEye.targetPupilX = 0; rightEye.targetPupilY = 0; break;
      case BEHAVIOR_IGNORING:
        leftEye.targetW = 36; leftEye.targetH = 24; rightEye.targetW = 36; rightEye.targetH = 24;
        leftEye.targetY = 18; rightEye.targetY = 18;
        leftEye.targetPupilX = -8.0f; rightEye.targetPupilX = -8.0f;
        leftEye.targetPupilY = -1.0f; rightEye.targetPupilY = -1.0f; break;
      case BEHAVIOR_DONT_CARE: {
        leftEye.targetW = 38; leftEye.targetH = 24; rightEye.targetW = 38; rightEye.targetH = 24;
        leftEye.targetY = 18; rightEye.targetY = 18;
        leftEye.targetPupilX = 5.0f; rightEye.targetPupilX = 5.0f;
        leftEye.targetPupilY = 1.0f; rightEye.targetPupilY = 1.0f;
        break;
      }
      case BEHAVIOR_SAD: case BEHAVIOR_CRYING:
        leftEye.targetW = 34; leftEye.targetH = 36; rightEye.targetW = 34; rightEye.targetH = 36;
        leftEye.targetPupilX = 0; leftEye.targetPupilY = 2.0f;
        rightEye.targetPupilX = 0; rightEye.targetPupilY = 2.0f; break;
      case BEHAVIOR_THINKING:
        leftEye.targetW = 34; leftEye.targetH = 32; rightEye.targetW = 36; rightEye.targetH = 36;
        leftEye.targetPupilX = 5; rightEye.targetPupilX = 5;
        leftEye.targetPupilY = -4; rightEye.targetPupilY = -4;
        break;
      case BEHAVIOR_RECOVERING: {
        float settle = constrain((float)(millis() - behaviorEnteredAt) / max(1UL, behaviorDurationMs), 0.0f, 1.0f);
        leftEye.targetW = 38 - settle * 2; rightEye.targetW = 38 - settle * 2;
        leftEye.targetH = 31 + settle * 5; rightEye.targetH = 31 + settle * 5;
        leftEye.targetPupilX *= (1.0f - settle * 0.2f);
        rightEye.targetPupilX *= (1.0f - settle * 0.2f);
        leftEye.targetPupilY *= (1.0f - settle * 0.2f);
        rightEye.targetPupilY *= (1.0f - settle * 0.2f);
        break;
      }
      case BEHAVIOR_CURIOUS:
        leftEye.targetW = 36; leftEye.targetH = 34; rightEye.targetW = 34; rightEye.targetH = 40; break;
      default: leftEye.targetW = bw; leftEye.targetH = bh; rightEye.targetW = bw; rightEye.targetH = bh; break;
    }
  }
  if (!leftEye.blinking && behaviorState == BEHAVIOR_AWAKE && currentGesture != FG_NONE) {
    float phase = (millis() - gestureStartedAt) / 120.0f;
    switch (currentGesture) {
      case FG_STRETCH:
        leftEye.targetW = 32; rightEye.targetW = 32;
        leftEye.targetH = 40 + sin(phase * 0.35f) * 2.0f;
        rightEye.targetH = leftEye.targetH;
        leftEye.targetY = 12; rightEye.targetY = 12;
        leftEye.targetPupilY = -2; rightEye.targetPupilY = -2;
        break;
      case FG_YAWN: {
        float yawn = sin(constrain((float)(millis() - gestureStartedAt) / gestureDurationMs, 0.0f, 1.0f) * PI);
        leftEye.targetH = 30 - yawn * 24; rightEye.targetH = leftEye.targetH;
        leftEye.targetY = 17 + yawn * 8; rightEye.targetY = leftEye.targetY;
        break;
      }
      case FG_LOOK_AROUND:
        leftEye.targetPupilX = sin(phase) * 8; rightEye.targetPupilX = leftEye.targetPupilX;
        leftEye.targetPupilY = cos(phase * 0.65f) * 2; rightEye.targetPupilY = leftEye.targetPupilY;
        break;
      case FG_DOZE_DIP: {
        float dip = sin(constrain((float)(millis() - gestureStartedAt) / gestureDurationMs, 0.0f, 1.0f) * PI);
        leftEye.targetH = 34 - dip * 15; rightEye.targetH = leftEye.targetH;
        leftEye.targetY = 14 + dip * 7; rightEye.targetY = leftEye.targetY;
        leftEye.targetPupilY = 3; rightEye.targetPupilY = 3;
        break;
      }
      case FG_SHIVER: {
        float shake = sin(phase * 2.8f) * 2.2f;
        leftEye.targetX = 18 + shake; rightEye.targetX = 74 + shake;
        leftEye.targetPupilY = -2; rightEye.targetPupilY = -2;
        break;
      }
      case FG_SQUINT_UV: case FG_HAPPY_SQUINT:
        leftEye.targetH = 18; rightEye.targetH = 18;
        leftEye.targetPupilY = -2; rightEye.targetPupilY = -2;
        break;
      case FG_WORRIED_LOOKUP:
        leftEye.targetPupilY = -5; rightEye.targetPupilY = -5;
        leftEye.targetH = 30; rightEye.targetH = 30;
        break;
      case FG_FEAR_SQUEEZE:
        leftEye.targetH = 9; rightEye.targetH = 9;
        leftEye.targetW = 32; rightEye.targetW = 32;
        break;
      case FG_CONTENT_WIGGLE:
        leftEye.targetPupilX = sin(phase) * 3; rightEye.targetPupilX = -leftEye.targetPupilX;
        leftEye.targetH = 30; rightEye.targetH = 30;
        break;
      case FG_SLOW_BLINK: {
        float blink = fabs(sin(constrain((float)(millis() - gestureStartedAt) / gestureDurationMs, 0.0f, 1.0f) * PI));
        leftEye.targetH = 36 - blink * 32; rightEye.targetH = leftEye.targetH;
        break;
      }
      case FG_SIDE_EYE:
        leftEye.targetPupilX = 8; rightEye.targetPupilX = 8;
        leftEye.targetPupilY = 0; rightEye.targetPupilY = 0;
        break;
      case FG_CONFUSED:
        leftEye.targetH = 28 + sin(phase * 0.7f) * 4;
        rightEye.targetH = 38 - sin(phase * 0.7f) * 4;
        leftEye.targetPupilX = -2; rightEye.targetPupilX = 3;
        leftEye.targetPupilY = -1; rightEye.targetPupilY = 2;
        break;
      case FG_WINK:
        rightEye.targetH = 3; rightEye.targetY = 29;
        leftEye.targetH = 34; leftEye.targetPupilY = -1;
        break;
      case FG_LAUGH: {
        float bounce = fabs(sin(phase * 1.5f));
        leftEye.targetH = 22 + bounce * 8; rightEye.targetH = leftEye.targetH;
        leftEye.targetY = 17 + bounce * 2; rightEye.targetY = leftEye.targetY;
        leftEye.targetPupilY = 1; rightEye.targetPupilY = 1;
        break;
      }
      case FG_HEART_EYES:
        leftEye.targetH = 29; rightEye.targetH = 29;
        leftEye.targetPupilX = 0; rightEye.targetPupilX = 0;
        break;
      case FG_CURIOUS_PEEK:
        leftEye.targetW = 28; leftEye.targetH = 25;
        rightEye.targetW = 40; rightEye.targetH = 39;
        leftEye.targetPupilX = -3; rightEye.targetPupilX = 2;
        break;
      case FG_SHOCK:
        leftEye.targetW = 29; rightEye.targetW = 29;
        leftEye.targetH = 42; rightEye.targetH = 42;
        leftEye.targetPupilY = 1; rightEye.targetPupilY = 1;
        break;
      default: break;
    }
  }
  leftEye.update(); rightEye.update();
}
void drawSleepFace() {
  unsigned long sleepElapsed = millis() - sleepEnteredAt;
  float breath = sin(sleepElapsed / 1050.0f);
  int eyeY = 34 + (int)round(breath * 1.5f);

  // Soft, relaxed closed eyes with a slight upward smile at each outer corner.
  for (int i = 0; i < 3; i++) {
    display.drawLine(20, eyeY + i, 25, eyeY + 3 + i, SSD1306_WHITE);
    display.drawLine(25, eyeY + 3 + i, 48, eyeY + 3 + i, SSD1306_WHITE);
    display.drawLine(48, eyeY + 3 + i, 53, eyeY + i, SSD1306_WHITE);
    display.drawLine(75, eyeY + i, 80, eyeY + 3 + i, SSD1306_WHITE);
    display.drawLine(80, eyeY + 3 + i, 103, eyeY + 3 + i, SSD1306_WHITE);
    display.drawLine(103, eyeY + 3 + i, 108, eyeY + i, SSD1306_WHITE);
  }

  // A bubble gently swells, wobbles, and lets go below the relaxed face.
  float cycle = (sleepElapsed % 4200UL) / 4200.0f;
  float swell = cycle < 0.58f ? sin((cycle / 0.58f) * 1.5708f) :
                (cycle < 0.72f ? 1.0f : (1.0f - (cycle - 0.72f) / 0.28f));
  int radius = 2 + (int)(swell * 5.0f);
  int wobble = cycle > 0.58f && cycle < 0.76f ? (int)round(sin(sleepElapsed / 38.0f) * 2.0f) : 0;
  int bx = 64 + wobble;
  int by = 43 - radius / 2;
  display.fillCircle(bx, by, radius, SSD1306_BLACK);
  display.drawCircle(bx, by, radius, SSD1306_WHITE);
  if (radius >= 5) {
    display.drawPixel(bx - 2, by - 2, SSD1306_WHITE);
    display.drawPixel(bx - 1, by - 2, SSD1306_WHITE);
  }

  int zy = 3 + ((sleepElapsed / 750) % 3);
  display.drawBitmap(106, zy, bmp_zzz, 16, 16, SSD1306_WHITE);
}
void updateAndDrawTears() {
  if (behaviorState != BEHAVIOR_CRYING) return;
  for (int i = 0; i < 4; i++) {
    if (!tears[i].active) continue;
    tears[i].y += tears[i].vy;
    if (tears[i].y > 60.0f) tears[i].y = 36.0f + (i % 2) * 3.0f;
    int tx = (int)tears[i].x, ty = (int)tears[i].y;
    display.drawPixel(tx, ty, SSD1306_WHITE); display.drawPixel(tx, ty + 1, SSD1306_WHITE);
  }
}
void drawSweatDropIfHot() {
  if (temperature > 32.0f && !isSleepingNow) {
    int x = 118, y = 10 + ((millis() / 400) % 3);
    display.drawBitmap(x, y, bmp_sweat, 8, 8, SSD1306_WHITE);
  }
}

void drawMouth() {
  int bob = 0;
  if (behaviorState == BEHAVIOR_HAPPY || behaviorState == BEHAVIOR_PLAYFUL ||
      behaviorState == BEHAVIOR_EXCITED || behaviorState == BEHAVIOR_OVER_EXCITED ||
      behaviorState == BEHAVIOR_CELEBRATING) {
    bob = (int)round(sin((millis() - behaviorEnteredAt) / 420.0f));
  }
  int eyeCenterY = (int)round((leftEye.y + rightEye.y) * 0.5f);
  bob += constrain((int)round((eyeCenterY - 14) * 0.35f), -2, 2);

  switch (behaviorState) {
    case BEHAVIOR_AWAKE:
    case BEHAVIOR_HAPPY:
    case BEHAVIOR_SHY:
    case BEHAVIOR_RECOVERING:
      display.drawLine(56, 51 + bob, 60, 54 + bob, SSD1306_WHITE);
      display.drawLine(60, 54 + bob, 64, 55 + bob, SSD1306_WHITE);
      display.drawLine(64, 55 + bob, 68, 54 + bob, SSD1306_WHITE);
      display.drawLine(68, 54 + bob, 72, 51 + bob, SSD1306_WHITE);
      break;
    case BEHAVIOR_CONTENT:
      display.drawLine(59, 53, 62, 55, SSD1306_WHITE);
      display.drawLine(62, 55, 66, 55, SSD1306_WHITE);
      display.drawLine(66, 55, 69, 53, SSD1306_WHITE);
      break;
    case BEHAVIOR_PLAYFUL:
    case BEHAVIOR_EXCITED:
    case BEHAVIOR_OVER_EXCITED:
    case BEHAVIOR_CELEBRATING:
      display.drawLine(54, 49 + bob, 58, 54 + bob, SSD1306_WHITE);
      display.drawLine(58, 54 + bob, 64, 57 + bob, SSD1306_WHITE);
      display.drawLine(64, 57 + bob, 70, 54 + bob, SSD1306_WHITE);
      display.drawLine(70, 54 + bob, 74, 49 + bob, SSD1306_WHITE);
      break;
    case BEHAVIOR_SURPRISED:
    case BEHAVIOR_WAKING:
      display.drawLine(61, 53, 67, 54, SSD1306_WHITE);
      break;
    case BEHAVIOR_YAWNING:
      display.drawLine(57, 49, 60, 56, SSD1306_WHITE);
      display.drawLine(60, 56, 64, 59, SSD1306_WHITE);
      display.drawLine(64, 59, 68, 56, SSD1306_WHITE);
      display.drawLine(68, 56, 71, 49, SSD1306_WHITE);
      break;
    case BEHAVIOR_SAD:
    case BEHAVIOR_CRYING:
      display.drawLine(56, 55, 60, 52, SSD1306_WHITE);
      display.drawLine(60, 52, 64, 51, SSD1306_WHITE);
      display.drawLine(64, 51, 68, 52, SSD1306_WHITE);
      display.drawLine(68, 52, 72, 55, SSD1306_WHITE);
      break;
    case BEHAVIOR_ANNOYED:
    case BEHAVIOR_FED_UP:
      display.drawLine(58, 54, 70, 54, SSD1306_WHITE);
      break;
    case BEHAVIOR_ANGRY:
      display.drawLine(57, 52, 71, 55, SSD1306_WHITE);
      break;
    case BEHAVIOR_IGNORING:
    case BEHAVIOR_DONT_CARE:
      display.drawLine(58, 53, 70, 53, SSD1306_WHITE);
      break;
    case BEHAVIOR_DIZZY:
      display.drawLine(58, 53, 70, 55, SSD1306_WHITE);
      break;
    case BEHAVIOR_GROGGY:
    case BEHAVIOR_SLEEPY:
    case BEHAVIOR_LOW_ENERGY:
      display.drawLine(60, 54, 68, 54, SSD1306_WHITE);
      break;
    default:
      break;
  }
}

void drawEmoPage() {
  if (isSleepingNow || behaviorState == BEHAVIOR_SLEEPING) return;
  if (isDozedNow) {
    int zy = 2 + ((millis() / 700) % 4);
    display.drawBitmap(108, zy, bmp_zzz, 16, 16, SSD1306_WHITE);
  }
  updatePhysics(); updateFloatingHearts(); updateSparkles();
  if (behaviorState == BEHAVIOR_ANNOYED || behaviorState == BEHAVIOR_ANGRY)
    display.drawBitmap(108, 2, bmp_anger, 16, 16, SSD1306_WHITE);
  if (behaviorState == BEHAVIOR_SHY || isBeingPetted) {
    display.drawLine(22, 48, 25, 45, SSD1306_WHITE);
    display.drawLine(27, 48, 30, 45, SSD1306_WHITE);
    display.drawLine(96, 48, 99, 45, SSD1306_WHITE);
    display.drawLine(101, 48, 104, 45, SSD1306_WHITE);
  }
  drawEye(true); drawEye(false);
  drawMouth();
  if (behaviorState == BEHAVIOR_DIZZY && millis() - behaviorEnteredAt >= 2550UL) {
    display.drawLine(28, 29, 44, 34, SSD1306_BLACK);
    display.drawLine(44, 29, 28, 34, SSD1306_BLACK);
    display.drawLine(84, 29, 100, 34, SSD1306_BLACK);
    display.drawLine(100, 29, 84, 34, SSD1306_BLACK);
    display.drawPixel(61, 46, SSD1306_WHITE);
    display.drawPixel(64, 46, SSD1306_WHITE);
    display.drawPixel(67, 46, SSD1306_WHITE);
  }
  updateAndDrawTears(); drawFloatingHearts(); drawSparkles();
  drawRainOverlay(); drawSweatDropIfHot();
  if (currentGesture == FG_HEART_EYES) {
    display.drawBitmap(30, 17, bmp_heart_small, 8, 8, SSD1306_WHITE);
    display.drawBitmap(86, 17, bmp_heart_small, 8, 8, SSD1306_WHITE);
  } else if (currentGesture == FG_CONFUSED) {
    display.drawPixel(63, 2, SSD1306_WHITE);
    display.drawPixel(63, 4, SSD1306_WHITE);
    display.drawLine(62, 1, 64, 1, SSD1306_WHITE);
  } else if (currentGesture == FG_SHOCK) {
    display.drawPixel(12, 20, SSD1306_WHITE); display.drawPixel(14, 17, SSD1306_WHITE);
    display.drawPixel(116, 20, SSD1306_WHITE); display.drawPixel(114, 17, SSD1306_WHITE);
  }
  drawAmbientPixelArt();
}

void drawAmbientPixelArt() {
  if (ambientArt == 0 || millis() >= ambientArtUntil) return;
  const int x = 57, y = 1;
  switch (ambientArt) {
    case 1:
      display.drawPixel(x + 6, y, SSD1306_WHITE); display.drawLine(x + 5, y + 2, x + 7, y + 2, SSD1306_WHITE);
      display.drawLine(x + 3, y + 4, x + 9, y + 4, SSD1306_WHITE); display.drawPixel(x + 6, y + 6, SSD1306_WHITE);
      break;
    case 2:
      display.drawLine(x + 2, y + 7, x + 11, y + 7, SSD1306_WHITE);
      display.drawLine(x + 4, y + 5, x + 9, y + 5, SSD1306_WHITE);
      display.drawPixel(x + 6, y + 3, SSD1306_WHITE); display.drawPixel(x + 7, y + 3, SSD1306_WHITE);
      display.drawLine(x + 5, y + 9, x + 8, y + 9, SSD1306_WHITE);
      break;
    case 3:
      display.drawPixel(x + 5, y + 1, SSD1306_WHITE); display.drawLine(x + 4, y + 3, x + 7, y + 3, SSD1306_WHITE);
      display.drawLine(x + 3, y + 5, x + 8, y + 5, SSD1306_WHITE); display.drawPixel(x + 2, y + 7, SSD1306_WHITE);
      display.drawPixel(x + 9, y + 7, SSD1306_WHITE);
      break;
    case 4:
      display.drawPixel(x + 6, y + 1, SSD1306_WHITE); display.drawLine(x + 4, y + 3, x + 8, y + 3, SSD1306_WHITE);
      display.drawLine(x + 3, y + 5, x + 9, y + 5, SSD1306_WHITE); display.drawPixel(x + 4, y + 7, SSD1306_WHITE);
      display.drawPixel(x + 8, y + 7, SSD1306_WHITE); display.drawPixel(x + 6, y + 9, SSD1306_WHITE);
      break;
    case 5:
      display.drawPixel(x + 6, y + 1, SSD1306_WHITE); display.drawPixel(x + 4, y + 3, SSD1306_WHITE);
      display.drawPixel(x + 8, y + 3, SSD1306_WHITE); display.drawLine(x + 3, y + 5, x + 9, y + 5, SSD1306_WHITE);
      display.drawPixel(x + 6, y + 8, SSD1306_WHITE);
      break;
  }
}

void startSelectedGame() {
  unsigned long now = millis();
  gameRunning = true;
  gameScore = 0;
  gameReactionMs = 0;
  gameLastFrameAt = now;
  gameLastFlapAt = 0;
  gameStartedAt = 0;
  if (selectedGame == GAME_FLAPPY) {
    gamePhase = GAME_READY;
    gamePhaseAt = now + 1100;
    gameBirdY = 28.0f;
    gameBirdVelocity = 0.0f;
    gamePipeX = 112.0f;
    gamePipeGapY = random(32, 37);
    gamePipeScored = false;
  } else {
    gamePhase = GAME_READY;
    reflexLightsOn = 0;
    gamePhaseAt = now + random(500UL, 1300UL);
  }
}

void handleGameTap() {
  unsigned long now = millis();
  if (selectedGame == GAME_FLAPPY) {
    if (gamePhase == GAME_OVER) {
      startSelectedGame();
      gamePhase = GAME_PLAYING;
    }
    if (gamePhase == GAME_READY) gamePhase = GAME_PLAYING;
    if (gamePhase == GAME_PLAYING &&
        (gameLastFlapAt == 0 || now - gameLastFlapAt >= 300UL)) {
      gameBirdVelocity = -46.0f;
      gameLastFlapAt = now;
    }
    return;
  }

  if (gameTouchPressedBeforeGo) {
    gameTouchPressedBeforeGo = false;
    gamePhase = GAME_RESULT;
    gameReactionMs = -1;
    gamePhaseAt = now;
    return;
  }
  gameTouchPressedBeforeGo = false;
  if (gamePhase == GAME_RESULT) {
    gameReactionMs = 0;
    gamePhase = GAME_READY;
    reflexLightsOn = 0;
    gamePhaseAt = now + random(500UL, 1300UL);
    return;
  }
  if (gamePhase == GAME_READY) {
    gamePhase = GAME_RESULT;
    gameReactionMs = -1;
    gamePhaseAt = now;
  } else if (gamePhase == GAME_PLAYING) {
    gameReactionMs = (int)(now - gameStartedAt);
    gamePhase = GAME_RESULT;
    gamePhaseAt = now;
    if (gameReactionMs < gameBestReactionMs || gameBestReactionMs == 0)
      gameBestReactionMs = gameReactionMs;
  }
}

void updateMiniGame() {
  if (currentPage != 5 || !gameRunning) return;
  unsigned long now = millis();
  if (selectedGame == GAME_BLINK_REFLEX) {
    if (gamePhase == GAME_READY && now >= gamePhaseAt) {
      if (reflexLightsOn < 3) {
        reflexLightsOn++;
        // Each lamp gets its own slightly jittered interval. After lamp three,
        // hold a random F1-style pause before the lights go out and GO appears.
        gamePhaseAt = now + (reflexLightsOn < 3
          ? random(350UL, 900UL)
          : random(550UL, 1700UL));
      } else {
        gamePhase = GAME_PLAYING;
        gameStartedAt = now;
      }
    }
    return;
  }
  if (gamePhase == GAME_READY && now >= gamePhaseAt) gamePhase = GAME_PLAYING;
  if (gamePhase != GAME_PLAYING) return;

  float dt = (now - gameLastFrameAt) / 1000.0f;
  gameLastFrameAt = now;
  if (dt > 0.04f) dt = 0.04f;
  gameBirdVelocity = min(25.0f, gameBirdVelocity + 105.0f * dt);
  gameBirdY += gameBirdVelocity * dt;
  gamePipeX -= 8.0f * dt;

  if (!gamePipeScored && gamePipeX + 8 < 28) {
    gamePipeScored = true;
    gameScore++;
    if (gameScore > gameBestFlappyScore) gameBestFlappyScore = gameScore;
  }
  if (gamePipeX < -9) {
    gamePipeX = 128;
    gamePipeGapY = random(32, 37);
    gamePipeScored = false;
  }

  bool hitPipe = gamePipeX < 34 && gamePipeX + 8 > 23 &&
    (gameBirdY - 2 < gamePipeGapY - 12 || gameBirdY + 2 > gamePipeGapY + 12);
  if (gameBirdY < 8 || gameBirdY > 60 || hitPipe) {
    gamePhase = GAME_OVER;
    if (gameScore > gameBestFlappyScore) gameBestFlappyScore = gameScore;
  }
}

void drawMiniGamePage() {
  display.setFont(NULL); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  if (!gameRunning) {
    display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(4, 2); display.print("ARCADE");
    display.setCursor(82, 2); display.print(selectedGame == GAME_FLAPPY ? "01 / 02" : "02 / 02");
    display.setTextColor(SSD1306_WHITE);
    display.drawRoundRect(4, 14, 120, 29, 5, SSD1306_WHITE);
    display.drawLine(44, 18, 44, 39, SSD1306_WHITE);
    if (selectedGame == GAME_FLAPPY) {
      display.drawLine(10, 36, 37, 36, SSD1306_WHITE);
      display.drawRoundRect(16, 24, 16, 10, 4, SSD1306_WHITE);
      display.fillCircle(27, 27, 1, SSD1306_WHITE);
      display.drawLine(16, 29, 12, 27, SSD1306_WHITE);
      display.drawLine(22, 31, 19, 34, SSD1306_WHITE);
      display.fillRect(35, 18, 4, 8, SSD1306_WHITE);
      display.fillRect(33, 26, 6, 3, SSD1306_WHITE);
      display.fillRect(35, 33, 4, 7, SSD1306_WHITE);
      display.fillRect(33, 31, 6, 3, SSD1306_WHITE);
      display.setCursor(51, 20); display.print("FLAPPY KIBO");
      display.setCursor(51, 32); display.print("Tap to flap");
    } else {
      display.drawRoundRect(10, 19, 28, 18, 5, SSD1306_WHITE);
      display.drawCircle(16, 28, 3, SSD1306_WHITE);
      display.drawCircle(24, 28, 3, SSD1306_WHITE);
      display.drawCircle(32, 28, 3, SSD1306_WHITE);
      display.fillCircle(16, 28, 1, SSD1306_WHITE);
      display.fillCircle(24, 28, 1, SSD1306_WHITE);
      display.fillCircle(32, 28, 1, SSD1306_WHITE);
      display.setCursor(51, 20); display.print("BLINK REFLEX");
      display.setCursor(51, 32); display.print("Tap on green");
    }
    display.drawLine(4, 45, 124, 45, SSD1306_WHITE);
    display.setCursor(5, 47); display.print("TAP PLAY | 2X NEXT");
    display.setCursor(5, 55); display.print("HOLD: BACK");
    return;
  }

  if (selectedGame == GAME_FLAPPY) {
    display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(3, 2); display.print("FLAPPY");
    display.setCursor(69, 2); display.print("S:"); display.print(gameScore);
    display.setCursor(98, 2); display.print("B:"); display.print(gameBestFlappyScore);
    display.setTextColor(SSD1306_WHITE);
    display.drawLine(0, 55, 128, 55, SSD1306_WHITE);
    for (int x = (millis() / 150) % 14; x < 128; x += 14) display.drawPixel(x, 59, SSD1306_WHITE);
    if (gamePhase == GAME_READY || gamePhase == GAME_PLAYING || gamePhase == GAME_OVER) {
      int px = (int)gamePipeX;
      int gapTop = gamePipeGapY - 12;
      int gapBottom = gamePipeGapY + 12;
      display.fillRect(px, 12, 8, max(0, gapTop - 12), SSD1306_WHITE);
      display.fillRect(px - 2, max(12, gapTop - 4), 12, 4, SSD1306_WHITE);
      display.fillRect(px - 2, gapBottom, 12, 4, SSD1306_WHITE);
      display.fillRect(px, gapBottom + 4, 8, max(0, 55 - (gapBottom + 4)), SSD1306_WHITE);
      int by = (int)gameBirdY;
      display.fillRoundRect(23, by - 4, 10, 8, 3, SSD1306_WHITE);
      display.drawPixel(30, by - 2, SSD1306_BLACK);
      display.drawLine(23, by, 19, by - 2, SSD1306_WHITE);
      int wing = (millis() / 100) % 2 ? 2 : -2;
      display.drawLine(26, by, 22, by + wing, SSD1306_WHITE);
    }
    if (gamePhase == GAME_READY) {
      display.setCursor(40, 15); display.print("TAP / HOLD");
    }
    if (gamePhase == GAME_OVER) {
      display.fillRoundRect(28, 18, 72, 29, 5, SSD1306_BLACK);
      display.drawRoundRect(28, 18, 72, 29, 5, SSD1306_WHITE);
      display.setCursor(53, 23); display.print("BONK");
      display.setCursor(35, 35); display.print("Tap: replay");
    }
    return;
  }

  display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(30, 2); display.print("BLINK REFLEX");
  display.setTextColor(SSD1306_WHITE);
  display.drawLine(12, 16, 116, 16, SSD1306_WHITE);
  if (gamePhase == GAME_READY || gamePhase == GAME_PLAYING) {
    const int lampX[] = { 42, 64, 86 };
    for (int i = 0; i < 3; i++) {
      display.drawRoundRect(lampX[i] - 7, 22, 14, 17, 5, SSD1306_WHITE);
      if (gamePhase == GAME_READY && i < reflexLightsOn)
        display.fillCircle(lampX[i], 30, 4, SSD1306_WHITE);
    }
    if (gamePhase == GAME_READY) {
      display.setCursor(49, 47); display.print("WAIT...");
    } else {
      display.setTextSize(2); display.setCursor(49, 43); display.print("GO!"); display.setTextSize(1);
    }
  } else {
    display.drawRoundRect(20, 21, 88, 30, 5, SSD1306_WHITE);
    display.setCursor(35, 26);
    if (gameReactionMs < 0) display.print("Too early!");
    else { display.print(gameReactionMs); display.print(" ms"); }
    display.setCursor(34, 39);
    if (gameBestReactionMs > 0) { display.print("Best "); display.print(gameBestReactionMs); display.print(" ms"); }
    else display.print("Tap to retry");
  }
}

// 17. PAGES
const unsigned char* getBigIcon(String w) {
  if (w == "Clear") return bmp_clear;
  if (w == "Clouds") return bmp_clouds;
  if (w == "Rain" || w == "Drizzle" || w == "Thunderstorm") return bmp_rain;
  return bmp_clouds;
}
const unsigned char* getMiniIcon(String w) {
  if (w == "Clear") return mini_sun;
  if (w == "Rain" || w == "Drizzle" || w == "Thunderstorm") return mini_rain;
  return mini_cloud;
}
void drawWifiBars() {
  display.setFont(NULL); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  if (WiFi.status() != WL_CONNECTED) {
    for (int i = 0; i < 4; i++) {
      int x = 2 + i * 3, h = 3 + i * 2;
      display.drawRect(x, 10 - h, 2, h, SSD1306_WHITE);
    }
    display.drawLine(1, 10, 13, 2, SSD1306_WHITE);
    display.drawLine(1, 11, 13, 3, SSD1306_WHITE);
    return;
  }
  long rssi = WiFi.RSSI(); int bars = 1;
  if (rssi > -60) bars = 4; else if (rssi > -70) bars = 3; else if (rssi > -80) bars = 2;
  for (int i = 0; i < 4; i++) {
    int x = 2 + i * 3, h = 3 + i * 2;
    if (i < bars) display.fillRect(x, 10 - h, 2, h, SSD1306_WHITE);
    else display.drawRect(x, 10 - h, 2, h, SSD1306_WHITE);
  }
}
void drawClockDigital() {
  struct tm t;
  if (!getLocalTime(&t, 5)) {
    display.setFont(NULL); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    display.setCursor(4, 14); display.print("Syncing time...");
    display.setCursor(4, 28);
    if (WiFi.status() != WL_CONNECTED) {
      display.print("WiFi: connecting...");
    } else {
      display.print("IP: "); display.print(WiFi.localIP());
    }
    display.setCursor(4, 42);
    display.print("NTP tries: "); display.print(ntpAttemptsCount);
    display.setCursor(4, 54);
    display.print("AP: 192.168.4.1");
    return;
  }
  display.setFont(NULL); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  String ampm = (t.tm_hour >= 12) ? "PM" : "AM";
  int h12 = t.tm_hour % 12; if (h12 == 0) h12 = 12;
  display.setCursor(108, 4); display.print(ampm);

  char hStr[4], mStr[4];
  sprintf(hStr, "%02d", h12);
  sprintf(mStr, "%02d", t.tm_min);

  display.setFont(&FreeSansBold18pt7b);
  int16_t x1, y1; uint16_t hw, hh;
  display.getTextBounds(hStr, 0, 0, &x1, &y1, &hw, &hh);

  // Hours right-aligned ending at x=59
  display.setCursor(59 - hw, 42); display.print(hStr);

  if ((t.tm_sec % 2) == 0) {
    display.fillRect(63, 25, 3, 4, SSD1306_WHITE);
    display.fillRect(63, 34, 3, 4, SSD1306_WHITE);
  }

  // Minutes left-aligned starting at x=69
  display.setCursor(69, 42); display.print(mStr);

  // Seconds small font top-right under ampm
  display.setFont(NULL); display.setTextSize(1);
  display.setCursor(108, 16);
  display.printf("%02d", t.tm_sec);

  display.setFont(&FreeSans9pt7b);
  char ds[24]; strftime(ds, 24, "%a, %b %d", &t);
  uint16_t dw, dh;
  display.getTextBounds(ds, 0, 0, &x1, &y1, &dw, &dh);
  display.setCursor((SCREEN_WIDTH - dw) / 2, 62); display.print(ds);
}
void drawClockAnalog() {
  struct tm t;
  if (!getLocalTime(&t, 5)) {
    drawClockDigital();
    return;
  }
  int cx = 64, cy = 32, r = 28;
  display.drawCircle(cx, cy, r, SSD1306_WHITE);
  for (int h = 0; h < 12; h++) {
    float a = h * (PI / 6.0f);
    int x1 = cx + (int)(sin(a) * (r - 3)), y1 = cy - (int)(cos(a) * (r - 3));
    int x2 = cx + (int)(sin(a) * (r - 1)), y2 = cy - (int)(cos(a) * (r - 1));
    if (h % 3 == 0) display.drawLine(x1, y1, x2, y2, SSD1306_WHITE); else display.drawPixel(x1, y1, SSD1306_WHITE);
  }
  float hA = ((t.tm_hour % 12) + t.tm_min / 60.0f) * (PI / 6.0f);
  float mA = (t.tm_min + t.tm_sec / 60.0f) * (PI / 30.0f);
  float sA = t.tm_sec * (PI / 30.0f);
  display.drawLine(cx, cy, cx + (int)(sin(hA) * (r - 14)), cy - (int)(cos(hA) * (r - 14)), SSD1306_WHITE);
  display.drawLine(cx, cy, cx + (int)(sin(mA) * (r - 6)), cy - (int)(cos(mA) * (r - 6)), SSD1306_WHITE);
  display.drawLine(cx, cy, cx + (int)(sin(sA) * (r - 2)), cy - (int)(cos(sA) * (r - 2)), SSD1306_WHITE);
  display.fillCircle(cx, cy, 2, SSD1306_WHITE);
}
void drawTimePage() { drawWifiBars(); if (analogClock) drawClockAnalog(); else drawClockDigital(); }
void formatHHMM(long ep, char* buf, size_t n) {
  time_t t = (time_t)ep; struct tm* lt = localtime(&t);
  if (!lt) { snprintf(buf, n, "--:--"); return; }
  snprintf(buf, n, "%02d:%02d", lt->tm_hour, lt->tm_min);
}
float cToF(float c) { return c * 9.0f / 5.0f + 32.0f; }
void drawWeatherPage() {
  display.setFont(NULL); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  if (millis() < citySwitchNotificationUntil) {
    display.fillRect(6, 16, 116, 32, SSD1306_BLACK);
    display.drawRect(6, 16, 116, 32, SSD1306_WHITE);
    display.drawRect(8, 18, 112, 28, SSD1306_WHITE);
    display.setCursor(14, 22); display.print("CITY "); display.print(currentCityIndex + 1); display.print("/"); display.print(weatherCityCount);
    display.setCursor(14, 34); display.print(city);
    return;
  }
  if (city.isEmpty()) { display.setCursor(24, 28); display.print("No City Set"); return; }
  if (WiFi.status() != WL_CONNECTED) { display.setCursor(24, 28); display.print("No Wi-Fi"); return; }
  if (apiKey.isEmpty()) { display.setCursor(12, 28); display.print("Add OpenWeather key"); return; }
  if (!weatherDataValid) {
    display.setCursor(18, 24); display.print(weatherMain == "Loading" ? "Fetching weather..." : "Weather unavailable");
    display.setCursor(18, 40); display.print(weatherDesc);
    if (weatherHttpCode == 404) {
      display.setCursor(18, 52); display.print(city); display.print(" / check country");
    }
    return;
  }
  String c = city; c.toUpperCase(); if (c.length() > 14) c = c.substring(0, 13) + ".";
  display.setCursor(2, 2); display.print(c);
  display.drawBitmap(96, 0, getBigIcon(weatherMain), 32, 32, SSD1306_WHITE);
  float shown = useFahrenheit ? cToF(temperature) : temperature;
  display.setFont(&FreeSansBold18pt7b);
  int ti = (int)shown; display.setCursor(2, 44); display.print(ti);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(String(ti).c_str(), 0, 44, &x1, &y1, &w, &h);
  int dx = x1 + w + 4; if (dx > 125) dx = 125;
  display.fillCircle(dx, 24, 2, SSD1306_WHITE);
  display.setFont(NULL); display.setTextSize(1);
  display.drawBitmap(60, 24, bmp_tiny_drop, 8, 8, SSD1306_WHITE);
  display.setCursor(70, 24); display.print(humidity); display.print("%");
  display.setCursor(60, 36); display.print("~"); display.print((int)(useFahrenheit ? cToF(feelsLike) : feelsLike)); display.print(useFahrenheit ? "F" : "C");
  display.drawLine(0, 50, 128, 50, SSD1306_WHITE);
  display.setCursor(2, 54);
  String d = weatherDesc; if (d.length() > 20) d = d.substring(0, 19) + ".";
  display.print(d);
  if (sunriseEpoch > 0 && sunsetEpoch > 0) {
    char rs[8], ss[8];
    formatHHMM(sunriseEpoch, rs, sizeof(rs)); formatHHMM(sunsetEpoch, ss, sizeof(ss));
    display.setCursor(96, 34); display.print(rs);
    display.setCursor(96, 44); display.print(ss);
  }
}
void drawForecastPage() {
  if (city.isEmpty()) {
    display.setFont(NULL); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    display.setCursor(24, 28); display.print("No City Set");
    return;
  }
  if (millis() < citySwitchNotificationUntil) {
    display.fillRect(6, 16, 116, 32, SSD1306_BLACK);
    display.drawRect(6, 16, 116, 32, SSD1306_WHITE);
    display.drawRect(8, 18, 112, 28, SSD1306_WHITE);
    display.setFont(NULL); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    display.setCursor(14, 22); display.print("CITY "); display.print(currentCityIndex + 1); display.print("/"); display.print(weatherCityCount);
    display.setCursor(14, 34); display.print(city);
    return;
  }
  if (WiFi.status() != WL_CONNECTED || apiKey.isEmpty() || !weatherForecastValid) {
    display.setFont(NULL); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    display.setCursor(18, 24);
    if (WiFi.status() != WL_CONNECTED) display.print("No Wi-Fi");
    else if (apiKey.isEmpty()) display.print("Add OpenWeather key");
    else if (weatherFetchState == W_GEOCODING) display.print("Finding city...");
    else if (weatherFetchState != W_IDLE) display.print("Fetching forecast...");
    else if (!weatherDataValid && weatherHttpCode >= 400) { display.print("Weather HTTP "); display.print(weatherHttpCode); }
    else if (weatherForecastError >= 400) { display.print("Forecast HTTP "); display.print(weatherForecastError); }
    else if (weatherForecastError == -1) display.print("Forecast parse error");
    else if (weatherForecastError == -2) display.print("No forecast data");
    else if (weatherForecastError == -3) display.print("Waiting for clock");
    else display.print("Forecast unavailable");
    if (weatherForecastError == 404 || (!weatherDataValid && weatherHttpCode == 404)) {
      display.setCursor(18, 40); display.print(city); display.print(" / check country");
    }
    return;
  }
  display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  display.setFont(NULL); display.setTextSize(1); display.setTextColor(SSD1306_BLACK);
  String header = city; header.toUpperCase();
  if (header.length() > 18) header = header.substring(0, 17) + ".";
  int startX = (128 - (header.length() * 6)) / 2;
  if (startX < 2) startX = 2;
  display.setCursor(startX, 2); display.print(header);
  display.setTextColor(SSD1306_WHITE);
  display.drawLine(42, 13, 42, 63, SSD1306_WHITE); display.drawLine(85, 13, 85, 63, SSD1306_WHITE);
  for (int i = 0; i < 3; i++) {
    int cs = i * 43, cx = cs + 21;
    String d = fcast[i].dayName; if (d.isEmpty()) d = "--";
    display.setFont(NULL); display.setCursor(cx - (d.length() * 3), 16); display.print(d);
    display.drawBitmap(cx - 8, 28, getMiniIcon(fcast[i].iconType), 16, 16, SSD1306_WHITE);
    int shown = useFahrenheit ? (int)cToF((float)fcast[i].temp) : fcast[i].temp;
    char ts[8]; sprintf(ts, "%d%c", shown, useFahrenheit ? 'F' : 'C');
    display.setCursor(cx - (strlen(ts) * 3), 48); display.print(ts);
  }
}
void drawWorldClock() {
  time_t nowEpoch; time(&nowEpoch);
  time_t tky = nowEpoch + (9 * 3600), ny = nowEpoch - (5 * 3600);
  struct tm* tt = gmtime(&tky); int th = tt->tm_hour, tmn = tt->tm_min;
  struct tm* nt = gmtime(&ny); int nh = nt->tm_hour, nm = nt->tm_min;
  display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  display.setFont(NULL); display.setTextSize(1); display.setTextColor(SSD1306_BLACK);
  display.setCursor(34, 2); display.print("WORLD CLOCK");
  display.setTextColor(SSD1306_WHITE);
  display.drawLine(64, 14, 64, 52, SSD1306_WHITE);
  display.setCursor(20, 18); display.print("TOKYO");
  display.setFont(&FreeSansBold9pt7b);
  char ts[8]; sprintf(ts, "%02d:%02d", th, tmn); display.setCursor(6, 42); display.print(ts);
  display.setFont(NULL); display.setCursor(72, 18); display.print("NEW YORK");
  display.setFont(&FreeSansBold9pt7b);
  char ns[8]; sprintf(ns, "%02d:%02d", nh, nm); display.setCursor(68, 42); display.print(ns);
  display.setFont(NULL); display.setCursor(30, 54); display.print("Tap: back");
}

// 18. WEATHER NETWORK
struct WeatherRequestArgs {
  String url;
  WeatherRequestType type;
  uint32_t generation;
};

String encodeQueryValue(const String& value) {
  const char hex[] = "0123456789ABCDEF";
  String encoded;
  encoded.reserve(value.length() * 3);
  for (size_t i = 0; i < value.length(); i++) {
    uint8_t c = (uint8_t)value[i];
    bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
    if (safe) encoded += (char)c;
    else { encoded += '%'; encoded += hex[c >> 4]; encoded += hex[c & 0x0F]; }
  }
  return encoded;
}

void weatherRequestTask(void* parameter) {
  WeatherRequestArgs* args = (WeatherRequestArgs*)parameter;
  HTTPClient client;
  client.begin(args->url);
  client.setTimeout(8000);
  client.setConnectTimeout(2000);
  int code = client.GET();
  String payload = code > 0 ? client.getString() : "";
  client.end();

  xSemaphoreTake(weatherResultMutex, portMAX_DELAY);
  weatherResultCode = code;
  weatherResultPayload = payload;
  weatherResultType = args->type;
  weatherResultGeneration = args->generation;
  weatherResultReady = true;
  weatherRequestRunning = false;
  xSemaphoreGive(weatherResultMutex);
  delete args;
  vTaskDelete(nullptr);
}

bool startWeatherRequest(const String& url, WeatherRequestType type) {
  if (!weatherResultMutex) return false;
  WeatherRequestArgs* args = new WeatherRequestArgs;
  if (!args) return false;
  args->url = url;
  args->type = type;
  args->generation = weatherGeneration;

  xSemaphoreTake(weatherResultMutex, portMAX_DELAY);
  if (weatherRequestRunning || weatherResultReady) {
    xSemaphoreGive(weatherResultMutex);
    delete args;
    return false;
  }
  weatherRequestRunning = true;
  xSemaphoreGive(weatherResultMutex);

  BaseType_t created = xTaskCreate(weatherRequestTask, "kibo_weather", 8192, args, 1, nullptr);
  if (created != pdPASS) {
    xSemaphoreTake(weatherResultMutex, portMAX_DELAY);
    weatherRequestRunning = false;
    xSemaphoreGive(weatherResultMutex);
    delete args;
    return false;
  }
  weatherFetchStart = millis();
  weatherFetchState = type == WX_GEOCODE ? W_GEOCODING :
                      (type == WX_FORECAST ? W_FETCHING_FORECAST : W_FETCHING_CURRENT);
  return true;
}

bool requestWeather() {
  if (weatherFetchState != W_IDLE) return true;
  if (WiFi.status() != WL_CONNECTED) return true;
  if (apiKey.isEmpty() || city.isEmpty()) return true;
  if (!weatherCoordinatesValid) {
    String place = encodeQueryValue(city);
    String country = weatherCountryForCurrentCity();
    if (!weatherGeocodeRetriedWithoutCountry && !country.isEmpty()) place += "," + encodeQueryValue(country);
    String url = "http://api.openweathermap.org/geo/1.0/direct?q=" + place +
                 "&limit=1&appid=" + encodeQueryValue(apiKey);
    if (!startWeatherRequest(url, WX_GEOCODE)) {
      DBG("[WX] geocoding request not started\n");
      if (!weatherDataValid) { weatherMain = "Unavailable"; weatherDesc = "Waiting to retry"; }
      return true;
    }
    if (!weatherDataValid) { weatherMain = "Loading"; weatherDesc = "Finding city"; }
    DBG("[WX] geocoding: %s%s%s\n", city.c_str(),
        (!weatherGeocodeRetriedWithoutCountry && !country.isEmpty()) ? "," : "",
        (!weatherGeocodeRetriedWithoutCountry && !country.isEmpty()) ? country.c_str() : "");
    return true;
  }
  String coords = "lat=" + String(weatherLatitude, 6) + "&lon=" + String(weatherLongitude, 6);
  String url = "http://api.openweathermap.org/data/2.5/weather?" + coords +
               "&appid=" + encodeQueryValue(apiKey) + "&units=metric";
  if (!startWeatherRequest(url, WX_CURRENT)) {
    DBG("[WX] current request not started\n");
    if (!weatherDataValid) { weatherMain = "Unavailable"; weatherDesc = "Waiting to retry"; }
    return true;
  }
  if (!weatherDataValid) { weatherMain = "Loading"; weatherDesc = "Fetching weather"; }
  DBG("[WX] request current: %s\n", city.c_str());
  return true;
}

void requestForecast() {
  if (apiKey.isEmpty() || city.isEmpty() || !weatherCoordinatesValid) { weatherFetchState = W_IDLE; return; }
  weatherForecastError = 0;
  String coords = "lat=" + String(weatherLatitude, 6) + "&lon=" + String(weatherLongitude, 6);
  String url = "http://api.openweathermap.org/data/2.5/forecast?" + coords +
               "&appid=" + encodeQueryValue(apiKey) + "&units=metric&cnt=24";
  if (!startWeatherRequest(url, WX_FORECAST)) {
    weatherFetchState = W_IDLE;
    weatherForecastValid = false;
    weatherForecastError = -4;
    DBG("[WX] forecast request not started\n");
  }
}

void pollWeather() {
  if (!weatherResultMutex) return;
  int code;
  WeatherRequestType type;
  uint32_t generation;
  String payload;
  xSemaphoreTake(weatherResultMutex, portMAX_DELAY);
  if (!weatherResultReady) {
    xSemaphoreGive(weatherResultMutex);
    return;
  }
  code = weatherResultCode;
  type = weatherResultType;
  generation = weatherResultGeneration;
  payload = weatherResultPayload;
  weatherResultPayload = "";
  weatherResultReady = false;
  xSemaphoreGive(weatherResultMutex);

  if (generation != weatherGeneration) {
    lastWeatherUpdate = 0;
    return;
  }
  WeatherFetchState expectedState = type == WX_GEOCODE ? W_GEOCODING :
                                   (type == WX_FORECAST ? W_FETCHING_FORECAST : W_FETCHING_CURRENT);
  if (weatherFetchState != expectedState) return;
  bool forecast = type == WX_FORECAST;
  weatherHttpCode = code;
  if (code != 200) {
    weatherFetchState = W_IDLE;
    if (forecast) { weatherForecastValid = false; weatherForecastError = code; }
    else {
      if (type == WX_GEOCODE) { weatherCoordinatesValid = false; weatherForecastError = code; }
      weatherDataValid = false;
      weatherMain = "Unavailable";
      if (code <= 0) weatherDesc = "Network error";
      else { weatherDesc = "HTTP "; weatherDesc += String(code); }
    }
    DBG("[WX] %s failed code=%d\n", forecast ? "forecast" : "current", code);
    return;
  }

  JSONVar response = JSON.parse(payload);
  if (JSON.typeof(response) == "undefined") {
    weatherFetchState = W_IDLE;
    if (forecast) { weatherForecastValid = false; weatherForecastError = -1; }
    else { weatherDataValid = false; weatherMain = "Unavailable"; weatherDesc = "Bad response"; }
    DBG("[WX] invalid JSON (%s)\n", forecast ? "forecast" : "current");
    return;
  }

  if (type == WX_GEOCODE) {
    if (JSON.typeof(response[0]) == "undefined" ||
        JSON.typeof(response[0]["lat"]) == "undefined" ||
        JSON.typeof(response[0]["lon"]) == "undefined") {
      if (!weatherGeocodeRetriedWithoutCountry) {
        weatherGeocodeRetriedWithoutCountry = true;
        weatherFetchState = W_IDLE;
        requestWeather();
        return;
      }
      weatherFetchState = W_IDLE;
      weatherCoordinatesValid = false;
      weatherDataValid = false;
      weatherForecastValid = false;
      weatherForecastError = 404;
      weatherHttpCode = 404;
      weatherMain = "Unavailable";
      weatherDesc = "City not found";
      return;
    }
    weatherLatitude = double(response[0]["lat"]);
    weatherLongitude = double(response[0]["lon"]);
    weatherCoordinatesValid = true;
    weatherFetchState = W_IDLE;
    weatherMain = "Loading";
    weatherDesc = "Fetching weather";
    requestWeather();
    return;
  }

  if (!forecast) {
    if (JSON.typeof(response["main"]) == "undefined" ||
        JSON.typeof(response["weather"]) == "undefined" ||
        JSON.typeof(response["weather"][0]["main"]) == "undefined") {
      weatherFetchState = W_IDLE;
      weatherDataValid = false;
      weatherMain = "Unavailable";
      weatherDesc = "Bad response";
      return;
    }
    if (JSON.typeof(response["dt"]) != "undefined") {
      long epoch = (long)response["dt"];
      if (epoch > 1700000000) {
        struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        if (!ntpSynced) { ntpSynced = true; DBG("[TIME] Synced via OpenWeatherMap dt: %ld\n", epoch); }
      }
    }
    temperature = double(response["main"]["temp"]);
    feelsLike = double(response["main"]["feels_like"]);
    humidity = int(response["main"]["humidity"]);
    weatherMain = (const char*)response["weather"][0]["main"];
    weatherDesc = (const char*)response["weather"][0]["description"];
    if (weatherDesc.length() > 0) weatherDesc[0] = toupper(weatherDesc[0]);
    if (JSON.typeof(response["sys"]) != "undefined") {
      if (JSON.typeof(response["sys"]["sunrise"]) != "undefined") sunriseEpoch = (long)response["sys"]["sunrise"];
      if (JSON.typeof(response["sys"]["sunset"]) != "undefined") sunsetEpoch = (long)response["sys"]["sunset"];
    }
    weatherDataValid = true;
    DBG("[WX] current %s %.1fC\n", weatherMain.c_str(), temperature);
    requestForecast();
    return;
  }

  int count = int(response["cnt"]);
  if (count < 1 || JSON.typeof(response["list"]) == "undefined") {
    weatherFetchState = W_IDLE;
    weatherForecastValid = false;
    weatherForecastError = -2;
    DBG("[WX] forecast has no entries\n");
    return;
  }
  const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  struct tm today;
  if (!getLocalTime(&today, 5)) {
    weatherFetchState = W_IDLE;
    weatherForecastValid = false;
    weatherForecastError = -3;
    return;
  }
  const int indices[3] = { 7, 15, 23 };
  for (int i = 0; i < 3; i++) {
    int idx = min(indices[i], count - 1);
    fcast[i].temp = (int)double(response["list"][idx]["main"]["temp"]);
    fcast[i].iconType = (const char*)response["list"][idx]["weather"][0]["main"];
    fcast[i].dayName = days[(today.tm_wday + i + 1) % 7];
  }
  weatherForecastValid = true;
  weatherForecastError = 0;
  weatherFetchState = W_IDLE;
  DBG("[WX] forecast ok\n");
}

// 19. NTP & WIFI HELPERS
void tryNtpSync() {
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  if (tzString.length() >= 3 && tzString.length() < 40) {
    setenv("TZ", tzString.c_str(), 1);
    tzset();
  } else {
    setenv("TZ", "IST-5:30", 1);
    tzset();
  }
  lastNtpAttempt = millis();
  ntpAttemptsCount++;
  DBG("[NTP] request #%u (tz=%s)\n", ntpAttemptsCount, tzString.c_str());
}

void syncTimeViaHttp() {
  if (WiFi.status() != WL_CONNECTED || ntpSynced) return;
  unsigned long now = millis();
  if (lastHttpTimeSyncAttempt != 0 && now - lastHttpTimeSyncAttempt < 600000UL) return;
  lastHttpTimeSyncAttempt = now;
  HTTPClient http;
  http.begin("http://worldtimeapi.org/api/ip");
  http.setTimeout(1200);
  http.setConnectTimeout(1200);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    JSONVar doc = JSON.parse(payload);
    if (JSON.typeof(doc) != "undefined" && doc.hasOwnProperty("unixtime")) {
      long epoch = (long)double(doc["unixtime"]);
      if (epoch > 1700000000) {
        struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        ntpSynced = true;
        DBG("[TIME] Synced via WorldTimeAPI: %ld\n", epoch);
        http.end();
        return;
      }
    }
  }
  http.end();

  if (!ntpSynced && WiFi.status() == WL_CONNECTED) {
    http.begin("http://timeapi.io/api/time/current/zone?timeZone=UTC");
    http.setTimeout(1200);
    http.setConnectTimeout(1200);
    httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      JSONVar doc = JSON.parse(payload);
      if (JSON.typeof(doc) != "undefined" && doc.hasOwnProperty("year")) {
        struct tm st;
        st.tm_year = int(doc["year"]) - 1900;
        st.tm_mon  = int(doc["month"]) - 1;
        st.tm_mday = int(doc["day"]);
        st.tm_hour = int(doc["hour"]);
        st.tm_min  = int(doc["minute"]);
        st.tm_sec  = int(doc["seconds"]);
        st.tm_isdst = 0;
        setenv("TZ", "UTC0", 1);
        tzset();
        time_t ep = mktime(&st);
        String restoreTz = tzString.isEmpty() ? "IST-5:30" : tzString;
        setenv("TZ", restoreTz.c_str(), 1);
        tzset();
        if (ep > 1700000000) {
          struct timeval tv = { .tv_sec = ep, .tv_usec = 0 };
          settimeofday(&tv, NULL);
          ntpSynced = true;
          DBG("[TIME] Synced via TimeAPI: %ld\n", (long)ep);
        }
      }
    }
    http.end();
  }
}

void updateNtp() {
  struct tm t;
  if (getLocalTime(&t, 5)) {
    if (!ntpSynced) { DBG("[NTP] synced %02d:%02d\n", t.tm_hour, t.tm_min); ntpSynced = true; }
    if (pendingTouchEpochUpdate) {
      time_t syncedEpoch = time(nullptr);
      if (syncedEpoch > 1600000000) {
        personality.lastTouchEpoch = (uint32_t)syncedEpoch;
        personality.lastSadStreakEpoch = 0;
        personalityDirty = true;
        pendingTouchEpochUpdate = false;
      }
    }
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (lastNtpAttempt == 0 || millis() - lastNtpAttempt > (ntpSynced ? 3600000UL : 8000UL)) {
      DBG("[NTP] retry\n");
      tryNtpSync();
    }
    if (!ntpSynced && (millis() - lastNtpAttempt > 3500UL)) {
      syncTimeViaHttp();
    }
  }
}

void updateWifiSta() {
  if (!ntpSynced && currentPage == 1 && wifiStaState == WIFI_STA_IDLE) {
    if (wifiStaStart > millis() + 5000UL) wifiStaStart = millis() + 5000UL;
  }

  switch (wifiStaState) {
    case WIFI_STA_IDLE: {
      if (millis() >= wifiStaStart && savedWifiCount > 0) {
        DBG("[WIFI] Scanning air for known networks...\n");
        WiFi.scanNetworks(true, false);
        wifiStaState = WIFI_STA_SCANNING;
        wifiStaStart = millis();
      }
      break;
    }
    case WIFI_STA_SCANNING: {
      int n = WiFi.scanComplete();
      if (n == WIFI_SCAN_RUNNING) {
        if (millis() - wifiStaStart > 15000UL) {
          WiFi.scanDelete();
          wifiStaState = WIFI_STA_IDLE;
          wifiStaStart = millis() + 5000UL;
          DBG("[WIFI] Scan timed out\n");
        }
        break;
      }
      int bestIdx = -1;
      int bestRssi = -999;
      if (n > 0) {
        for (int i = 0; i < n; i++) {
          String scannedSsid = WiFi.SSID(i);
          int scannedRssi = WiFi.RSSI(i);
          for (int k = 0; k < savedWifiCount; k++) {
            if (scannedSsid.equals(savedWifiList[k].ssid) && scannedRssi > bestRssi) {
              bestRssi = scannedRssi;
              bestIdx = k;
            }
          }
        }
      }
      WiFi.scanDelete();
      if (bestIdx >= 0) {
        currentWifiSlot = bestIdx;
        currentConnectedSsid = savedWifiList[bestIdx].ssid;
        WiFi.begin(savedWifiList[bestIdx].ssid.c_str(), savedWifiList[bestIdx].pass.c_str());
        wifiStaState = WIFI_STA_CONNECTING;
        wifiStaStart = millis();
        DBG("[WIFI] Connecting to matched network: %s\n", currentConnectedSsid.c_str());
      } else {
        wifiStaState = WIFI_STA_IDLE;
        unsigned long retryInterval = ntpSynced ? WIFI_RETRY_INTERVAL_MS : 15000UL;
        wifiStaStart = millis() + retryInterval;
        DBG("[WIFI] No known networks in range. %s\n", ntpSynced ? "Sleeping 5 min..." : "Retrying in 15s for time sync...");
      }
      break;
    }
    case WIFI_STA_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        wifiStaState = WIFI_STA_CONNECTED;
        DBG("[WIFI] connected to %s, IP=%s\n", currentConnectedSsid.c_str(), WiFi.localIP().toString().c_str());
        lastNtpAttempt = 0;
        tryNtpSync();
        if (weatherFetchState == W_IDLE) requestWeather();
      } else if (millis() - wifiStaStart > 12000UL) {
        DBG("[WIFI] connection timeout on %s\n", currentConnectedSsid.c_str());
        WiFi.disconnect(true);
        wifiStaState = WIFI_STA_IDLE;
        unsigned long retryInterval = ntpSynced ? WIFI_RETRY_INTERVAL_MS : 15000UL;
        wifiStaStart = millis() + retryInterval;
      }
      break;
    case WIFI_STA_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        DBG("[WIFI] connection lost! Will retry\n");
        WiFi.disconnect(true);
        wifiStaState = WIFI_STA_IDLE;
        unsigned long retryInterval = ntpSynced ? WIFI_RETRY_INTERVAL_MS : 15000UL;
        wifiStaStart = millis() + retryInterval;
      }
      break;
  }
}

// 20. REACTION ENGINE
void playReactionBasedOnContext(unsigned long offDuration) {
  struct tm t;
  bool haveTime = getLocalTime(&t, 5);
  int hour = haveTime ? t.tm_hour : 12;
  bool morning = (hour >= 6 && hour < 12);
  bool evening = (hour >= 17 && hour < 21);
  bool night = (hour >= 21 || hour < 6);
  BehaviorState welcome = BEHAVIOR_SURPRISED;
  unsigned long duration = 1000;
  if (offDuration > 3600000UL) {
    if (personality.familiarity < 12) welcome = BEHAVIOR_SHY;
    else if (night) welcome = BEHAVIOR_GROGGY;
    else if (morning) welcome = BEHAVIOR_CURIOUS;
    else welcome = BEHAVIOR_HAPPY;
    duration = GROGGY_DURATION_MS;
    if (night && personality.energy < 35) welcome = BEHAVIOR_SLEEPY;
  } else if (offDuration > 300000UL) {
    if (personality.familiarity < 12) welcome = BEHAVIOR_SHY;
    else if (personality.irritability > 55) welcome = BEHAVIOR_FED_UP;
    else welcome = BEHAVIOR_CURIOUS;
    duration = 2500;
  } else {
    duration = 900;
  }
  if (evening && welcome == BEHAVIOR_HAPPY) {
    welcome = BEHAVIOR_CONTENT;
    duration = 2000;
  }
  setBehavior(welcome, duration, true);
}

// 21. SETUP
void playBootAnimation() {
  display.setTextColor(SSD1306_WHITE);
  int cx = 64, cy = 32;
  for (int r = 0; r < 75; r += 5) { display.clearDisplay(); display.fillCircle(cx, cy, r, SSD1306_WHITE); display.display(); delay(8); }
  for (int r = 0; r < 75; r += 5) { display.clearDisplay(); display.fillCircle(cx, cy, 75, SSD1306_WHITE); display.fillCircle(cx, cy, r, SSD1306_BLACK); display.display(); delay(8); }
  display.setFont(&FreeSansBold9pt7b);
  String bt = "Kibo";
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(bt, 0, 0, &x1, &y1, &w, &h);
  display.clearDisplay(); display.setCursor((SCREEN_WIDTH - w) / 2, 38); display.print(bt);
  display.display(); delay(1200);
}
void initI2CAndDisplay() {
  Wire.begin(SDA_PIN_DEFAULT, SCL_PIN_DEFAULT);
  Wire.beginTransmission(OLED_ADDR);
  if (Wire.endTransmission() != 0) {
    DBG("[I2C] try 8/9\n");
    Wire.end(); Wire.begin(SDA_PIN_ALT, SCL_PIN_ALT);
  }
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[ERROR] SSD1306 fail"); while (true) delay(1000);
  }
  display.clearDisplay(); display.display();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
}
void setup() {
  Serial.begin(115200);
  delay(100);
  weatherResultMutex = xSemaphoreCreateMutex();
  bootMillis = millis();
  DBG("\n[BOOT] Kibo Phase 3.1\n");
  pinMode(TOUCH_PIN, INPUT);
  DBG("[BOOT] TOUCH_PIN initial read: %d\n", digitalRead(TOUCH_PIN));
  initI2CAndDisplay();
  for (int i = 0; i < 4; i++) hearts[i].active = false;
  for (int i = 0; i < 8; i++) sparkles[i].active = false;
  for (int i = 0; i < RAIN_DROPS; i++) raindrops[i].active = false;
  bool forceSetup = false;
  for (unsigned long t = millis(); millis() - t < CONFIG_HOLD_MS; ) {
    if ((TOUCH_ACTIVE_HIGH && digitalRead(TOUCH_PIN) == HIGH) ||
        (!TOUCH_ACTIVE_HIGH && digitalRead(TOUCH_PIN) == LOW)) { forceSetup = true; break; }
    delay(50);
  }
  loadConfig(); loadPersonality(); loadBirthdays();
  currentPage = 0;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(CONFIG_AP_SSID, apPassword.c_str());
  setupWebServerRoutes(); server.begin();
  DBG("[AP] Kibo-Setup @ 192.168.4.1\n");
  if (forceSetup || savedWifiCount == 0) { inSetupScreen = true; renderQRSetupScreen(); return; }
  leftEye.init(18, 14, 36, 36); rightEye.init(74, 14, 36, 36);
  playBootAnimation();
  wifiStaState = WIFI_STA_IDLE; wifiStaStart = millis(); lastNtpAttempt = 0;
  if (personality.lastBehavior != BEHAVIOR_SLEEPING && personality.lastBehavior != BEHAVIOR_AWAKE) {
    setBehavior(BEHAVIOR_GROGGY, GROGGY_DURATION_MS, true); behaviorFallback = BEHAVIOR_AWAKE;
  } else behaviorState = BEHAVIOR_AWAKE;
  weatherRefreshInterval = WEATHER_REFRESH_MS + random(0, 60000);
  lastWeatherUpdate = millis(); lastInteractionMillis = millis();
  lastFaceActiveAt = millis(); behaviorEnteredAt = millis();
  lastEnergyTick = millis(); lastHeapLog = millis();
  nextGestureRoll = millis() + random(GESTURE_ROLL_MIN_MS, GESTURE_ROLL_MAX_MS);
}

// 22. MAIN LOOP
void loop() {
  server.handleClient();
  if (inSetupScreen) {
    renderQRSetupScreen();
    if ((TOUCH_ACTIVE_HIGH && digitalRead(TOUCH_PIN) == HIGH) ||
        (!TOUCH_ACTIVE_HIGH && digitalRead(TOUCH_PIN) == LOW)) {
      delay(300); inSetupScreen = false; currentPage = 0;
    }
    return;
  }
  unsigned long now = millis();
  updateWifiSta();
  updateNtp();
  if (now - lastHeapLog > HEAP_LOG_INTERVAL_MS) { lastHeapLog = now; DBG("[HEAP] %u\n", (unsigned)ESP.getFreeHeap()); }
  handleTouchInput();
  if (currentPage != lastRenderedPage) {
    lastRenderedPage = currentPage;
    pageTransitionAt = now;
  }
  updateBehaviorEngine();
  updateGestureBank();
  updateMiniGame();
  updateRainOverlay();
  updateLightning();
  if (!isSleepingNow && weatherFetchState == W_IDLE && (WiFi.status() == WL_CONNECTED) &&
      (lastWeatherUpdate == 0 || now - lastWeatherUpdate > weatherRefreshInterval)) {
    if (requestWeather()) lastWeatherUpdate = now;
  }
  pollWeather();
  savePersonality(false);
  display.clearDisplay();
  if (isSleepingNow || behaviorState == BEHAVIOR_SLEEPING) {
    drawSleepFace();
  } else if (lightningActive) {
    drawLightning();
  } else {
    switch (currentPage) {
      case 0: drawEmoPage(); break;
      case 1: drawTimePage(); break;
      case 2: drawWeatherPage(); break;
      case 3: drawForecastPage(); break;
      case 4: drawWorldClock(); break;
      case 5: drawMiniGamePage(); break;
      default: currentPage = 0; drawEmoPage(); break;
    }
  }
  unsigned long transitionElapsed = millis() - pageTransitionAt;
  if (pageTransitionAt != 0 && transitionElapsed < 180UL && !isSleepingNow) {
    float progress = constrain(transitionElapsed / 180.0f, 0.0f, 1.0f);
    float eased = progress * progress * (3.0f - 2.0f * progress);
    int curtainWidth = (int)(SCREEN_WIDTH * (1.0f - eased));
    if (curtainWidth > 0) {
      display.fillRect(0, 0, curtainWidth, SCREEN_HEIGHT, SSD1306_BLACK);
      display.drawLine(curtainWidth, 0, curtainWidth, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    }
  }
  display.display();
  if (currentPage == 5 && gameRunning) delay(8);
}
