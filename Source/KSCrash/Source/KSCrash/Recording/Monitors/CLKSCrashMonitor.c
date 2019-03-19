//
//  CLKSCrashMonitor.c
//
//  Created by Karl Stenerud on 2012-02-12.
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


#include "CLKSCrashMonitor.h"
#include "CLKSCrashMonitorContext.h"
#include "CLKSCrashMonitorType.h"

#include "CLKSCrashMonitor_Deadlock.h"
#include "CLKSCrashMonitor_MachException.h"
#include "CLKSCrashMonitor_CPPException.h"
#include "CLKSCrashMonitor_NSException.h"
#include "CLKSCrashMonitor_Signal.h"
#include "CLKSCrashMonitor_System.h"
#include "CLKSCrashMonitor_User.h"
#include "CLKSCrashMonitor_AppState.h"
#include "CLKSCrashMonitor_Zombie.h"
#include "CLKSDebug.h"
#include "CLKSThread.h"
#include "CLKSSystemCapabilities.h"

#include <memory.h>

//#define CLKSLogger_LocalLevel TRACE
#include "CLKSLogger.h"


// ============================================================================
#pragma mark - Globals -
// ============================================================================

typedef struct
{
    CLKSCrashMonitorType monitorType;
    CLKSCrashMonitorAPI* (*getAPI)(void);
} Monitor;

static Monitor g_monitors[] =
{
#if CLKSCRASH_HAS_MACH
    {
        .monitorType = CLKSCrashMonitorTypeMachException,
        .getAPI = clkscm_machexception_getAPI,
    },
#endif
#if CLKSCRASH_HAS_SIGNAL
    {
        .monitorType = CLKSCrashMonitorTypeSignal,
        .getAPI = clkscm_signal_getAPI,
    },
#endif
#if CLKSCRASH_HAS_OBJC
    {
        .monitorType = CLKSCrashMonitorTypeNSException,
        .getAPI = clkscm_nsexception_getAPI,
    },
    {
        .monitorType = CLKSCrashMonitorTypeMainThreadDeadlock,
        .getAPI = clkscm_deadlock_getAPI,
    },
    {
        .monitorType = CLKSCrashMonitorTypeZombie,
        .getAPI = clkscm_zombie_getAPI,
    },
#endif
    {
        .monitorType = CLKSCrashMonitorTypeCPPException,
        .getAPI = clkscm_cppexception_getAPI,
    },
    {
        .monitorType = CLKSCrashMonitorTypeUserReported,
        .getAPI = clkscm_user_getAPI,
    },
    {
        .monitorType = CLKSCrashMonitorTypeSystem,
        .getAPI = clkscm_system_getAPI,
    },
    {
        .monitorType = CLKSCrashMonitorTypeApplicationState,
        .getAPI = clkscm_appstate_getAPI,
    },
};
static int g_monitorsCount = sizeof(g_monitors) / sizeof(*g_monitors);

static CLKSCrashMonitorType g_activeMonitors = CLKSCrashMonitorTypeNone;

static bool g_handlingFatalException = false;
static bool g_crashedDuringExceptionHandling = false;
static bool g_requiresAsyncSafety = false;

static void (*g_onExceptionEvent)(struct CLKSCrash_MonitorContext* monitorContext);

// ============================================================================
#pragma mark - API -
// ============================================================================

static inline CLKSCrashMonitorAPI* getAPI(Monitor* monitor)
{
    if(monitor != NULL && monitor->getAPI != NULL)
    {
        return monitor->getAPI();
    }
    return NULL;
}

static inline void setMonitorEnabled(Monitor* monitor, bool isEnabled)
{
    CLKSCrashMonitorAPI* api = getAPI(monitor);
    if(api != NULL && api->setEnabled != NULL)
    {
        api->setEnabled(isEnabled);
    }
}

static inline bool isMonitorEnabled(Monitor* monitor)
{
    CLKSCrashMonitorAPI* api = getAPI(monitor);
    if(api != NULL && api->isEnabled != NULL)
    {
        return api->isEnabled();
    }
    return false;
}

static inline void addContextualInfoToEvent(Monitor* monitor, struct CLKSCrash_MonitorContext* eventContext)
{
    CLKSCrashMonitorAPI* api = getAPI(monitor);
    if(api != NULL && api->addContextualInfoToEvent != NULL)
    {
        api->addContextualInfoToEvent(eventContext);
    }
}

void clkscm_setEventCallback(void (*onEvent)(struct CLKSCrash_MonitorContext *monitorContext))
{
    g_onExceptionEvent = onEvent;
}

void clkscm_setActiveMonitors(CLKSCrashMonitorType monitorTypes)
{
    if(clksdebug_isBeingTraced() && (monitorTypes & CLKSCrashMonitorTypeDebuggerUnsafe))
    {
        static bool hasWarned = false;
        if(!hasWarned)
        {
            hasWarned = true;
            CLKSLOGBASIC_WARN("    ************************ Crash Handler Notice ************************");
            CLKSLOGBASIC_WARN("    *     App is running in a debugger. Masking out unsafe monitors.     *");
            CLKSLOGBASIC_WARN("    * This means that most crashes WILL NOT BE RECORDED while debugging! *");
            CLKSLOGBASIC_WARN("    **********************************************************************");
        }
        monitorTypes &= CLKSCrashMonitorTypeDebuggerSafe;
    }
    if(g_requiresAsyncSafety && (monitorTypes & CLKSCrashMonitorTypeAsyncUnsafe))
    {
        CLKSLOG_DEBUG("Async-safe environment detected. Masking out unsafe monitors.");
        monitorTypes &= CLKSCrashMonitorTypeAsyncSafe;
    }

    CLKSLOG_DEBUG("Changing active monitors from 0x%x tp 0x%x.", g_activeMonitors, monitorTypes);

    CLKSCrashMonitorType activeMonitors = CLKSCrashMonitorTypeNone;
    for(int i = 0; i < g_monitorsCount; i++)
    {
        Monitor* monitor = &g_monitors[i];
        bool isEnabled = monitor->monitorType & monitorTypes;
        setMonitorEnabled(monitor, isEnabled);
        if(isMonitorEnabled(monitor))
        {
            activeMonitors |= monitor->monitorType;
        }
        else
        {
            activeMonitors &= ~monitor->monitorType;
        }
    }

    CLKSLOG_DEBUG("Active monitors are now 0x%x.", activeMonitors);
    g_activeMonitors = activeMonitors;
}

CLKSCrashMonitorType clkscm_getActiveMonitors()
{
    return g_activeMonitors;
}


// ============================================================================
#pragma mark - Private API -
// ============================================================================

bool clkscm_notifyFatalExceptionCaptured(bool isAsyncSafeEnvironment)
{
    g_requiresAsyncSafety |= isAsyncSafeEnvironment; // Don't let it be unset.
    if(g_handlingFatalException)
    {
        g_crashedDuringExceptionHandling = true;
    }
    g_handlingFatalException = true;
    if(g_crashedDuringExceptionHandling)
    {
        CLKSLOG_INFO("Detected crash in the crash reporter. Uninstalling CLKSCrash.");
        clkscm_setActiveMonitors(CLKSCrashMonitorTypeNone);
    }
    return g_crashedDuringExceptionHandling;
}

void clkscm_handleException(struct CLKSCrash_MonitorContext *context)
{
    context->requiresAsyncSafety = g_requiresAsyncSafety;
    if(g_crashedDuringExceptionHandling)
    {
        context->crashedDuringCrashHandling = true;
    }
    for(int i = 0; i < g_monitorsCount; i++)
    {
        Monitor* monitor = &g_monitors[i];
        if(isMonitorEnabled(monitor))
        {
            addContextualInfoToEvent(monitor, context);
        }
    }

    g_onExceptionEvent(context);

    if (context->currentSnapshotUserReported) {
        g_handlingFatalException = false;
    } else {
        if(g_handlingFatalException && !g_crashedDuringExceptionHandling) {
            CLKSLOG_DEBUG("Exception is fatal. Restoring original handlers.");
            clkscm_setActiveMonitors(CLKSCrashMonitorTypeNone);
        }
    }
}
