# FR24localmonitor

## Hardware

### Components
* ESP32-WROOM-32 
* Display Velleman WPSH412
* Some cables
* Enclosure components (3D printed)

### Connection ESP32-Display (Cabling)


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
