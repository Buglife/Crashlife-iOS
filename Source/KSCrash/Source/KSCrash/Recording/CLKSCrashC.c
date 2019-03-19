//
//  CLKSCrashC.c
//
//  Created by Karl Stenerud on 2012-01-28.
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


#include "CLKSCrashC.h"

#include "CLKSCrashCachedData.h"
#include "CLKSCrashReport.h"
#include "CLKSCrashReportFixer.h"
#include "CLKSCrashReportStore.h"
#include "CLKSCrashMonitor_Deadlock.h"
#include "CLKSCrashMonitor_User.h"
#include "CLKSFileUtils.h"
#include "CLKSObjC.h"
#include "CLKSString.h"
#include "CLKSCrashMonitor_System.h"
#include "CLKSCrashMonitor_Zombie.h"
#include "CLKSCrashMonitor_AppState.h"
#include "CLKSCrashMonitorContext.h"
#include "CLKSSystemCapabilities.h"

//#define CLKSLogger_LocalLevel TRACE
#include "CLKSLogger.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// ============================================================================
#pragma mark - Globals -
// ============================================================================

/** True if CLKSCrash has been installed. */
static volatile bool g_installed = 0;

static bool g_shouldAddConsoleLogToReport = false;
static bool g_shouldPrintPreviousLog = false;
static char g_consoleLogPath[CLKSFU_MAX_PATH_LENGTH];
static CLKSCrashMonitorType g_monitoring = CLKSCrashMonitorTypeProductionSafeMinimal;
static char g_lastCrashReportFilePath[CLKSFU_MAX_PATH_LENGTH];
static CLKSReportWrittenCallback g_reportWrittenCallback;


// ============================================================================
#pragma mark - Utility -
// ============================================================================

static void printPreviousLog(const char* filePath)
{
    char* data;
    int length;
    if(clksfu_readEntireFile(filePath, &data, &length, 0))
    {
        printf("\nvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv Previous Log vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n\n");
        printf("%s\n", data);
        printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n\n");
        fflush(stdout);
    }
}


// ============================================================================
#pragma mark - Callbacks -
// ============================================================================

/** Called when a crash occurs.
 *
 * This function gets passed as a callback to a crash handler.
 */
static void onCrash(struct CLKSCrash_MonitorContext* monitorContext)
{
    if (monitorContext->currentSnapshotUserReported == false) {
        CLKSLOG_DEBUG("Updating application state to note crash.");
        clkscrashstate_notifyAppCrash();
    }
    monitorContext->consoleLogPath = g_shouldAddConsoleLogToReport ? g_consoleLogPath : NULL;

    if(monitorContext->crashedDuringCrashHandling)
    {
        clkscrashreport_writeRecrashReport(monitorContext, g_lastCrashReportFilePath);
    }
    else
    {
        char crashReportFilePath[CLKSFU_MAX_PATH_LENGTH];
        int64_t reportID = clkscrs_getNextCrashReport(crashReportFilePath);
        strncpy(g_lastCrashReportFilePath, crashReportFilePath, sizeof(g_lastCrashReportFilePath));
        clkscrashreport_writeStandardReport(monitorContext, crashReportFilePath);

        if(g_reportWrittenCallback)
        {
            g_reportWrittenCallback(reportID);
        }
    }
}


// ============================================================================
#pragma mark - API -
// ============================================================================

CLKSCrashMonitorType clkscrash_install(const char *appName, const char *const installPath)
{
    CLKSLOG_DEBUG("Installing crash reporter.");

    if(g_installed)
    {
        CLKSLOG_DEBUG("Crash reporter already installed.");
        return g_monitoring;
    }
    g_installed = 1;

    char path[CLKSFU_MAX_PATH_LENGTH];
    snprintf(path, sizeof(path), "%s/Reports", installPath);
    clksfu_makePath(path);
    clkscrs_initialize(appName, path);

    snprintf(path, sizeof(path), "%s/Data", installPath);
    clksfu_makePath(path);
    snprintf(path, sizeof(path), "%s/Data/CrashState.json", installPath);
    clkscrashstate_initialize(path);

    snprintf(g_consoleLogPath, sizeof(g_consoleLogPath), "%s/Data/ConsoleLog.txt", installPath);
    if(g_shouldPrintPreviousLog)
    {
        printPreviousLog(g_consoleLogPath);
    }
    clkslog_setLogFilename(g_consoleLogPath, true);

    clksccd_init(60);

    clkscm_setEventCallback(onCrash);
    CLKSCrashMonitorType monitors = clkscrash_setMonitoring(g_monitoring);

    CLKSLOG_DEBUG("Installation complete.");
    return monitors;
}

CLKSCrashMonitorType clkscrash_setMonitoring(CLKSCrashMonitorType monitors)
{
    g_monitoring = monitors;
    
    if(g_installed)
    {
        clkscm_setActiveMonitors(monitors);
        return clkscm_getActiveMonitors();
    }
    // Return what we will be monitoring in future.
    return g_monitoring;
}

void clkscrash_setUserInfoJSON(const char *const userInfoJSON)
{
    clkscrashreport_setUserInfoJSON(userInfoJSON);
}

void clkscrash_setDeadlockWatchdogInterval(double deadlockWatchdogInterval)
{
#if CLKSCRASH_HAS_OBJC
    clkscm_setDeadlockHandlerWatchdogInterval(deadlockWatchdogInterval);
#endif
}

void clkscrash_setSearchQueueNames(bool searchQueueNames)
{
    clksccd_setSearchQueueNames(searchQueueNames);
}

void clkscrash_setIntrospectMemory(bool introspectMemory)
{
    clkscrashreport_setIntrospectMemory(introspectMemory);
}

void clkscrash_setDoNotIntrospectClasses(const char **doNotIntrospectClasses, int length)
{
    clkscrashreport_setDoNotIntrospectClasses(doNotIntrospectClasses, length);
}

void clkscrash_setCrashNotifyCallback(const CLKSReportWriteCallback onCrashNotify)
{
    clkscrashreport_setUserSectionWriteCallback(onCrashNotify);
}

void clkscrash_setReportWrittenCallback(const CLKSReportWrittenCallback onReportWrittenNotify)
{
    g_reportWrittenCallback = onReportWrittenNotify;
}

void clkscrash_setAddConsoleLogToReport(bool shouldAddConsoleLogToReport)
{
    g_shouldAddConsoleLogToReport = shouldAddConsoleLogToReport;
}

void clkscrash_setPrintPreviousLog(bool shouldPrintPreviousLog)
{
    g_shouldPrintPreviousLog = shouldPrintPreviousLog;
}

void clkscrash_setMaxReportCount(int maxReportCount)
{
    clkscrs_setMaxReportCount(maxReportCount);
}

void clkscrash_reportUserException(const char *name,
        const char *reason,
        const char *language,
        const char *lineOfCode,
        const char *stackTrace,
        bool logAllThreads,
        bool terminateProgram)
{
    clkscm_reportUserException(name,
            reason,
            language,
            lineOfCode,
            stackTrace,
            logAllThreads,
            terminateProgram);
    if(g_shouldAddConsoleLogToReport)
    {
        clkslog_clearLogFile();
    }
}

void clkscrash_notifyAppActive(bool isActive)
{
    clkscrashstate_notifyAppActive(isActive);
}

void clkscrash_notifyAppInForeground(bool isInForeground)
{
    clkscrashstate_notifyAppInForeground(isInForeground);
}

void clkscrash_notifyAppTerminate(void)
{
    clkscrashstate_notifyAppTerminate();
}

void clkscrash_notifyAppCrash(void)
{
    clkscrashstate_notifyAppCrash();
}

int clkscrash_getReportCount()
{
    return clkscrs_getReportCount();
}

int clkscrash_getReportIDs(int64_t *reportIDs, int count)
{
    return clkscrs_getReportIDs(reportIDs, count);
}

char* clkscrash_readReport(int64_t reportID)
{
    if(reportID <= 0)
    {
        CLKSLOG_ERROR("Report ID was %" PRIx64, reportID);
        return NULL;
    }

    char* rawReport = clkscrs_readReport(reportID);
    if(rawReport == NULL)
    {
        CLKSLOG_ERROR("Failed to load report ID %" PRIx64, reportID);
        return NULL;
    }

    char* fixedReport = clkscrf_fixupCrashReport(rawReport);
    if(fixedReport == NULL)
    {
        CLKSLOG_ERROR("Failed to fixup report ID %" PRIx64, reportID);
    }

    free(rawReport);
    return fixedReport;
}

int64_t clkscrash_addUserReport(const char *report, int reportLength)
{
    return clkscrs_addUserReport(report, reportLength);
}

void clkscrash_deleteAllReports()
{
    clkscrs_deleteAllReports();
}

void clkscrash_deleteReportWithID(int64_t reportID)
{
    clkscrs_deleteReportWithID(reportID);
}
