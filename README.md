# ESP32 Fiat Lux
A WS2812 LED controller with webserver  
Select different LED strip color schemes from a website  
Use your LED strip as a clock.

## Features

- Wifi station connection to your local network
- Select different modes for let yor LED strip from a website
- SNTP get the current time from the internet  
  
<img width="32" height="32" src="website/zahnrad.svg" alt="settings"> Set number of LEDs, starting point, direction, ...  
<img width="32" height="32" src="website/rainbow.svg" alt="rainbow"> Rainbow colors  
<img width="32" height="32" src="website/clock-rainbow.svg" alt="rainbow"> blue in the morning, green in the afternoon, red in the night  
<img width="32" height="32" src="website/clock-rainbow2.svg" alt="rainbow"> rotating LEDs show the time  
<img width="32" height="32" src="website/walking-person.svg" alt="walking"> walking LEDs  
<img width="32" height="32" src="website/turtle-svgrepo-com.svg" alt="turtle"> rotate slower  
<img width="32" height="32" src="website/running_rabbit.svg" alt="rabbit"> rotate faster  

## Design philosophy

Keep it simple!  
This applies to the source code, the user interface and the documentation.  

The Espressif IDF with its examples provides all the basic functionality that an IoT device needs.  
This project makes also use of the standard C++ library, especially for better code structuring and string processing.  
The webserver makes use of static HTML pages and a few javascript files. There is no need for node.js, Angular, React or some other fancy javascript libraries
that replace one sort of complexity with onother one.
This project only copied the nice color picker wheel from [github.com/jaames/iro.js](https://github.com/jaames/iro.js).  
Javascript is good for "if you click here, do something there", but it should not be used for complex apps in my opinion.
HTML5 is good enough to build websites that look good at your PCs screen as well as your smartphone display.
All files in the folder [website](https://github.com/chbergmann/esp32-fiat-lux/tree/main/website) are loaded to a flash file system inside the chip.  

A picture is better than 1000 words. I tried to use as few text in the user interface as possible.

## Requirements

The Espressif IDF

| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- |

* [ESP-IDF Getting Started Guide on ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
* [ESP-IDF Getting Started Guide on ESP32-S2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html)
* [ESP-IDF Getting Started Guide on ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html)

### Configure the project

Open the project configuration menu (`idf.py menuconfig`).

In the `Fiat Lux Configuration` menu:

* Set the Wi-Fi configuration.
    * Set `WiFi SSID`.
    * Set `WiFi Password`.

Optional: If you need, change the other options according to your requirements.

### Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
