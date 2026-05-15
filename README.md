# Pomelo 3DS
Pomelo 3ds is an alternative homemenu to the stock homemenu! Currently the project is in early stages of development and this repository only serves as a super basic POC.
This project currently only supports US systems, it is able to run on mikage and on real hardware running US firmware.

## Features
- [x] List all installed titles
- [x] Launch selected title
- [ ] Handle APT events
- [ ] Pretty UI
- [ ] OS Stats - Battery, wifi and etc...
- [ ] Show title 3d icon
- [ ] Cutomizable UI

## Build
This project uses my own fork of `libctru`. libctru is intented for building a 3ds application / game, however
the homemenu has different requirements, so i created my own fork - [link](https://github.com/ron-popov/libctru-for-homemenu)

It also requires to have `makerom` installed and in your path to work. `makerom` is used to cxi files, that can be deployed to mikage.
And code.bin + exheader.bin files that can be deployed to real hardware.

To test the app, you can do the following on Linux:
```sh
# Pull submodules - custom libctru for homemenu
git submodule update --init

# Will output only a 3dsx file
# This doesn't require "makerom"
make 3dsx

# Build 3dsx, cxi, and code.bin + exheader.bin
# Make sure you have "makerom" in your path
make all
```


## Deploy to mikage homemenu
The title built by this project is supposed to run as a home menu. Mikage must be bootstrapped as a US system.
To install this title as a mikage homemenu do the following (assuming you already have mikage home menu running)
```bash
# Backup original mikage homemenu
cp -v ~/.local/share/mikage/data/00040030/00009802/content/00000027.cxi ~/.local/share/mikage/data/00040030/00009802/content/00000027.cxi.bak
cp -v ~/.local/share/mikage/data/00040030/00009802/content/00000000.cxi ~/.local/share/mikage/data/00040030/00009802/content/00000000.cxi.bak

# This will override the mikage homemenu with pomelo
make install_mikage
```

## Deploy to real hardware
This requires having a sdcard and luma3ds installed on your system
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