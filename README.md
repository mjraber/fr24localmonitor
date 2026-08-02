# FR24localmonitor

## Hardware

### Components
* ESP32-WROOM-32 
* Display Velleman WPSH412
* Some cables
* Enclosure components (3D printed)

### Cabling
* EPS32 PowerSupply through USB

### Display to ESP32 connections
* 5V &harr; 5V 
* GND &harr; GND
* RD &harr; inactive
* WR &harr; 26
* RS &harr; 25
* CS &harr; 33
* RST &harr; 32
* D0 &harr; 16
* D1 &harr; 17
* D2 &harr; 18
* D3 &harr; 19
* D4 &harr; 21
* D5 &harr; 22
* D6 &harr; 23
* D7 &harr; 15

### 3D-Printing

In my project, I used a Anycubic i3 Mega S, but you can use any other 3D printer, maybe with some adaption of parameters.

In the files-folder:
* f3d-files
* stl-files
* Brother-Label for Pinout

## Software

There is only one single program for the whole thing and one file for the credentials

* _main.cpp_ for the main program
* _secrets_template.h_ provided as a template for _secrets.h_, which is not provided here
* _secrets.h_ to be copied from _secrets_template.h_ and replace all data with our credentials and local settings
