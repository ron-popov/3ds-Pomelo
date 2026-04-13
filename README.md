# Custom Homemenu
Custom Homemenu made with libctru and citro2d.

## Build

This project works with the default 3ds devkitpro installation.   
At this state, this is still fullfilled with bugs and disgraceful tricks.   
I also plan to migrate to C++ to take advantage of the class system.   

To test the app, you can do the following on Linux:
```sh
# build
make
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

# Build cxi file (which is a ncch), specifically for europe region
# The cxi file will be in custom_homemenu.3dsx.ncch
~/3ds/mikage-dev/build/tools/3dsx_to_cia/3dsx_to_cia --title-id 0x4003000009802 --gen-ncch --input custom_homemenu.3dsx

# Copy new cxi file to mikage homemenu cxi file directory
cp -v -f custom_homemenu.3dsx.ncch ~/.local/share/mikage/data/00040030/00009802/content/00000000.cxi
```
