//
//  CLKSCrashMonitor_AppState.c
//
//  Created by Karl Stenerud on 2012-02-05.
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//


#include "CLKSCrashMonitor_AppState.h"

#include "CLKSFileUtils.h"
#include "CLKSJSONCodec.h"
#include "CLKSCrashMonitorContext.h"

//#define CLKSLogger_LocalLevel TRACE
#include "CLKSLogger.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>


// ============================================================================
#pragma mark - Constants -
// ============================================================================

#define kFormatVersion 1

#define kKeyFormatVersion "version"
#define kKeyCrashedLastLaunch "crashedLastLaunch"
#define kKeyActiveDurationSinceLastCrash "activeDurationSinceLastCrash"
#define kKeyBackgroundDurationSinceLastCrash "backgroundDurationSinceLastCrash"
#define kKeyLaunchesSinceLastCrash "launchesSinceLastCrash"
#define kKeySessionsSinceLastCrash "sessionsSinceLastCrash"
#define kKeySessionsSinceLaunch "sessionsSinceLaunch"



// ============================================================================
#pragma mark - Globals -
// ============================================================================

/** Location where stat file is stored. */
static const char* g_stateFilePath;

/** Current state. */
static CLKSCrash_AppState g_state;

static volatile bool g_isEnabled = false;

// ============================================================================
#pragma mark - JSON Encoding -
// ============================================================================

static int onBooleanElement(const char* const name, const bool value, void* const userData)
{
    CLKSCrash_AppState* state = userData;

    if(strcmp(name, kKeyCrashedLastLaunch) == 0)
    {
        state->crashedLastLaunch = value;
    }

    return CLKSJSON_OK;
}

static int onFloatingPointElement(const char* const name, const double value, void* const userData)
{
    CLKSCrash_AppState* state = userData;

    if(strcmp(name, kKeyActiveDurationSinceLastCrash) == 0)
    {
        state->activeDurationSinceLastCrash = value;
    }
    if(strcmp(name, kKeyBackgroundDurationSinceLastCrash) == 0)
    {
        state->backgroundDurationSinceLastCrash = value;
    }

    return CLKSJSON_OK;
}

static int onIntegerElement(const char* const name, const int64_t value, void* const userData)
{
    CLKSCrash_AppState* state = userData;

    if(strcmp(name, kKeyFormatVersion) == 0)
    {
        if(value != kFormatVersion)
        {
            CLKSLOG_ERROR("Expected version 1 but got %" PRId64, value);
            return CLKSJSON_ERROR_INVALID_DATA;
        }
    }
    else if(strcmp(name, kKeyLaunchesSinceLastCrash) == 0)
    {
        state->launchesSinceLastCrash = (int)value;
    }
    else if(strcmp(name, kKeySessionsSinceLastCrash) == 0)
    {
        state->sessionsSinceLastCrash = (int)value;
    }

    // FP value might have been written as a whole number.
    return onFloatingPointElement(name, value, userData);
}

static int onNullElement(__unused const char* const name, __unused void* const userData)
{
    return CLKSJSON_OK;
}

static int onStringElement(__unused const char* const name,
                           __unused const char* const value,
                           __unused void* const userData)
{
    return CLKSJSON_OK;
}

static int onBeginObject(__unused const char* const name, __unused void* const userData)
{
    return CLKSJSON_OK;
}

static int onBeginArray(__unused const char* const name, __unused void* const userData)
{
    return CLKSJSON_OK;
}

static int onEndContainer(__unused void* const userData)
{
    return CLKSJSON_OK;
}

static int onEndData(__unused void* const userData)
{
    return CLKSJSON_OK;
}


/** Callback for adding JSON data.
 */
static int addJSONData(const char* const data, const int length, void* const userData)
{
    const int fd = *((int*)userData);
    const bool success = clksfu_writeBytesToFD(fd, data, length);
    return success ? CLKSJSON_OK : CLKSJSON_ERROR_CANNOT_ADD_DATA;
}


// ============================================================================
#pragma mark - Utility -
// ============================================================================

static double getCurentTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static double timeSince(double timeInSeconds)
{
    return getCurentTime() - timeInSeconds;
}

/** Load the persistent state portion of a crash context.
 *
 * @param path The path to the file to read.
 *
 * @return true if the operation was successful.
 */
bool loadState(const char* const path)
{
    // Stop if the file doesn't exist.
    // This is expected on the first run of the app.
    const int fd = open(path, O_RDONLY);
    if(fd < 0)
    {
        return false;
    }
    close(fd);

    char* data;
    int length;
    if(!clksfu_readEntireFile(path, &data, &length, 50000))
    {
        CLKSLOG_ERROR("%s: Could not load file", path);
        return false;
    }

    CLKSJSONDecodeCallbacks callbacks;
    callbacks.onBeginArray = onBeginArray;
    callbacks.onBeginObject = onBeginObject;
    callbacks.onBooleanElement = onBooleanElement;
    callbacks.onEndContainer = onEndContainer;
    callbacks.onEndData = onEndData;
    callbacks.onFloatingPointElement = onFloatingPointElement;
    callbacks.onIntegerElement = onIntegerElement;
    callbacks.onNullElement = onNullElement;
    callbacks.onStringElement = onStringElement;

    int errorOffset = 0;

    char stringBuffer[1000];
    const int result = clksjson_decode(data,
                                     (int)length,
                                     stringBuffer,
                                     sizeof(stringBuffer),
                                     &callbacks,
                                     &g_state,
                                     &errorOffset);
    free(data);
    if(result != CLKSJSON_OK)
    {
        CLKSLOG_ERROR("%s, offset %d: %s",
                    path, errorOffset, clksjson_stringForError(result));
        return false;
    }
    return true;
}

/** Save the persistent state portion of a crash context.
 *
 * @param path The path to the file to create.
 *
 * @return true if the operation was successful.
 */
bool saveState(const char* const path)
{
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if(fd < 0)
    {
        CLKSLOG_ERROR("Could not open file %s for writing: %s",
                    path,
                    strerror(errno));
        return false;
    }

    CLKSJSONEncodeContext JSONContext;
    clksjson_beginEncode(&JSONContext,
                       true,
                       addJSONData,
                       &fd);

    int result;
    if((result = clksjson_beginObject(&JSONContext, NULL)) != CLKSJSON_OK)
    {
        goto done;
    }
    if((result = clksjson_addIntegerElement(&JSONContext,
                                          kKeyFormatVersion,
                                          kFormatVersion)) != CLKSJSON_OK)
    {
        goto done;
    }
    // Record this launch crashed state into "crashed last launch" field.
    if((result = clksjson_addBooleanElement(&JSONContext,
                                          kKeyCrashedLastLaunch,
                                          g_state.crashedThisLaunch)) != CLKSJSON_OK)
    {
        goto done;
    }
    if((result = clksjson_addFloatingPointElement(&JSONContext,
                                                kKeyActiveDurationSinceLastCrash,
                                                g_state.activeDurationSinceLastCrash)) != CLKSJSON_OK)
    {
        goto done;
    }
    if((result = clksjson_addFloatingPointElement(&JSONContext,
                                                kKeyBackgroundDurationSinceLastCrash,
                                                g_state.backgroundDurationSinceLastCrash)) != CLKSJSON_OK)
    {
        goto done;
    }
    if((result = clksjson_addIntegerElement(&JSONContext,
                                          kKeyLaunchesSinceLastCrash,
                                          g_state.launchesSinceLastCrash)) != CLKSJSON_OK)
    {
        goto done;
    }
    if((result = clksjson_addIntegerElement(&JSONContext,
                                          kKeySessionsSinceLastCrash,
                                          g_state.sessionsSinceLastCrash)) != CLKSJSON_OK)
    {
        goto done;
    }
    result = clksjson_endEncode(&JSONContext);

done:
    close(fd);
    if(result != CLKSJSON_OK)
    {
        CLKSLOG_ERROR("%s: %s",
                    path, clksjson_stringForError(result));
        return false;
    }
    return true;
}


// ============================================================================
#pragma mark - API -
// ============================================================================

void clkscrashstate_initialize(const char *const stateFilePath)
{
    g_stateFilePath = strdup(stateFilePath);
    memset(&g_state, 0, sizeof(g_state));
    loadState(g_stateFilePath);
}

bool clkscrashstate_reset()
{
    if(g_isEnabled)
    {
        g_state.sessionsSinceLaunch = 1;
        g_state.activeDurationSinceLaunch = 0;
        g_state.backgroundDurationSinceLaunch = 0;
        if(g_state.crashedLastLaunch)
        {
            g_state.activeDurationSinceLastCrash = 0;
            g_state.backgroundDurationSinceLastCrash = 0;
            g_state.launchesSinceLastCrash = 0;
            g_state.sessionsSinceLastCrash = 0;
        }
        g_state.crashedThisLaunch = false;
        
        // Simulate first transition to foreground
        g_state.launchesSinceLastCrash++;
        g_state.sessionsSinceLastCrash++;
        g_state.applicationIsInForeground = true;
        
        return saveState(g_stateFilePath);
    }
    return false;
}

void clkscrashstate_notifyAppActive(const bool isActive)
{
    if(g_isEnabled)
    {
        g_state.applicationIsActive = isActive;
        if(isActive)
        {
            g_state.appStateTransitionTime = getCurentTime();
        }
        else
        {
            double duration = timeSince(g_state.appStateTransitionTime);
            g_state.activeDurationSinceLaunch += duration;
            g_state.activeDurationSinceLastCrash += duration;
        }
    }
}

void clkscrashstate_notifyAppInForeground(const bool isInForeground)
{
    if(g_isEnabled)
    {
        const char* const stateFilePath = g_stateFilePath;

        g_state.applicationIsInForeground = isInForeground;
        if(isInForeground)
        {
            double duration = getCurentTime() - g_state.appStateTransitionTime;
            g_state.backgroundDurationSinceLaunch += duration;
            g_state.backgroundDurationSinceLastCrash += duration;
            g_state.sessionsSinceLastCrash++;
            g_state.sessionsSinceLaunch++;
        }
        else
        {
            g_state.appStateTransitionTime = getCurentTime();
            saveState(stateFilePath);
        }
    }
}

void clkscrashstate_notifyAppTerminate(void)
{
    if(g_isEnabled)
    {
        const char* const stateFilePath = g_stateFilePath;

        const double duration = timeSince(g_state.appStateTransitionTime);
        g_state.backgroundDurationSinceLastCrash += duration;
        saveState(stateFilePath);
    }
}

void clkscrashstate_notifyAppCrash(void)
{
    if(g_isEnabled)
    {
        const char* const stateFilePath = g_stateFilePath;

        const double duration = timeSince(g_state.appStateTransitionTime);
        if(g_state.applicationIsActive)
        {
            g_state.activeDurationSinceLaunch += duration;
            g_state.activeDurationSinceLastCrash += duration;
        }
        else if(!g_state.applicationIsInForeground)
        {
            g_state.backgroundDurationSinceLaunch += duration;
            g_state.backgroundDurationSinceLastCrash += duration;
        }
        g_state.crashedThisLaunch = true;
        saveState(stateFilePath);
    }
}

const CLKSCrash_AppState* const clkscrashstate_currentState(void)
{
    return &g_state;
}

static void setEnabled(bool isEnabled)
{
    if(isEnabled != g_isEnabled)
    {
        g_isEnabled = isEnabled;
        if(isEnabled)
        {
            clkscrashstate_reset();
        }
    }
}

static bool isEnabled()
{
    return g_isEnabled;
}


static void addContextualInfoToEvent(CLKSCrash_MonitorContext* eventContext)
{
    if(g_isEnabled)
    {
        eventContext->AppState.activeDurationSinceLastCrash = g_state.activeDurationSinceLastCrash;
        eventContext->AppState.activeDurationSinceLaunch = g_state.activeDurationSinceLaunch;
        eventContext->AppState.applicationIsActive = g_state.applicationIsActive;
        eventContext->AppState.applicationIsInForeground = g_state.applicationIsInForeground;
        eventContext->AppState.appStateTransitionTime = g_state.appStateTransitionTime;
        eventContext->AppState.backgroundDurationSinceLastCrash = g_state.backgroundDurationSinceLastCrash;
        eventContext->AppState.backgroundDurationSinceLaunch = g_state.backgroundDurationSinceLaunch;
        eventContext->AppState.crashedLastLaunch = g_state.crashedLastLaunch;
        eventContext->AppState.crashedThisLaunch = g_state.crashedThisLaunch;
        eventContext->AppState.launchesSinceLastCrash = g_state.launchesSinceLastCrash;
        eventContext->AppState.sessionsSinceLastCrash = g_state.sessionsSinceLastCrash;
        eventContext->AppState.sessionsSinceLaunch = g_state.sessionsSinceLaunch;
    }
}

CLKSCrashMonitorAPI* clkscm_appstate_getAPI()
{
    static CLKSCrashMonitorAPI api =
    {
        .setEnabled = setEnabled,
        .isEnabled = isEnabled,
        .addContextualInfoToEvent = addContextualInfoToEvent
    };
    return &api;
}
