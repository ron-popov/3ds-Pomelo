/**
 * @file citro2d.h
 * @brief Central citro2d header. Includes all others.
 */
#pragma once

#ifdef CITRO2D_BUILD
#error "This header file is only for external users of citro2d."
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <citro3d.h>
#include <tex3ds.h>

#include "include/base.h"
#include "include/spritesheet.h"
#include "include/sprite.h"
#include "include/text.h"
#include "include/font.h"

#ifdef __cplusplus
}
#endif
