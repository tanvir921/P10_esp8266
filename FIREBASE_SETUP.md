# P10 Display with WiFiManager and Firebase Setup Guide

## Overview
This project creates a P10 LED display that:
- Uses WiFiManager for easy WiFi setup via web portal
- Connects to Firebase for dynamic text management
- Shows multiple sentences with selection capability
- Auto-reconnects and shows all status on display
- Opens configuration portal when WiFi disconnects

## Initial Setup

### Step 1: First Boot
1. Upload the code to your ESP8266
2. The display will show "Starting WiFi Setup..."
3. ESP8266 will create a WiFi hotspot named "P10_Display_Setup"
4. Connect your phone/computer to this hotspot
5. A captive portal should open automatically (or go to 192.168.4.1)

### Step 2: WiFi Configuration
1. In the captive portal, click "Configure WiFi"
2. Select your WiFi network and enter password
3. Scroll down to see Firebase configuration fields:
   - **Firebase Host**: your-project-id-default-rtdb.firebaseio.com (without https://)
   - **Firebase Auth**: your-database-secret-token
4. Click "Save" to connect

### Step 3: Firebase Project Setup

#### Create Firebase Project
1. Go to [Firebase Console](https://console.firebase.google.com/)
2. Click "Create a project"
3. Enter project name (e.g., "p10-display")
4. Follow the setup wizard

#### Set up Realtime Database
1. In Firebase Console, go to "Realtime Database"
2. Click "Create Database"
3. Choose "Start in test mode" for now
4. Select your preferred location

#### Get Firebase Credentials
1. Your Firebase Host is: `your-project-id-default-rtdb.firebaseio.com`
2. For auth token:
   - Go to Project Settings (gear icon)
   - Click "Service accounts" tab
   - Click "Database secrets"
   - Copy the secret key

#### Set up Database Structure
Create this structure in your Firebase Realtime Database:

```json
{
  "display": {
    "sentences": [
      "Welcome to P10 Display!",
      "Firebase connected successfully",
      "Change text from Firebase console",
      "Multiple sentences supported",
      "Real-time updates work!"
    ],
    "selectedSentence": 0
  }
}
```

## Database Structure Explained

### `/display/sentences` (Array)
- Array of text strings to display
- Index 0, 1, 2, etc.
- Can have up to 10 sentences
- Each sentence will scroll across the display

### `/display/selectedSentence` (Number)
- Which sentence to currently display
- 0 = first sentence, 1 = second sentence, etc.
- Change this value to switch between sentences
- Updates every 10 seconds

## Usage

### Normal Operation
1. Device connects to WiFi automatically
2. Shows "WiFi Connected!" with IP address
3. Connects to Firebase and shows "Firebase Connected!"
4. Starts scrolling the selected sentence
5. Updates from Firebase every 10 seconds

### Changing Text
1. Go to Firebase Console → Realtime Database
2. Navigate to `/display/sentences`
3. Edit any sentence in the array
4. Change `/display/selectedSentence` to pick which one to show
5. Display updates within 10 seconds

### Adding New Sentences
1. In Firebase Console, add new entries to the sentences array
2. Index them as 0, 1, 2, 3, etc.
3. Set `selectedSentence` to the index you want to display

### WiFi Reconnection
- If WiFi disconnects, display shows "WiFi Disconnected!"
- Device automatically opens "P10_Display_Setup" hotspot
- Reconnect and reconfigure as needed
- Firebase reconnects automatically after WiFi is restored

## Status Messages
The display shows various status messages:
- "P10 Display Starting..." - Initial boot
- "Starting WiFi Setup..." - WiFiManager initializing
- "Connecting to WiFi..." - Connecting to saved network
- "Config Mode" - Configuration portal active
- "WiFi Connected!" - Successful WiFi connection
- "Connecting Firebase..." - Firebase initialization
- "Firebase Connected!" - Firebase ready
- "Firebase Failed!" - Check credentials
- "WiFi Disconnected!" - Connection lost

## Troubleshooting

### WiFi Issues
- Device creates "P10_Display_Setup" hotspot when no WiFi
- Connect to hotspot and configure via web portal
- Portal timeout is 5 minutes, then device restarts

### Firebase Issues
- Check Firebase host format (no https:// prefix)
- Verify auth token is correct
- Ensure database rules allow read access
- Check Serial Monitor (115200 baud) for debug info

### Display Issues
- All status messages appear on display
- Serial monitor provides detailed debugging
- Device restarts if configuration fails

## Example Firebase Rules
For testing, use these permissive rules (tighten for production):

```json
{
  "rules": {
    ".read": true,
    ".write": true
  }
}
```

## Advanced Features
- Supports up to 10 sentences in the array
- Real-time updates from Firebase console
- Automatic WiFi reconnection
- Visual status feedback on display
- Serial debugging at 115200 baud
