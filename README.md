Raspberry Pi Pico firmware for radiation detection. It provides the following functionalities.

* digitize waveforms from a radiation detector
* blink the LED on Pi Pico board when a waveform is recorded
* sound an external buzzer when a waveform is recorded
* display information on an OLED
* store data in a micro SD card

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

<img height="350" alt="wiring of buzzer, reset button, SD card reader and OLED" src="https://github.com/user-attachments/assets/aa8cd64e-2b6f-4b83-a76f-b1f3f276e2c8" />
<img height="350" alt="Pico with buzzer, reset button, SD card reader and OLED" src="https://github.com/user-attachments/assets/75766d4e-b454-47be-b969-617ea47397db" />

Wiring of power supply, detector, pre-amp and comparator:

<img height="350" alt="wiring of HV, detector, pre-amp and comparator" src="https://github.com/user-attachments/assets/649352ee-714e-409d-bcda-12c593d7c90d" />

## Firmware Functionality

### Reset Pi Pico

A tact [button] switch connecting pin 30 (RUN) and 28 (Ground) of Pi Pico can be used to reset the board. It can also be used [together] with the [BOOTSEL] button on board to turn the board into a USD drive so that a firmware file (*.uf2) can be dragged and dropped into the drive.

[button]: https://www.amazon.com/dp/B07X8T9D2Q
[together]: https://www.raspberrypi.com/news/how-to-add-a-reset-button-to-your-raspberry-pi-pico
[BOOTSEL]: https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html#resetting-flash-memory

### Blink LED on Pi Pico

```c
gpio_init(PICO_DEFAULT_LED_PIN);              // initialize LED on Pico board
gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT); // set direction of GPIO signal
gpio_put(PICO_DEFAULT_LED_PIN, true);         // turn on LED
gpio_put(PICO_DEFAULT_LED_PIN, false);        // turn off LED
```

### Drive an External Buzzer

```c
gpio_init(0);              // positive leg of buzzer is connected to GPIO 0
gpio_set_dir(0, GPIO_OUT); // set direction of GPIO signal
gpio_put(0, true);         // turn on buzzer
gpio_put(0, false);        // turn off buzzer
```

Since Pi Pico can only output 3.3 V from a GPIO pin, a [buzzer] that can run at 3 V is needed.

[buzzer]: https://www.amazon.com/dp/B07VRK7ZPF

### Save Data in a Micro SD Card

A slightly older [library](https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico) is used instead of a [newer one](https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico) to communicate with an SD card, because the latter offers way too much for this simple project. The library is added as a submodule in the folder `SDCard`.

```sh
git submodule add --depth 1 https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico SDCard
git config -f .gitmodules submodule.SDCard.shallow true
git commit -am 'added SD card lib'
```

A search of files named `run{nnn}.txt` in the SD card is made when the Pi Pico is powered/reset. A new file `run{nnn+1}.txt` is created to avoid overwriting old data. The program stops when 5,000 events are recorded in the file. Push the reset button to start a new run.

The SD card [adapter] utilizes [SPI] to communicate with Pi Pico.

[adapter]: https://www.amazon.com/dp/B0989SM146
[SPI]: https://en.wikipedia.org/wiki/Serial_Peripheral_Interface

### Display Data on an OLED
[pico-ssd1306] is used to communicate with an [OLED]. The code is added as a submodule in the folder `OLED`.

```sh
git submodule add --depth 1 https://github.com/daschr/pico-ssd1306 OLED
git config -f .gitmodules submodule.OLED.shallow true
git commit -am 'added OLED lib'
```

The following lines of code is added to [CMakeLists.txt](CMakeLists.txt) to use [pico-ssd1306] as a library:

```cmake
add_library(ssd1306_i2c INTERFACE)
target_sources(ssd1306_i2c INTERFACE OLED/ssd1306.c)
target_include_directories(ssd1306_i2c INTERFACE OLED)
target_link_libraries(ssd1306_i2c INTERFACE pico_stdlib hardware_i2c)
```

The idea comes from <https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico/blob/master/FatFs_SPI/CMakeLists.txt>.

[OLED]: https://goldenmorninglcd.com/oled-display-module/0.96-inch-128x64-ssd1306-gme12864-11

[pico-ssd1306]: https://github.com/daschr/pico-ssd1306

### Digitize Waveforms from Detector

Two out of 12 [DMA] channels in Pi Pico are used to save waveforms digitized in ADC channel 0 into the on-board flash memory. One is used as the sampling channel (`smpl_ch`) connecting ADC FIFO to the declared memory location; the other is used as the controlling channel (`ctrl_ch`) connecting the memory location to the DMA's write trigger address, which retriggers `smpl_ch` to restart. In order to avoid conflict with the DMA usage of the SD card library, both channels are obtained in the following way:

```c
smpl_ch = dma_claim_unused_channel(true);
ctrl_ch = dma_claim_unused_channel(true);
```
[DMA]: https://github.com/fandahao17/Raspberry-Pi-DMA-Tutorial

ADC0 (GPIO26) is used to digitize the output of pre-amp.

### Receive Trigger from Comparator
GPIO28 is used to receive trigger signals from the comparator. If the op-amp, which the comparator is based on, is powered by 3.3 V, the trigger signal is a square pulse of 3.3 V. The width of the pulse is proportional to the width of the signal from the pre-amp. When the voltage recieved by PGIO28 goes up, the Pi Pico will sound the buzzer, light the LED, find the highest sample in the digitized waveform, print the result to the serial port associated with the USB connection, and save the result to the SD card.
