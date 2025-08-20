Firmware for Radiation Detection with Raspberry Pi Pico, which can

* digitize waveform data from a radiation detector
* blink the LED on Pi Pico board when radiation is detected
* sound an external buzzer when radiation is detected
* display trigger information on an OLED
* store data in a Micro SD card

## Getting Started
For most users, simply download `firmware.uf2` from <https://github.com/jintonic/pico/releases>, flash it to Pi Pico, and that's it.

For those who want to modify the source code, please

1. [install] "Pi Pico C/C++ SDK" through Visual Studio Code and its "Raspberry Pi Pico" extension.
2. download the source code from <https://github.com/jintonic/pico>.
3. go to "Raspberry Pi Pico Project" tab in the left sidebar in VS Code, select "Import Project", and import the downloaded project. A folder called `.vscode` will be created in the source code folder.
4. click "Compile Project" in the sidebar. If the project compiles, one can find `firmware.uf2` in the `build` sub-folder.
5. click "Run Project" in the sidebar to flash the uf2 file to Pi Pico, or do it manually.

[install]: https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf

## Wiring

## Firmware Functionality

### Blinking LED on Pi Pico

### Driving an External Buzzer

### Saving Data in a Micro SD Card

<https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico>

### Displaying Data on an OLED

### Digitizing Waveform from Detector