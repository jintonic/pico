Firmware for Radiation Detection with Raspberry Pi Pico

## Getting Started
For most users, simply download firmware.uf2 from <https://github.com/jintonic/pico/releases>, flash it to Pi Pico, and that's it.

For those who want to modify the source code, please

1. install Pi Pico C/C++ SDK through Visual Studio Code and its Raspberry Pi Pico extension.
2. download the source code from <https://github.com/jintonic/pico>.
3. go to "Raspberry Pi Pico Project" tab in the left sidebar in VS Code, select "Import Project", and import the downloaded project. A folder called `.vscode` will be created in the source code folder.
4. click "Compile Project" in the sidebar. If the project compiles, one can find `firmware.uf2` in the `build` sub-folder.
5. click "Run Project" in the sidebar to flash the uf2 file to Pi Pico, or do it manually.
