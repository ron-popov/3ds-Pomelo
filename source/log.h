#pragma once

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>


void log_debug(const char* format, ...);

void print_error_code_verbose(char* desc, Result res);

void log_debug_hardware(const char* text);
