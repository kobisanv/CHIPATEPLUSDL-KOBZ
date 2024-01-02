# CHIP-8 Emulator

## Introduction

This project is a simple CHIP-8 emulator written in C++. The emulator is designed to run CHIP-8 programs, which are interpreted programming language originally used on the COSMAC VIP and Telmac 1800 8-bit microcomputers.

## Features

- Emulates the CHIP-8 instruction set.
- Supports a variety of opcodes and instructions.
- Basic input handling for keypad.
- Display rendering for CHIP-8 graphics.
- Debugging information output to the console.

## How to Get Started
- ROM Files are found in CMake Build Debug | Root Folder contains TXT files with instructions to each game respectively
- Start off by cloning the repo in your terminal of choice `git clone https://github.com/kobisanv/CHIPATEPLUSDL-KOBZ.git`
- From the directory enter `CHIP_8__.exe "rom_file.ch8"` for Windows, and `./CHIP_8__.app rom_file.ch8` for Mac/Linux after building the project in your preferred IDE as EXE files are not compatible with macOS or Linux
- The last step is to enjoy the game! For controls, please refer below!

## Controls
- The TXT file may say some keys to press, however when trying it out for yourself, you may realize that the QWERTY keys do not correspond to the CHIP-8 keypad
- This is a small guide on mapping from CHIP-8 keypad values found in the TXT file
- `1 (CHIP-8) -> 1 (QWERTY)`
- `2 (CHIP-8) -> 2 (QWERTY)`
- `3 (CHIP-8) -> 3 (QWERTY)`
- `C (CHIP-8) -> 4 (QWERTY)`

- `4 (CHIP-8) -> q (QWERTY)`
- `5 (CHIP-8) -> w (QWERTY)`
- `6 (CHIP-8) -> e (QWERTY)`
- `D (CHIP-8) -> r (QWERTY)`

- `7 (CHIP-8) -> a (QWERTY)`
- `8 (CHIP-8) -> s (QWERTY)`
- `9 (CHIP-8) -> d (QWERTY)`
- `E (CHIP-8) -> f (QWERTY)`

- `A (CHIP-8) -> z (QWERTY)`
- `0 (CHIP-8) -> x (QWERTY)`
- `B (CHIP-8) -> c (QWERTY)`
- `F (CHIP-8) -> v (QWERTY)`

### Resources

- [SDL library](https://www.libsdl.org/)
- [ROMS](https://github.com/kripod/chip8-roms/tree/master/games)
- [WIKI](https://www.wikiwand.com/en/CHIP-8)
- [Other Resources](https://github.com/f0lg0/CHIP-8)
