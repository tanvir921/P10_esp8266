# P10 Display Wiring Guide for ESP8266

Based on your PCB images, here's how to connect your P10 display to the ESP8266:

## P10 Display Connector Pinout
Looking at your P10 display connector (16-pin):

```
P10 Pin | Function | ESP8266 Pin | NodeMCU Pin
--------|----------|-------------|------------
1       | R1       | Not used    | -
2       | GND      | GND         | GND
3       | B1       | Not used    | -
4       | GND      | GND         | GND
5       | A        | GPIO5       | D1
6       | C        | GPIO15      | D8
7       | CLK      | GPIO14      | D5 (SPI CLK)
8       | STB/LAT  | GPIO16      | D0
9       | R0       | GPIO13      | D7 (SPI MOSI)
10      | GND      | GND         | GND
11      | B0       | Not used    | -
12      | GND      | GND         | GND
13      | B        | GPIO4       | D2
14      | D        | GPIO12      | D6
15      | OE       | GPIO2       | D4
16      | VCC      | 5V          | VU (5V)
```

## Power Requirements
- P10 displays require 5V power supply
- Can draw significant current (up to 2A for a single panel at full brightness)
- Use external 5V power supply for reliable operation
- Connect ESP8266 and P10 grounds together

## Important Notes
1. The code assumes a single 32x16 P10 panel
2. Timer interrupt is used for display refresh (required for PxMatrix)
3. Adjust pin definitions in code if using different GPIO pins
4. For multiple panels, modify matrix_width and matrix_height accordingly

## Testing
The example code will:
1. Display "Hello P10!" text
2. Show a moving dot animation
3. Print debug info to Serial Monitor (115200 baud)

## Troubleshooting
- If display is blank: Check power supply and wiring
- If display flickers: Verify timer interrupt is working
- If colors are wrong: Check RGB pin connections
- If text is garbled: Verify address pins (A, B, C, D)
