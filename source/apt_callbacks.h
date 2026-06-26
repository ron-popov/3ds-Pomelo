#pragma once

#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <stddef.h>


void aptCallback(APT_HookType hook, void* param);
void aptMessageCallback(void* user, NS_APPID sender, void* msg, size_t msgsize);
void aptSignalCallback(APT_Signal signal);
void _aptDebug(int a, int b);
void __appInit(void);