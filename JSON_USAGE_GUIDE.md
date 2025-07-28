# Firebase JSON Examples for P10 Display

## Basic Example (`firebase_example.json`)
This is the minimal structure your P10 display code expects:

```json
{
  "display": {
    "sentences": [
      "Welcome to P10 Display!",
      "Firebase connected successfully",
      "Real-time text updates working",
      // ... more messages
    ],
    "selectedSentence": 0
  }
}
```

## Extended Example (`firebase_extended_example.json`)
This includes additional features you could implement:

```json
{
  "display": {
    "sentences": [...],
    "selectedSentence": 0,
    "settings": {
      "scrollSpeed": 50,
      "brightness": 100,
      "updateInterval": 10000
    },
    "status": {
      "lastUpdate": "2025-07-28T12:00:00Z",
      "deviceOnline": true,
      "wifiConnected": true,
      "firebaseConnected": true
    }
  }
}
```

## How to Import to Firebase:

### Method 1: Manual Import
1. Go to [Firebase Console](https://console.firebase.google.com/)
2. Select your project
3. Go to **Realtime Database**
4. Click the **3 dots menu** → **Import JSON**
5. Upload either `firebase_example.json` or `firebase_extended_example.json`

### Method 2: Manual Creation
1. In Realtime Database, click the **+** button
2. Add key: `display`
3. Under `display`, add:
   - `sentences` (array)
   - `selectedSentence` (number)
4. Add your messages to the sentences array

## Usage:

### Current Code Supports:
- ✅ `sentences` array (up to 10 messages)
- ✅ `selectedSentence` number (0-9)

### Future Enhancements (not implemented yet):
- ⏳ `settings.scrollSpeed` - Control scroll speed
- ⏳ `settings.brightness` - Control display brightness  
- ⏳ `settings.updateInterval` - Control Firebase update frequency
- ⏳ `status` - Device status reporting

## Real-time Control:

1. **Change Message**: Edit any sentence in the array
2. **Switch Message**: Change `selectedSentence` to 0, 1, 2, etc.
3. **Add Message**: Add new entry to sentences array
4. **Remove Message**: Delete entry from sentences array

## Testing:

1. Import the basic example JSON
2. Upload your P10 display code
3. Connect to "P10_Display_Setup" WiFi
4. Configure your Firebase credentials
5. Watch the display show "Welcome to P10 Display!"
6. Change `selectedSentence` to 1 in Firebase Console
7. Wait 10 seconds to see "Firebase connected successfully"

The display will automatically update every 10 seconds with any changes you make in the Firebase console!
