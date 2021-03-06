//
//  CLKSCrashReportFixer.c
//
//  Created by Karl Stenerud on 2016-11-07.
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

#include "CLKSCrashReportFields.h"
#include "CLKSSystemCapabilities.h"
#include "CLKSJSONCodec.h"
#include "CLKSDemangle_CPP.h"
#if CLKSCRASH_HAS_SWIFT
#include "CLKSDemangle_Swift.h"
#endif
#include "CLKSDate.h"
#include "CLKSLogger.h"

#include <stdlib.h>
#include <string.h>

#define MAX_DEPTH 100
#define MAX_NAME_LENGTH 100

static char* datePaths[][MAX_DEPTH] =
{
    {"", CLKSCrashField_Report, CLKSCrashField_Timestamp},
    {"", CLKSCrashField_RecrashReport, CLKSCrashField_Report, CLKSCrashField_Timestamp},
};
static int datePathsCount = sizeof(datePaths) / sizeof(*datePaths);

static char* demanglePaths[][MAX_DEPTH] =
{
    {"", CLKSCrashField_Crash, CLKSCrashField_Threads, "", CLKSCrashField_Backtrace, CLKSCrashField_Contents, "", CLKSCrashField_SymbolName},
    {"", CLKSCrashField_RecrashReport, CLKSCrashField_Crash, CLKSCrashField_Threads, "", CLKSCrashField_Backtrace, CLKSCrashField_Contents, "", CLKSCrashField_SymbolName},
    {"", CLKSCrashField_Crash, CLKSCrashField_Error, CLKSCrashField_CPPException, CLKSCrashField_Name},
    {"", CLKSCrashField_RecrashReport, CLKSCrashField_Crash, CLKSCrashField_Error, CLKSCrashField_CPPException, CLKSCrashField_Name},
};
static int demanglePathsCount = sizeof(demanglePaths) / sizeof(*demanglePaths);

typedef struct
{
    CLKSJSONEncodeContext* encodeContext;
    char objectPath[MAX_DEPTH][MAX_NAME_LENGTH];
    int currentDepth;
    char* outputPtr;
    int outputBytesLeft;
} FixupContext;

static bool increaseDepth(FixupContext* context, const char* name)
{
    if(context->currentDepth >= MAX_DEPTH)
    {
        return false;
    }
    if(name == NULL)
    {
        *context->objectPath[context->currentDepth] = '\0';
    }
    else
    {
        strncpy(context->objectPath[context->currentDepth], name, sizeof(context->objectPath[context->currentDepth]));
    }
    context->currentDepth++;
    return true;
}

static bool decreaseDepth(FixupContext* context)
{
    if(context->currentDepth <= 0)
    {
        return false;
    }
    context->currentDepth--;
    return true;
}

static bool matchesPath(FixupContext* context, char** path, const char* finalName)
{
    if(finalName == NULL)
    {
        finalName = "";
    }

    for(int i = 0;i < context->currentDepth; i++)
    {
        if(strncmp(context->objectPath[i], path[i], MAX_NAME_LENGTH) != 0)
        {
            return false;
        }
    }
    if(strncmp(finalName, path[context->currentDepth], MAX_NAME_LENGTH) != 0)
    {
        return false;
    }
    return true;
}

static bool matchesAPath(FixupContext* context, const char* name, char* paths[][MAX_DEPTH], int pathsCount)
{
    for(int i = 0; i < pathsCount; i++)
    {
        if(matchesPath(context, paths[i], name))
        {
            return true;
        }
    }
    return false;
}

static bool shouldDemangle(FixupContext* context, const char* name)
{
    return matchesAPath(context, name, demanglePaths, demanglePathsCount);
}

static bool shouldFixDate(FixupContext* context, const char* name)
{
    return matchesAPath(context, name, datePaths, datePathsCount);
}

static int onBooleanElement(const char* const name,
                            const bool value,
                            void* const userData)
{
    FixupContext* context = (FixupContext*)userData;
    return clksjson_addBooleanElement(context->encodeContext, name, value);
}

static int onFloatingPointElement(const char* const name,
                                  const double value,
                                  void* const userData)
{
    FixupContext* context = (FixupContext*)userData;
    return clksjson_addFloatingPointElement(context->encodeContext, name, value);
}

static int onIntegerElement(const char* const name,
                            const int64_t value,
                            void* const userData)
{
    FixupContext* context = (FixupContext*)userData;
    int result = CLKSJSON_OK;
    if(shouldFixDate(context, name))
    {
        char buffer[21];
        clksdate_utcStringFromTimestamp((time_t)value, buffer);

        result = clksjson_addStringElement(context->encodeContext, name, buffer, (int)strlen(buffer));
    }
    else
    {
        result = clksjson_addIntegerElement(context->encodeContext, name, value);
    }
    return result;
}

static int onNullElement(const char* const name,
                         void* const userData)
{
    FixupContext* context = (FixupContext*)userData;
    return clksjson_addNullElement(context->encodeContext, name);
}

static int onStringElement(const char* const name,
                           const char* const value,
                           void* const userData)
{
    FixupContext* context = (FixupContext*)userData;
    const char* stringValue = value;
    char* demangled = NULL;
    if(shouldDemangle(context, name))
    {
        demangled = clksdm_demangleCPP(value);
#if CLKSCRASH_HAS_SWIFT
        if(demangled == NULL)
        {
            demangled = clksdm_demangleSwift(value);
        }
#endif
        if(demangled != NULL)
        {
            stringValue = demangled;
        }
    }
    int result = clksjson_addStringElement(context->encodeContext, name, stringValue, (int)strlen(stringValue));
    if(demangled != NULL)
    {
        free(demangled);
    }
    return result;
}

static int onBeginObject(const char* const name,
                         void* const userData)
{
    FixupContext* context = (FixupContext*)userData;
    int result = clksjson_beginObject(context->encodeContext, name);
    if(!increaseDepth(context, name))
    {
        return CLKSJSON_ERROR_DATA_TOO_LONG;
    }
    return result;
}

static int onBeginArray(const char* const name,
                        void* const userData)
{
    FixupContext* context = (FixupContext*)userData;
    int result = clksjson_beginArray(context->encodeContext, name);
    if(!increaseDepth(context, name))
    {
        return CLKSJSON_ERROR_DATA_TOO_LONG;
    }
    return result;
}

static int onEndContainer(void* const userData)
{
    FixupContext* context = (FixupContext*)userData;
    int result = clksjson_endContainer(context->encodeContext);
    if(!decreaseDepth(context))
    {
        // Do something;
    }
    return result;
}

static int onEndData(__unused void* const userData)
{
    FixupContext* context = (FixupContext*)userData;
    return clksjson_endEncode(context->encodeContext);
}

static int addJSONData(const char* data, int length, void* userData)
{
    FixupContext* context = (FixupContext*)userData;
    if(length > context->outputBytesLeft)
    {
        return CLKSJSON_ERROR_DATA_TOO_LONG;
    }
    memcpy(context->outputPtr, data, length);
    context->outputPtr += length;
    context->outputBytesLeft -= length;
    
    return CLKSJSON_OK;
}

char* clkscrf_fixupCrashReport(const char *crashReport)
{
    if(crashReport == NULL)
    {
        return NULL;
    }

    CLKSJSONDecodeCallbacks callbacks =
    {
        .onBeginArray = onBeginArray,
        .onBeginObject = onBeginObject,
        .onBooleanElement = onBooleanElement,
        .onEndContainer = onEndContainer,
        .onEndData = onEndData,
        .onFloatingPointElement = onFloatingPointElement,
        .onIntegerElement = onIntegerElement,
        .onNullElement = onNullElement,
        .onStringElement = onStringElement,
    };
    int stringBufferLength = 10000;
    char* stringBuffer = malloc((unsigned)stringBufferLength);
    int crashReportLength = (int)strlen(crashReport);
    int fixedReportLength = (int)(crashReportLength * 1.5);
    char* fixedReport = malloc((unsigned)fixedReportLength);
    CLKSJSONEncodeContext encodeContext;
    FixupContext fixupContext =
    {
        .encodeContext = &encodeContext,
        .currentDepth = 0,
        .outputPtr = fixedReport,
        .outputBytesLeft = fixedReportLength,
    };
    
    clksjson_beginEncode(&encodeContext, true, addJSONData, &fixupContext);
    
    int errorOffset = 0;
    int result = clksjson_decode(crashReport, (int)strlen(crashReport), stringBuffer, stringBufferLength, &callbacks, &fixupContext, &errorOffset);
    *fixupContext.outputPtr = '\0';
    free(stringBuffer);
    if(result != CLKSJSON_OK)
    {
        CLKSLOG_ERROR("Could not decode report: %s", clksjson_stringForError(result));
        free(fixedReport);
        return NULL;
    }
    return fixedReport;
}
