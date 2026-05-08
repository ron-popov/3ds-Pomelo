# Custom Homemenu
Custom Homemenu made with libctru and citro2d.

## Features
- [x] List all installed titles
- [x] Launch selected title
- [ ] Handle APT events
- [ ] Pretty UI
- [ ] OS Stats - Battery, wifi and etc...
- [ ] Show title 3d icon
- [ ] Cutomizable UI

## Build

This project works with the default 3ds devkitpro installation.   
At this state, this is still fullfilled with bugs and disgraceful tricks.   
I also plan to migrate to C++ to take advantage of the class system.   

To test the app, you can do the following on Linux:
```sh
# Pull submodules - custom libctru
git submodule update --init

# This doesn't require "3dsx_to_cxi", will output only a 3dsx file
make 3dsx

# build 3dsx and cxi file
# this requires install / build from mikage source the tool "3dsx_to_cxi" and add to path
make all

```


## Deploy to mikage homemenu
The title built by this project is supposed to run as a home menu
To install this title as a mikage homemenu do the following (assuming you already have mikage home menu running)
```bash
# Backup original mikage homemenu
cp -v ~/.local/share/mikage/data/00040030/00009802/content/00000027.cxi ~/.local/share/mikage/data/00040030/00009802/content/00000027.cxi.bak
cp -v ~/.local/share/mikage/data/00040030/00009802/content/00000000.cxi ~/.local/share/mikage/data/00040030/00009802/content/00000000.cxi.bak

# This will delete original mikage homemenu cxi files
# And then copy the cxi file we built to mikage homemenu cxi file directory
make install_mikage
```


## Known Issues
### Unable to launch all games
For some reason, not entirely sure yet why, some games won't boot and the kernel will panic when they load
I am currently aware of 1 game that does this, which is "Super Mario 3D Land"
Some games load but crash after a couple of seconds, currently i am using mostly [3ds-examples](https://github.com/devkitpro/3ds-examples) for testing things out

### Not handling APT events
We don't do anything with the apt events except for printin them
Becuase of this things like returning to the homemenu after a game exists don't work.
Another thing that doens't work is suspending the game by pressing the home button.