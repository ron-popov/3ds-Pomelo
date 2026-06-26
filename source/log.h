#pragma once

#include <3ds.h>


void log_debug(const char* format, ...);

void print_error_code_verbose(char* desc, Result res);

void log_debug_hardware(const char* text);
