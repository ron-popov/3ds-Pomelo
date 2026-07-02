#include "apt_callbacks.h"
#include "log.h"
#include "utils.h"

// This handles apt callbacks to our process
void aptCallback(APT_HookType hook, void* param) {
    log_debug("Got APT callback");

    switch(hook) {
        case APTHOOK_ONSUSPEND:
            log_debug("Got apt hook APTHOOK_ONSUSPEND");
            break;
        case APTHOOK_ONRESTORE:
            log_debug("Got apt hook APTHOOK_ONRESTORE");
            break;
        case APTHOOK_ONSLEEP:
            log_debug("Got apt hook APTHOOK_ONSLEEP");
            break;
        case APTHOOK_ONWAKEUP:
            log_debug("Got apt hook APTHOOK_ONWAKEUP");
            break;
        case APTHOOK_ONEXIT:
            log_debug("Got apt hook APTHOOK_ONEXIT");
            break;
        case APTHOOK_COUNT:
            log_debug("Got apt hook APTHOOK_COUNT");
            break;
        default:
            log_debug("Got apt hook UNKNOWN");
            break;
    }
}

// This handles messages from other applets
void aptMessageCallback(void* user, NS_APPID sender, void* msg, size_t msgsize) {
    log_debug("Got message from other system applet");
}

void aptSignalCallback(APT_Signal signal) {
    switch (signal) {
        case APTSIGNAL_POWERBUTTON:
        case APTSIGNAL_POWERBUTTON2: // Shutdown the system
            log_debug("Initiating shutdown due to power button press");
			printf("Shutting down...\n");
			hardwareTimerSleep(2); // Give the console a moment to flush the log
            ptmSysmInit();
            PTMSYSM_ShutdownAsync(0);
            break;
        case APTSIGNAL_HOMEBUTTON2:
        case APTSIGNAL_HOMEBUTTON:
            log_debug("Got APT Home button press signal");
            break;
        default:
            log_debug("Got APT signal 0x%x", signal);
            break;
        }
}

/// libctru weak function overriding, the functions here are defined as weak in libctru
/// The fact they are defined here, overrides the libctru implementation, each function is overriden for different purposes

// This function is used to debug apt messages, we are overriding it to use our logger
void _aptDebug(int a, int b) {
    log_debug("APT Debug 0x%x 0x%x", a, b);
}
