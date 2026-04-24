# Custom Homemenu
Custom Homemenu made with libctru and citro2d.

## Features
- [x] List all installed titles
- [ ] Launch selected title
- [ ] OS Stats - Battery, wifi and etc...
- [ ] Pretty UI
- [ ] Cutomizable UI

## Build

This project works with the default 3ds devkitpro installation.   
At this state, this is still fullfilled with bugs and disgraceful tricks.   
I also plan to migrate to C++ to take advantage of the class system.   

To test the app, you can do the following on Linux:
```sh
# Patch 3dsx_crt0.o to change our appid to 0x101 instead of the default 0x300
sudo bash -c "xxd /opt/devkitpro/devkitARM/arm-none-eabi/lib/armv6k/fpu/3dsx_crt0.o | sed 's/00000040: 0003 0000 0000 8001/00000040: 0101 0000 0000 8001/' | xxd -r > /opt/devkitpro/devkitARM/arm-none-eabi/lib/armv6k/fpu/3dsx_crt0.o.temp && mv -v -f /opt/devkitpro/devkitARM/arm-none-eabi/lib/armv6k/fpu/3dsx_crt0.o.temp /opt/devkitpro/devkitARM/arm-none-eabi/lib/armv6k/fpu/3dsx_crt0.o"

# build
make

# Build cxi file (which is a ncch), specifically for europe region
# The cxi file will be in custom_homemenu.3dsx.ncch
~/3ds/mikage-dev/build/tools/3dsx_to_cia/3dsx_to_cia --title-id 0x4003000009802 --gen-ncch --input custom_homemenu.3dsx

```


## Deploy to mikage homemenu
The title built by this project is supposed to run as a home menu
To install this title as a mikage homemenu do the following (assuming you already have mikage home menu running)
```bash
# Backup original mikage homemenu
cp -v ~/.local/share/mikage/data/00040030/00009802/content/00000027.cxi ~/.local/share/mikage/data/00040030/00009802/content/00000027.cxi.bak
cp -v ~/.local/share/mikage/data/00040030/00009802/content/00000000.cxi ~/.local/share/mikage/data/00040030/00009802/content/00000000.cxi.bak

# Delete original mikage homemenu cxi files
rm -v ~/.local/share/mikage/data/00040030/00009802/content/00000027.cxi
rm -v ~/.local/share/mikage/data/00040030/00009802/content/00000000.cxi

# Copy cxi file we built to mikage homemenu cxi file directory (overriding the original one)
cp -v -f custom_homemenu.3dsx.ncch ~/.local/share/mikage/data/00040030/00009802/content/00000000.cxi
```
