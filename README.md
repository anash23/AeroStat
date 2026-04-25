# AeroStat

AeroStat is a temperature-controlled fan system built with ESP32.  
It turns the fan ON when room temperature exceeds a user-defined threshold.

## Features

- ESP32-hosted web interface for fan control and threshold setup.
- User can manually turn the fan ON from the webpage.
- User can set temperature threshold from the webpage.
- Hardware fail-safe mode:
  - If ESP32 stops working, the analog fallback circuit automatically turns the fan ON above 30°C.

## Hardware Used

- ESP32
- LM35 temperature sensor
- LM358 op-amp
- MOSFET (fan switching)
- DHT11 

## Pin / Connection Notes

- LM358 pin 2 and pin 3 are connected to:
  - LM35 center pin (temperature signal)
  - ESP32 GPIO26

## Software Stack

- Firmware environment: Arduino IDE
- Libraries used:
  - `WiFi.h`
  - `DHT.h`
  - `WebServer.h`
- Webpage/UI was created with Claude and hosted directly on ESP32.

## How It Works

1. LM35 senses room temperature.
2. ESP32 reads temperature and serves a local control webpage.
3. User can set threshold temperature or force fan ON manually.
4. If the controller path fails, analog hardware fallback ensures fan turns ON automatically above 30°C.

## Author

Avinash Reddy
