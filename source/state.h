#pragma once

#include "log.h"

// When adding a new state, note to also add it to GetStateName - that how the logger gets the name of the state
enum HomemenuState {
	STATE_NONE = 0,                   // State hasn't been initialized
    STATE_FOREGROUND = 1,             // Currently in the foreground, user can make actions
    STATE_WAIT_TO_REGISTER = 2,       // In transition to new app, user can still do actions
    STATE_BACKGROUND = 3              // In background, must still handle shutdowns
};

static enum HomemenuState state = STATE_NONE;

void GetStateName(enum HomemenuState state, char* nameOut) {
    switch (state)
    {
        case STATE_NONE:
            strcpy(nameOut, "None");
            return;
        case STATE_FOREGROUND:
            strcpy(nameOut, "Foreground");
            return;
        case STATE_WAIT_TO_REGISTER:
            strcpy(nameOut, "Waiting App To Register");
            return;
        case STATE_BACKGROUND:
            strcpy(nameOut, "Background");
            return;
        default:
            strcpy(nameOut, "ERROR - NO NAME");
            return;
    }
}

enum HomemenuState GetState() {
    return state;
}

void SetState(enum HomemenuState newState) {
    char oldStateName[64];
    char newStateName[64];
    GetStateName(state, (char*)&oldStateName);
    GetStateName(newState, (char*)&newStateName);

    log_debug(
        "Changing state from \"%s\" (0x%x) to \"%s\" (0x%x)", 
        oldStateName, state, 
        newStateName, newState
    );

    state = newState;
}