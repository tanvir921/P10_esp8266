/*
 * P10 Display with WiFiManager and Firebase
 * Features:
 * - WiFiManager for easy WiFi setup via web portal
 * - Firebase integration with sentence list and selection
 * - Auto-reconnect and configuration portal on disconnect
 * - All status messages shown on display
 * 
 * email : bonny@grobak.net - www.grobak.net - www.elektronmart.com
*/

#include <DMDESP.h>
#include <fonts/ElektronMart6x12.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <time.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>
// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

// Firebase credentials (hardcoded for easier setup)
String firebase_host = "p10-esp8266-default-rtdb.firebaseio.com";
String firebase_auth = "PkQ6JzTVwIAcms8l2W6nDx9o3QR6wqmxVhcP3MjS";
String firebase_url = ""; // For persistent storage

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// WiFiManager
WiFiManager wm;

// Function declarations
bool ScrollingText(int y, uint8_t speed);
void displayDigitalClock();
void initTimeSync();
void initWiFiManager();
void initFirebase();
void updateTextFromFirebase();
void startFirebaseStream();
void handleFirebaseStream();
void saveDataToEEPROM();
void loadDataFromEEPROM();
void displayMessage(String message, bool clearFirst = true);
void checkWiFiConnection();
void saveConfigCallback();
void configModeCallback(WiFiManager *myWiFiManager);

// Global variables
String displayText = "Starting P10 Display..."; // Default text
String sentences[10]; // Array to store sentences from Firebase
int selectedSentence = 0; // Which sentence to display
int totalSentences = 0; // How many sentences are available
unsigned long lastFirebaseUpdate = 0;
unsigned long lastWiFiCheck = 0;
const unsigned long firebaseUpdateInterval = 10000; // Check every 10 seconds (more responsive)
const unsigned long wifiCheckInterval = 5000; // Check WiFi every 5 seconds (faster reconnect)
const unsigned long wifiReconnectTimeout = 3600000; // 1 hour = 60 minutes * 60 seconds * 1000 milliseconds
bool firebaseConnected = false;
bool shouldSaveConfig = false;
bool wifiReconnecting = false;
unsigned long wifiDisconnectedTime = 0;

// Firebase streaming variables
bool streamActive = false;
bool dataChanged = false;
unsigned long lastDataSave = 0;
const unsigned long dataSaveInterval = 5000; // Save to EEPROM every 5 seconds if changed

// Clock display variables
bool showClock = false; // Start with scrolling text first
unsigned long lastClockSwitch = 0;
const unsigned long clockDisplayTime = 10000; // Show clock for 10 seconds
bool colonBlink = true;
unsigned long lastColonBlink = 0;
const unsigned long colonBlinkInterval = 500; // Blink every 500ms
bool scrollCompleted = false;

// Time zone settings (adjust for your location)
const long gmtOffset_sec = 6 * 3600; // GMT+6 for Bangladesh (6 hours * 3600 seconds)
const int daylightOffset_sec = 0; // No daylight saving in Bangladesh

// EEPROM addresses
const int EEPROM_SIZE = 1024;
const int EEPROM_ADDR_SELECTED = 0;
const int EEPROM_ADDR_TOTAL = 4;
const int EEPROM_ADDR_SENTENCES = 8;

//SETUP DMD
#define DISPLAYS_WIDE 1 // Panel Columns
#define DISPLAYS_HIGH 1 // Panel Rows
DMDESP Disp(DISPLAYS_WIDE, DISPLAYS_HIGH);  // Number of P10 panels used (COLUMNS, ROWS)



//----------------------------------------------------------------------
// SETUP

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting P10 Display with WiFiManager and Firebase...");

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Load cached data from EEPROM first
  loadDataFromEEPROM();

  // DMDESP Setup
  Disp.start(); // Start DMDESP library
  Disp.setBrightness(100); // Brightness level
  Disp.setFont(ElektronMart6x12); // Set font
  
  // Show startup message
  displayMessage("P10 Display Starting...");
  delay(2000);
  
  // Initialize WiFiManager
  initWiFiManager();
  
  // Initialize time after WiFi connection
  initTimeSync();
  
  // Initialize Firebase with hardcoded credentials
  initFirebase();
  
  Serial.println("Setup complete!");
}



//----------------------------------------------------------------------
// LOOP

void loop() {
  // Always run display refresh first - this ensures continuous display
  Disp.loop(); 
  
  // Handle clock/text switching based on scroll completion
  if (!showClock) {
    // Show scrolling text and check if it completed
    bool completed = ScrollingText(0, 90);
    if (completed && !scrollCompleted) {
      // Scroll just completed, switch to clock
      scrollCompleted = true;
      showClock = true;
      lastClockSwitch = millis();
      Disp.clear();
    }
  } else {
    // Show clock and check if 6 seconds passed
    if (millis() - lastClockSwitch >= clockDisplayTime) {
      showClock = false;
      scrollCompleted = false;
      Disp.clear();
    } else {
      displayDigitalClock();
    }
  }

  // Check WiFi connection periodically
  if (millis() - lastWiFiCheck > wifiCheckInterval) {
    checkWiFiConnection();
    lastWiFiCheck = millis();
  }

  // Handle Firebase stream (lightweight, non-blocking)
  if (firebaseConnected) {
    if (!streamActive && millis() - lastFirebaseUpdate > firebaseUpdateInterval) {
      // Start Firebase stream
      startFirebaseStream();
      lastFirebaseUpdate = millis();
    } else if (streamActive) {
      // Handle stream events
      handleFirebaseStream();
    }
  }
  
  // Save data to EEPROM if changed (background task)
  if (dataChanged && millis() - lastDataSave > dataSaveInterval) {
    saveDataToEEPROM();
    dataChanged = false;
    lastDataSave = millis();
  }
  
  // Small delay to prevent overwhelming the system
  delay(1);
}


//--------------------------
// DISPLAY SCROLLING TEXT

bool ScrollingText(int y, uint8_t speed) {

  static uint32_t pM;
  static uint32_t x;
  static String lastDisplayText = "";
  static bool needsRedraw = true;
  
  uint32_t width = Disp.width();  // 32 pixels wide
  uint32_t height = Disp.height(); // 16 pixels high
  Disp.setFont(ElektronMart6x12);
  uint32_t textWidth = Disp.textWidth(displayText.c_str());
  
  // Start from center and scroll to the left, then wrap around to right side
  uint32_t centerStart = width / 2; // Start from center (pixel 16)
  uint32_t fullScroll = textWidth + width; // Total scroll distance
  
  // Check if text has changed
  if (displayText != lastDisplayText) {
    lastDisplayText = displayText;
    needsRedraw = true;
    x = 0; // Reset scroll position when text changes
  }
  
  bool scrollComplete = false;
  
  if((millis() - pM) > speed) { 
    pM = millis();
    if (x < fullScroll) {
      ++x;
    } else {
      x = 0;
      scrollComplete = true; // One complete scroll cycle finished
    }
    needsRedraw = true;
  }
  
  // Only clear and redraw when necessary
  if (needsRedraw) {
    Disp.clear();
    
    // Calculate text position: start from center, move left
    int32_t textX = centerStart - x;
    
    // If text has moved completely off the left side, wrap it to the right side
    if (textX + textWidth < 0) {
      textX = width + (textX + textWidth);
    }
    
    // Center the text vertically in the display
    int textY = (height / 2) - 6; // Adjust for font height
    
    Disp.drawText(textX, textY, displayText.c_str());
    needsRedraw = false;
  }
  
  return scrollComplete;
}

//--------------------------
// DISPLAY MESSAGE HELPER

void displayMessage(String message, bool clearFirst) {
  if (clearFirst) {
    Disp.clear();
  }
  Serial.println(message);
  
  // Split long messages into multiple lines if needed
  if (message.length() <= 16) {
    Disp.drawText(0, 4, message.c_str());
  } else {
    // Show first part, then scroll if needed
    Disp.drawText(0, 4, message.substring(0, 16).c_str());
  }
}

//--------------------------
// WIFIMANAGER INITIALIZATION

void initWiFiManager() {
  displayMessage("Starting WiFi Setup...");
  
  // Set callback for saving config
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setAPCallback(configModeCallback);
  
  // Set timeout for config portal
  wm.setConfigPortalTimeout(300); // 5 minutes timeout
  
  // Try to connect to saved WiFi
  displayMessage("Connecting to WiFi...");
  
  if (!wm.autoConnect("P10_Display_Setup")) {
    displayMessage("Failed to connect");
    displayMessage("Starting config portal");
    delay(2000);
    
    // Start config portal
    if (!wm.startConfigPortal("P10_Display_Setup")) {
      displayMessage("Config timeout");
      delay(3000);
      ESP.restart();
    }
  }
  
  // If we get here, we're connected
  displayMessage("WiFi Connected!");
  displayMessage("IP: " + WiFi.localIP().toString());
  delay(2000);
  
  Serial.println("WiFi connected successfully!");
  Serial.println("IP address: " + WiFi.localIP().toString());
}

//--------------------------
// WIFIMANAGER CALLBACKS

void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void configModeCallback(WiFiManager *myWiFiManager) {
  displayMessage("Config Mode");
  displayMessage("Connect to: P10_Display_Setup");
  delay(2000);
  displayMessage("Open: " + WiFi.softAPIP().toString());
  Serial.println("Entered config mode");
  Serial.println("AP IP: " + WiFi.softAPIP().toString());
}

//--------------------------
// WIFI CONNECTION CHECK

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    if (!wifiReconnecting) {
      // First time detecting disconnection
      wifiReconnecting = true;
      wifiDisconnectedTime = millis();
      displayMessage("WiFi Disconnected!");
      firebaseConnected = false;
      
      Serial.println("WiFi disconnected, starting 1-hour reconnection attempts...");
    }
    
    // Calculate how long we've been disconnected
    unsigned long disconnectedDuration = millis() - wifiDisconnectedTime;
    unsigned long remainingTime = wifiReconnectTimeout - disconnectedDuration;
    
    if (disconnectedDuration < wifiReconnectTimeout) {
      // Still within 1-hour window, keep trying to reconnect
      int minutesRemaining = remainingTime / 60000;
      int secondsRemaining = (remainingTime % 60000) / 1000;
      
      displayMessage("Reconnecting WiFi...");
      Serial.println("Attempting WiFi reconnection... Time remaining: " + 
                    String(minutesRemaining) + "m " + String(secondsRemaining) + "s");
      
      // Try to reconnect to saved WiFi
      WiFi.reconnect();
      
      // Wait up to 10 seconds for this reconnection attempt
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        // Successful reconnection
        displayMessage("WiFi Reconnected!");
        Serial.println("\nWiFi reconnected successfully after " + 
                      String(disconnectedDuration / 1000) + " seconds!");
        delay(2000);
        
        // Reset flags and reinitialize Firebase
        wifiReconnecting = false;
        wifiDisconnectedTime = 0;
        initFirebase();
      } else {
        Serial.println(" failed.");
        // Show countdown on display
        displayMessage("Retry in " + String(minutesRemaining) + "m " + String(secondsRemaining) + "s");
      }
    } else {
      // 1 hour has passed, reset WiFi credentials and open config portal
      Serial.println("\n1 hour reconnection timeout reached. Resetting WiFi credentials...");
      displayMessage("WiFi Reset Required");
      delay(2000);
      
      displayMessage("Clearing WiFi Settings...");
      
      // Clear saved WiFi credentials
      wm.resetSettings();
      
      delay(2000);
      displayMessage("Starting WiFi Setup...");
      
      // Reset flags
      wifiReconnecting = false;
      wifiDisconnectedTime = 0;
      
      // Start config portal with fresh settings
      if (!wm.startConfigPortal("P10_Display_Setup")) {
        displayMessage("Config timeout");
        delay(3000);
        ESP.restart();
      } else {
        displayMessage("WiFi Reconfigured!");
        delay(2000);
        initFirebase();
      }
    }
  } else {
    // WiFi is connected, ensure reconnecting flags are cleared
    if (wifiReconnecting) {
      wifiReconnecting = false;
      wifiDisconnectedTime = 0;
    }
  }
}

//--------------------------
// FIREBASE INITIALIZATION

void initFirebase() {
  displayMessage("Connecting Firebase...");
  
  if (firebase_host.length() == 0 || firebase_auth.length() == 0) {
    displayMessage("Firebase not configured");
    displayText = "Use WiFi portal to set Firebase";
    return;
  }
  
  // Configure Firebase - store in persistent strings
  firebase_url = "https://" + firebase_host;
  config.database_url = firebase_url.c_str();
  config.signer.tokens.legacy_token = firebase_auth.c_str();
  
  // Initialize Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  // Test connection
  if (Firebase.ready()) {
    firebaseConnected = true;
    displayMessage("Firebase Connected!");
    Serial.println("Firebase initialized successfully");
    
    // Load initial data
    updateTextFromFirebase();
  } else {
    firebaseConnected = false;
    displayMessage("Firebase Failed!");
    displayText = "Check Firebase credentials";
    Serial.println("Firebase connection failed");
  }
  
  delay(2000);
}

//--------------------------
// EEPROM DATA MANAGEMENT

void saveDataToEEPROM() {
  Serial.println("Saving data to EEPROM...");
  
  // Save selected sentence
  EEPROM.put(EEPROM_ADDR_SELECTED, selectedSentence);
  
  // Save total sentences
  EEPROM.put(EEPROM_ADDR_TOTAL, totalSentences);
  
  // Save sentences (each sentence max 80 chars)
  int addr = EEPROM_ADDR_SENTENCES;
  for (int i = 0; i < totalSentences && i < 10; i++) {
    String sentence = sentences[i];
    if (sentence.length() > 80) sentence = sentence.substring(0, 80);
    
    // Save length first
    uint8_t len = sentence.length();
    EEPROM.put(addr, len);
    addr += sizeof(uint8_t);
    
    // Save string data
    for (int j = 0; j < len; j++) {
      EEPROM.put(addr + j, sentence[j]);
    }
    addr += 80; // Fixed space per sentence
  }
  
  EEPROM.commit();
  Serial.println("Data saved to EEPROM");
}

void loadDataFromEEPROM() {
  Serial.println("Loading data from EEPROM...");
  
  // Load selected sentence
  EEPROM.get(EEPROM_ADDR_SELECTED, selectedSentence);
  
  // Load total sentences
  EEPROM.get(EEPROM_ADDR_TOTAL, totalSentences);
  
  // Validate data
  if (selectedSentence < 0 || selectedSentence > 9) selectedSentence = 0;
  if (totalSentences < 0 || totalSentences > 10) totalSentences = 0;
  
  // Load sentences
  int addr = EEPROM_ADDR_SENTENCES;
  for (int i = 0; i < totalSentences && i < 10; i++) {
    // Load length
    uint8_t len;
    EEPROM.get(addr, len);
    addr += sizeof(uint8_t);
    
    if (len > 80) len = 80; // Safety check
    
    // Load string data
    char buffer[81];
    for (int j = 0; j < len; j++) {
      EEPROM.get(addr + j, buffer[j]);
    }
    buffer[len] = '\0';
    sentences[i] = String(buffer);
    
    addr += 80; // Fixed space per sentence
  }
  
  // Set initial display text from cached data
  if (totalSentences > 0 && selectedSentence < totalSentences) {
    displayText = sentences[selectedSentence];
    Serial.println("Loaded cached display text: " + displayText);
  } else if (totalSentences > 0) {
    displayText = sentences[0];
    selectedSentence = 0;
    Serial.println("Using first cached sentence: " + displayText);
  } else {
    displayText = "Loading from Firebase...";
    Serial.println("No cached data found, will load from Firebase");
  }
  
  Serial.println("Loaded " + String(totalSentences) + " sentences from EEPROM");
}

//--------------------------
// FIREBASE STREAM FUNCTIONS

void startFirebaseStream() {
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready for streaming!");
    return;
  }
  
  Serial.println("Starting Firebase stream...");
  
  // Use a simple get request instead of complex streaming to reduce load
  updateTextFromFirebase();
}

void handleFirebaseStream() {
  // This function is kept for compatibility but not used in the simplified approach
  streamActive = false;
}

//--------------------------
// SIMPLIFIED FIREBASE UPDATE

void updateTextFromFirebase() {
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready!");
    return;
  }
  
  Serial.println("Quick Firebase update...");
  
  bool updated = false;
  
  // Always check for sentences first, especially on initial load
  static int updateCounter = 0;
  updateCounter++;
  
  // On first run or every 5th update, check for new sentences
  if (totalSentences == 0 || updateCounter >= 5) {
    updateCounter = 0;
    
    Serial.println("Checking for sentences in Firebase...");
    
    // Check all sentences to build the initial list
    int maxSentencesToCheck = (totalSentences == 0) ? 10 : 3; // Check all on first run, fewer on updates
    
    for (int i = 0; i < maxSentencesToCheck; i++) {
      String path = "/display/sentences/" + String(i);
      if (Firebase.RTDB.getString(&fbdo, path.c_str())) {
        String sentence = fbdo.stringData();
        if (sentence.length() > 0) {
          if (sentence != sentences[i]) {
            sentences[i] = sentence;
            updated = true;
            Serial.println("Updated sentence " + String(i) + ": " + sentence);
          }
          // Update total sentences count
          if (i >= totalSentences) {
            totalSentences = i + 1;
            updated = true;
          }
        } else if (i == 0) {
          // If first sentence is empty, there are no sentences
          Serial.println("No sentences found in Firebase");
          break;
        } else {
          // End of sentences found
          break;
        }
      } else {
        Serial.println("Failed to read sentence " + String(i) + ": " + fbdo.errorReason());
        if (i == 0) {
          // Can't read first sentence, might be a connection issue
          Serial.println("Cannot read Firebase data");
          return;
        }
        break;
      }
      delay(10); // Small delay between requests
    }
  }
  
  // Now check for selected sentence (after we have sentences loaded)
  if (Firebase.RTDB.getInt(&fbdo, "/display/selectedSentence")) {
    int newSelected = fbdo.intData();
    Serial.println("Firebase selectedSentence: " + String(newSelected) + ", current: " + String(selectedSentence));
    
    if (newSelected != selectedSentence && newSelected >= 0) {
      // If we don't have enough sentences loaded yet, but the selection changed, try to load that specific sentence
      if (newSelected >= totalSentences) {
        Serial.println("Loading additional sentence " + String(newSelected) + "...");
        String path = "/display/sentences/" + String(newSelected);
        if (Firebase.RTDB.getString(&fbdo, path.c_str())) {
          String sentence = fbdo.stringData();
          if (sentence.length() > 0) {
            sentences[newSelected] = sentence;
            totalSentences = max(totalSentences, newSelected + 1);
            updated = true;
            Serial.println("Loaded new sentence " + String(newSelected) + ": " + sentence);
          }
        }
      }
      
      // Now update the selected sentence if we have it
      if (newSelected < totalSentences && sentences[newSelected].length() > 0) {
        selectedSentence = newSelected;
        displayText = sentences[selectedSentence];
        updated = true;
        Serial.println("Updated selected sentence to: " + String(selectedSentence) + " - " + displayText);
      } else {
        Serial.println("Selected sentence " + String(newSelected) + " not available yet");
      }
    }
  } else {
    Serial.println("Failed to read selectedSentence: " + fbdo.errorReason());
  }
  
  // If display text is still loading message or default, set it to the selected sentence
  if (totalSentences > 0 && (displayText == "Loading from Firebase..." || displayText == "Starting P10 Display...")) {
    if (selectedSentence < totalSentences && sentences[selectedSentence].length() > 0) {
      displayText = sentences[selectedSentence];
      updated = true;
      Serial.println("Set initial display text: " + displayText);
    } else if (sentences[0].length() > 0) {
      displayText = sentences[0];
      selectedSentence = 0;
      updated = true;
      Serial.println("Using first sentence as default: " + displayText);
    }
  }
  
  // Only show error if we truly have no sentences
  if (totalSentences == 0) {
    Serial.println("No sentences available from Firebase");
    if (displayText == "Loading from Firebase..." || displayText == "Starting P10 Display...") {
      displayText = "No sentences in Firebase";
    }
  }
  
  if (updated) {
    dataChanged = true;
    Serial.println("Firebase update completed. Total sentences: " + String(totalSentences));
  }
}

//--------------------------
// TIME SYNCHRONIZATION

void initTimeSync() {
  displayMessage("Syncing time...");
  
  // Configure time with NTP servers
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  
  // Wait for time to be set
  int attempts = 0;
  while (!time(nullptr) && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (time(nullptr)) {
    displayMessage("Time synced!");
    Serial.println("\nTime synchronized successfully");
  } else {
    displayMessage("Time sync failed");
    Serial.println("\nFailed to sync time");
  }
  
  delay(1000);
}

//--------------------------
// DIGITAL CLOCK DISPLAY

void displayDigitalClock() {
  // Handle colon blinking
  if (millis() - lastColonBlink >= colonBlinkInterval) {
    colonBlink = !colonBlink;
    lastColonBlink = millis();
  }
  
  // Get current time
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  if (!timeinfo) {
    Disp.clear();
    Disp.setFont(ElektronMart6x12);
    Disp.drawText(2, 4, "TIME ERROR");
    return;
  }
  
  // Convert to 12-hour format
  int hour = timeinfo->tm_hour;
  int minute = timeinfo->tm_min;
  
  // Convert 24-hour to 12-hour format
  if (hour == 0) {
    hour = 12; // Midnight
  } else if (hour > 12) {
    hour = hour - 12; // Afternoon/Evening
  }
  
  // Format time components separately for fixed positioning
  String hourStr = (hour < 10) ? " " + String(hour) : String(hour);
  String minStr = (minute < 10) ? "0" + String(minute) : String(minute);
  
  // Clear display
  Disp.clear();
  Disp.setFont(ElektronMart6x12);
  
  // Calculate fixed positions for each component
  int hourWidth = Disp.textWidth(hourStr.c_str());
  int colonWidth = Disp.textWidth(":");
  int minWidth = Disp.textWidth(minStr.c_str());
  
  // Total width for centering
  int totalWidth = hourWidth + colonWidth + minWidth;
  int startX = (32 - totalWidth) / 2;
  int startY = (16 - 12) / 2;
  
  // Draw components at fixed positions
  Disp.drawText(startX, startY, hourStr.c_str());
  
  // Only draw colon when it should be visible
  if (colonBlink) {
    Disp.drawText(startX + hourWidth, startY, ":");
  }
  
  Disp.drawText(startX + hourWidth + colonWidth, startY, minStr.c_str());
}