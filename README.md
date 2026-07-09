# Pomelo 3DS
Pomelo 3ds is an alternative homemenu to the stock homemenu! Currently the project is in early stages of development and this repository only serves as a super basic POC.
This project currently only supports US systems, it is able to run on mikage and on real hardware running US firmware.

## Features
- List all installed titles
- Launch selected title

## Build
This project uses my own fork of `libctru`. libctru is intented for building a 3ds application / game, however
the homemenu has different requirements, so i created my own fork - [link](https://github.com/ron-popov/libctru-for-homemenu)

It also requires to have `makerom` and `ctrtool` installed and in your path to work. `makerom` is used to cxi files, that can be deployed to mikage.
`ctrtool` is used to extract the code.bin + exheader.bin files from the CXI file, those files can then be deployed to real hardware.

To test the app, you can do the following on Linux:
```sh
# Pull submodules - custom libctru for homemenu
git submodule update --init

# Will output only a 3dsx file
# This doesn't require "makerom"
make 3dsx

# Build 3dsx, cxi, and code.bin + exheader.bin
# Make sure you have "makerom" and "ctrtool" in your path
make all
```

## Deploy to real hardware
This requires having a sdcard and luma3ds installed on your system. Currently it only supports US systems.
```bash
# Build a "pomelo.code.bin" and "pomelo.exheader.bin"
make code.bin

# Then renale "pomelo.code.bin" and "pomelo.exheader.bin"
mv pomelo.code.bin code.bin
mv pomelo.exheader.bin exheader.bin

# Then copy them to a SDCARD to
# If the "titles" or "0004003000008F02" directories don't exist on your SDCARD, create them
/luma/titles/0004003000008F02/code.bin
/luma/titles/0004003000008F02/exheader.bin

# Insert the SDCARD into your system and enjoy :)
```