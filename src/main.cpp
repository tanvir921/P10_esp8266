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
#include <fonts/ElektronMart6x8.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include <ESP8266WebServer.h>

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
void ScrollingText(int y, uint8_t speed);
void initWiFiManager();
void initFirebase();
void updateTextFromFirebase();
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
const unsigned long firebaseUpdateInterval = 10000; // Update every 10 seconds
const unsigned long wifiCheckInterval = 5000; // Check WiFi every 5 seconds (faster reconnect)
const unsigned long wifiReconnectTimeout = 3600000; // 1 hour = 60 minutes * 60 seconds * 1000 milliseconds
bool firebaseConnected = false;
bool shouldSaveConfig = false;
bool wifiReconnecting = false;
unsigned long wifiDisconnectedTime = 0;

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

  // DMDESP Setup
  Disp.start(); // Start DMDESP library
  Disp.setBrightness(100); // Brightness level
  Disp.setFont(ElektronMart6x8); // Set font
  
  // Show startup message
  displayMessage("P10 Display Starting...");
  delay(2000);
  
  // Initialize WiFiManager
  initWiFiManager();
  
  // Initialize Firebase with hardcoded credentials
  initFirebase();
  
  Serial.println("Setup complete!");
}



//----------------------------------------------------------------------
// LOOP

void loop() {
  // Run display refresh
  Disp.loop(); 

  // Check WiFi connection periodically
  if (millis() - lastWiFiCheck > wifiCheckInterval) {
    checkWiFiConnection();
    lastWiFiCheck = millis();
  }

  // Update text from Firebase periodically (only if Firebase is connected)
  if (firebaseConnected && millis() - lastFirebaseUpdate > firebaseUpdateInterval) {
    updateTextFromFirebase();
    lastFirebaseUpdate = millis();
  }

  // Display scrolling text
  ScrollingText(0, 50); 
}


//--------------------------
// DISPLAY SCROLLING TEXT

void ScrollingText(int y, uint8_t speed) {

  static uint32_t pM;
  static uint32_t x;
  uint32_t width = Disp.width();
  uint32_t height = Disp.height();
  Disp.setFont(ElektronMart6x8);
  uint32_t fullScroll = Disp.textWidth(displayText.c_str()) + width;
  
  if((millis() - pM) > speed) { 
    pM = millis();
    if (x < fullScroll) {
      ++x;
    } else {
      x = 0;
    }
    
    // Clear the display before drawing new text
    Disp.clear();
    
    // Center the text vertically in the full display
    Disp.drawText(width - x, (height / 2) - 4, displayText.c_str());
  }  

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
// UPDATE TEXT FROM FIREBASE

void updateTextFromFirebase() {
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready!");
    return;
  }
  
  Serial.println("Attempting to fetch data from Firebase...");
  
  // Get the selected sentence index
  if (Firebase.RTDB.getInt(&fbdo, "/display/selectedSentence")) {
    int newSelected = fbdo.intData();
    Serial.println("Successfully read selectedSentence: " + String(newSelected));
    if (newSelected != selectedSentence && newSelected >= 0) {
      selectedSentence = newSelected;
      Serial.println("Selected sentence updated to: " + String(selectedSentence));
    }
  } else {
    Serial.println("Failed to read selectedSentence: " + fbdo.errorReason());
  }
  
  // Get sentences individually instead of as JSON array
  Serial.println("Attempting to read sentences individually...");
  totalSentences = 0;
  
  // Read each sentence individually from Firebase
  for (int i = 0; i < 10; i++) {
    String path = "/display/sentences/" + String(i);
    if (Firebase.RTDB.getString(&fbdo, path.c_str())) {
      String sentence = fbdo.stringData();
      if (sentence.length() > 0) {
        sentences[i] = sentence;
        totalSentences = i + 1;
        Serial.println("Sentence " + String(i) + ": " + sentences[i]);
      } else {
        break;
      }
    } else {
      Serial.println("No sentence found at path: " + path);
      break;
    }
  }
  
  Serial.println("Total sentences found: " + String(totalSentences));
  
  // Set display text based on selected sentence
  if (selectedSentence < totalSentences && totalSentences > 0) {
    String newText = sentences[selectedSentence];
    if (newText != displayText) {
      displayText = newText;
      Serial.println("Display text updated to: " + displayText);
    }
  } else if (totalSentences > 0) {
    // If selected sentence is out of range, use first sentence
    displayText = sentences[0];
    selectedSentence = 0;
    Serial.println("Selected sentence out of range, using first: " + displayText);
  } else {
    displayText = "No sentences configured";
    Serial.println("No sentences found in database!");
  }
}