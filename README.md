Raspberry Pi Pico firmware for radiation detection. It provides the following functionalities.

* digitize waveform data from a radiation detector
* blink the LED on Pi Pico board when radiation is detected
* sound an external buzzer when radiation is detected
* display trigger information on an OLED
* store data in a Micro SD card

## Getting Started
For most users, simply download `firmware.uf2` from <https://github.com/jintonic/pico/releases>, flash it to Pi Pico, and that's it.

For those who want to modify the source code, please

1. [install] "Pi Pico C/C++ SDK" through Visual Studio Code and its "Raspberry Pi Pico" extension.
2. download this repository from <https://github.com/jintonic/pico>
```sh
git clone --recurse-submodules --depth 1 https://github.com/jintonic/pico
```
3. go to "Raspberry Pi Pico Project" tab in the left sidebar in VS Code, select "Import Project", and import the downloaded project. Select `Pico` as a Kit if asked. A folder called `.vscode` and a file `pico_sdk_import.cmake` will be created in the source code folder.
4. modify code as you like.
5. click "Compile Project" in the sidebar. Select `firmware` if you are asked to `Select a launch target for pico`. If the project compiles, one can find `firmware.uf2` in the `build` sub-folder.
6. click "Run Project" in the sidebar to flash the uf2 file to Pi Pico, or do it manually.

[install]: https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf

## Wiring
Wiring of buzzer, reset button, SD card reader ([SPI](https://electronics.stackexchange.com/a/594276/316245)) and OLED (I2C) around a Pi Pico on a bread board:

<img height="500" alt="wiring of buzzer, reset button, SD card reader and OLED" src="https://github.com/user-attachments/assets/aa8cd64e-2b6f-4b83-a76f-b1f3f276e2c8" />
<img height="500" alt="Pico with buzzer, reset button, SD card reader and OLED" src="https://github.com/user-attachments/assets/75766d4e-b454-47be-b969-617ea47397db" />

## Firmware Functionality

### Blinking LED on Pi Pico

### Driving an External Buzzer

### Saving Data in a Micro SD Card
A slightly older [library](https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico) is used instead of a [newer one](https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico) to communicate with an SD card, because the latter offers way too much for this simple project. The library is added as a submodule in the folder `SDCard`.

```sh
git submodule add --depth 1 https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico SDCard
git config -f .gitmodules submodule.SDCard.shallow true
git commit -am 'added SD card lib'
```

### Displaying Data on an OLED
<https://github.com/daschr/pico-ssd1306> is used to communicate with an OLED. The code is added as a submodule in the folder `OLED`.

```sh
git submodule add --depth 1 https://github.com/daschr/pico-ssd1306 OLED
git config -f .gitmodules submodule.OLED.shallow true
git commit -am 'added OLED lib'
```

The following lines of code is added to [CMakeLists.txt](CMakeLists.txt) to use pico-ssd1306 as a library:

```cmake
# Add ssd1306 library
add_library(ssd1306_i2c INTERFACE)
target_sources(ssd1306_i2c INTERFACE OLED/ssd1306.c)
target_include_directories(ssd1306_i2c INTERFACE OLED)
target_link_libraries(ssd1306_i2c INTERFACE pico_stdlib hardware_i2c)
```

The idea comes from <https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico/blob/master/FatFs_SPI/CMakeLists.txt>.

The OLED is from <https://goldenmorninglcd.com/oled-display-module/0.96-inch-128x64-ssd1306-gme12864-11>

### Digitizing Waveform from Detector
