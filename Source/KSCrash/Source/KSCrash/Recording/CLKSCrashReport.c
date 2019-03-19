//
//  CLKSCrashReport.m
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


#include "CLKSCrashReport.h"

#include "CLKSCrashReportFields.h"
#include "CLKSCrashReportWriter.h"
#include "CLKSDynamicLinker.h"
#include "CLKSFileUtils.h"
#include "CLKSJSONCodec.h"
#include "CLKSCPU.h"
#include "CLKSMemory.h"
#include "CLKSMach.h"
#include "CLKSThread.h"
#include "CLKSObjC.h"
#include "CLKSSignalInfo.h"
#include "CLKSCrashMonitor_Zombie.h"
#include "CLKSString.h"
#include "CLKSCrashReportVersion.h"
#include "CLKSStackCursor_Backtrace.h"
#include "CLKSStackCursor_MachineContext.h"
#include "CLKSSystemCapabilities.h"
#include "CLKSCrashCachedData.h"

//#define CLKSLogger_LocalLevel TRACE
#include "CLKSLogger.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


// ============================================================================
#pragma mark - Constants -
// ============================================================================

/** Default number of objects, subobjects, and ivars to record from a memory loc */
#define kDefaultMemorySearchDepth 15

/** How far to search the stack (in pointer sized jumps) for notable data. */
#define kStackNotableSearchBackDistance 20
#define kStackNotableSearchForwardDistance 10

/** How much of the stack to dump (in pointer sized jumps). */
#define kStackContentsPushedDistance 20
#define kStackContentsPoppedDistance 10
#define kStackContentsTotalDistance (kStackContentsPushedDistance + kStackContentsPoppedDistance)

/** The minimum length for a valid string. */
#define kMinStringLength 4


// ============================================================================
#pragma mark - JSON Encoding -
// ============================================================================

#define getJsonContext(REPORT_WRITER) ((CLKSJSONEncodeContext*)((REPORT_WRITER)->context))

/** Used for writing hex string values. */
static const char g_hexNybbles[] =
{
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

// ============================================================================
#pragma mark - Runtime Config -
// ============================================================================

typedef struct
{
    /** If YES, introspect memory contents during a crash.
     * Any Objective-C objects or C strings near the stack pointer or referenced by
     * cpu registers or exceptions will be recorded in the crash report, along with
     * their contents.
     */
    bool enabled;
    
    /** List of classes that should never be introspected.
     * Whenever a class in this list is encountered, only the class name will be recorded.
     */
    const char** restrictedClasses;
    int restrictedClassesCount;
} CLKSCrash_IntrospectionRules;

static const char* g_userInfoJSON;
static CLKSCrash_IntrospectionRules g_introspectionRules;
static CLKSReportWriteCallback g_userSectionWriteCallback;


#pragma mark Callbacks

static void addBooleanElement(const CLKSCrashReportWriter* const writer, const char* const key, const bool value)
{
    clksjson_addBooleanElement(getJsonContext(writer), key, value);
}

static void addFloatingPointElement(const CLKSCrashReportWriter* const writer, const char* const key, const double value)
{
    clksjson_addFloatingPointElement(getJsonContext(writer), key, value);
}

static void addIntegerElement(const CLKSCrashReportWriter* const writer, const char* const key, const int64_t value)
{
    clksjson_addIntegerElement(getJsonContext(writer), key, value);
}

static void addUIntegerElement(const CLKSCrashReportWriter* const writer, const char* const key, const uint64_t value)
{
    clksjson_addIntegerElement(getJsonContext(writer), key, (int64_t)value);
}

static void addStringElement(const CLKSCrashReportWriter* const writer, const char* const key, const char* const value)
{
    clksjson_addStringElement(getJsonContext(writer), key, value, CLKSJSON_SIZE_AUTOMATIC);
}

static void addTextFileElement(const CLKSCrashReportWriter* const writer, const char* const key, const char* const filePath)
{
    const int fd = open(filePath, O_RDONLY);
    if(fd < 0)
    {
        CLKSLOG_ERROR("Could not open file %s: %s", filePath, strerror(errno));
        return;
    }

    if(clksjson_beginStringElement(getJsonContext(writer), key) != CLKSJSON_OK)
    {
        CLKSLOG_ERROR("Could not start string element");
        goto done;
    }

    char buffer[512];
    int bytesRead;
    for(bytesRead = (int)read(fd, buffer, sizeof(buffer));
        bytesRead > 0;
        bytesRead = (int)read(fd, buffer, sizeof(buffer)))
    {
        if(clksjson_appendStringElement(getJsonContext(writer), buffer, bytesRead) != CLKSJSON_OK)
        {
            CLKSLOG_ERROR("Could not append string element");
            goto done;
        }
    }

done:
    clksjson_endStringElement(getJsonContext(writer));
    close(fd);
}

static void addDataElement(const CLKSCrashReportWriter* const writer,
                           const char* const key,
                           const char* const value,
                           const int length)
{
    clksjson_addDataElement(getJsonContext(writer), key, value, length);
}

static void beginDataElement(const CLKSCrashReportWriter* const writer, const char* const key)
{
    clksjson_beginDataElement(getJsonContext(writer), key);
}

static void appendDataElement(const CLKSCrashReportWriter* const writer, const char* const value, const int length)
{
    clksjson_appendDataElement(getJsonContext(writer), value, length);
}

static void endDataElement(const CLKSCrashReportWriter* const writer)
{
    clksjson_endDataElement(getJsonContext(writer));
}

static void addUUIDElement(const CLKSCrashReportWriter* const writer, const char* const key, const unsigned char* const value)
{
    if(value == NULL)
    {
        clksjson_addNullElement(getJsonContext(writer), key);
    }
    else
    {
        char uuidBuffer[37];
        const unsigned char* src = value;
        char* dst = uuidBuffer;
        for(int i = 0; i < 4; i++)
        {
            *dst++ = g_hexNybbles[(*src>>4)&15];
            *dst++ = g_hexNybbles[(*src++)&15];
        }
        *dst++ = '-';
        for(int i = 0; i < 2; i++)
        {
            *dst++ = g_hexNybbles[(*src>>4)&15];
            *dst++ = g_hexNybbles[(*src++)&15];
        }
        *dst++ = '-';
        for(int i = 0; i < 2; i++)
        {
            *dst++ = g_hexNybbles[(*src>>4)&15];
            *dst++ = g_hexNybbles[(*src++)&15];
        }
        *dst++ = '-';
        for(int i = 0; i < 2; i++)
        {
            *dst++ = g_hexNybbles[(*src>>4)&15];
            *dst++ = g_hexNybbles[(*src++)&15];
        }
        *dst++ = '-';
        for(int i = 0; i < 6; i++)
        {
            *dst++ = g_hexNybbles[(*src>>4)&15];
            *dst++ = g_hexNybbles[(*src++)&15];
        }

        clksjson_addStringElement(getJsonContext(writer), key, uuidBuffer, (int)(dst - uuidBuffer));
    }
}

static void addJSONElement(const CLKSCrashReportWriter* const writer,
                           const char* const key,
                           const char* const jsonElement,
                           bool closeLastContainer)
{
    int jsonResult = clksjson_addJSONElement(getJsonContext(writer),
                                           key,
                                           jsonElement,
                                           (int)strlen(jsonElement),
                                           closeLastContainer);
    if(jsonResult != CLKSJSON_OK)
    {
        char errorBuff[100];
        snprintf(errorBuff,
                 sizeof(errorBuff),
                 "Invalid JSON data: %s",
                 clksjson_stringForError(jsonResult));
        clksjson_beginObject(getJsonContext(writer), key);
        clksjson_addStringElement(getJsonContext(writer),
                                CLKSCrashField_Error,
                                errorBuff,
                                CLKSJSON_SIZE_AUTOMATIC);
        clksjson_addStringElement(getJsonContext(writer),
                                CLKSCrashField_JSONData,
                                jsonElement,
                                CLKSJSON_SIZE_AUTOMATIC);
        clksjson_endContainer(getJsonContext(writer));
    }
}

static void addJSONElementFromFile(const CLKSCrashReportWriter* const writer,
                                   const char* const key,
                                   const char* const filePath,
                                   bool closeLastContainer)
{
    clksjson_addJSONFromFile(getJsonContext(writer), key, filePath, closeLastContainer);
}

static void beginObject(const CLKSCrashReportWriter* const writer, const char* const key)
{
    clksjson_beginObject(getJsonContext(writer), key);
}

static void beginArray(const CLKSCrashReportWriter* const writer, const char* const key)
{
    clksjson_beginArray(getJsonContext(writer), key);
}

static void endContainer(const CLKSCrashReportWriter* const writer)
{
    clksjson_endContainer(getJsonContext(writer));
}


static void addTextLinesFromFile(const CLKSCrashReportWriter* const writer, const char* const key, const char* const filePath)
{
    char readBuffer[1024];
    CLKSBufferedReader reader;
    if(!clksfu_openBufferedReader(&reader, filePath, readBuffer, sizeof(readBuffer)))
    {
        return;
    }
    char buffer[1024];
    beginArray(writer, key);
    {
        for(;;)
        {
            int length = sizeof(buffer);
            clksfu_readBufferedReaderUntilChar(&reader, '\n', buffer, &length);
            if(length <= 0)
            {
                break;
            }
            buffer[length - 1] = '\0';
            clksjson_addStringElement(getJsonContext(writer), NULL, buffer, CLKSJSON_SIZE_AUTOMATIC);
        }
    }
    endContainer(writer);
    clksfu_closeBufferedReader(&reader);
}

static int addJSONData(const char* restrict const data, const int length, void* restrict userData)
{
    CLKSBufferedWriter* writer = (CLKSBufferedWriter*)userData;
    const bool success = clksfu_writeBufferedWriter(writer, data, length);
    return success ? CLKSJSON_OK : CLKSJSON_ERROR_CANNOT_ADD_DATA;
}


// ============================================================================
#pragma mark - Utility -
// ============================================================================

/** Check if a memory address points to a valid null terminated UTF-8 string.
 *
 * @param address The address to check.
 *
 * @return true if the address points to a string.
 */
static bool isValidString(const void* const address)
{
    if((void*)address == NULL)
    {
        return false;
    }

    char buffer[500];
    if((uintptr_t)address+sizeof(buffer) < (uintptr_t)address)
    {
        // Wrapped around the address range.
        return false;
    }
    if(!clksmem_copySafely(address, buffer, sizeof(buffer)))
    {
        return false;
    }
    return clksstring_isNullTerminatedUTF8String(buffer, kMinStringLength, sizeof(buffer));
}

/** Get the backtrace for the specified machine context.
 *
 * This function will choose how to fetch the backtrace based on the crash and
 * machine context. It may store the backtrace in backtraceBuffer unless it can
 * be fetched directly from memory. Do not count on backtraceBuffer containing
 * anything. Always use the return value.
 *
 * @param crash The crash handler context.
 *
 * @param machineContext The machine context.
 *
 * @param cursor The stack cursor to fill.
 *
 * @return True if the cursor was filled.
 */
static bool getStackCursor(const CLKSCrash_MonitorContext* const crash,
                           const struct CLKSMachineContext* const machineContext,
                           CLKSStackCursor *cursor)
{
    if(clksmc_getThreadFromContext(machineContext) == clksmc_getThreadFromContext(crash->offendingMachineContext))
    {
        *cursor = *((CLKSStackCursor*)crash->stackCursor);
        return true;
    }

    clkssc_initWithMachineContext(cursor, CLKSSC_STACK_OVERFLOW_THRESHOLD, machineContext);
    return true;
}


// ============================================================================
#pragma mark - Report Writing -
// ============================================================================

/** Write the contents of a memory location.
 * Also writes meta information about the data.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param address The memory address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeMemoryContents(const CLKSCrashReportWriter* const writer,
                                const char* const key,
                                const uintptr_t address,
                                int* limit);

/** Write a string to the report.
 * This will only print the first child of the array.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param objectAddress The object's address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeNSStringContents(const CLKSCrashReportWriter* const writer,
                                  const char* const key,
                                  const uintptr_t objectAddress,
                                  __unused int* limit)
{
    const void* object = (const void*)objectAddress;
    char buffer[200];
    if(clksobjc_copyStringContents(object, buffer, sizeof(buffer)))
    {
        writer->addStringElement(writer, key, buffer);
    }
}

/** Write a URL to the report.
 * This will only print the first child of the array.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param objectAddress The object's address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeURLContents(const CLKSCrashReportWriter* const writer,
                             const char* const key,
                             const uintptr_t objectAddress,
                             __unused int* limit)
{
    const void* object = (const void*)objectAddress;
    char buffer[200];
    if(clksobjc_copyStringContents(object, buffer, sizeof(buffer)))
    {
        writer->addStringElement(writer, key, buffer);
    }
}

/** Write a date to the report.
 * This will only print the first child of the array.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param objectAddress The object's address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeDateContents(const CLKSCrashReportWriter* const writer,
                              const char* const key,
                              const uintptr_t objectAddress,
                              __unused int* limit)
{
    const void* object = (const void*)objectAddress;
    writer->addFloatingPointElement(writer, key, clksobjc_dateContents(object));
}

/** Write a number to the report.
 * This will only print the first child of the array.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param objectAddress The object's address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeNumberContents(const CLKSCrashReportWriter* const writer,
                                const char* const key,
                                const uintptr_t objectAddress,
                                __unused int* limit)
{
    const void* object = (const void*)objectAddress;
    writer->addFloatingPointElement(writer, key, clksobjc_numberAsFloat(object));
}

/** Write an array to the report.
 * This will only print the first child of the array.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param objectAddress The object's address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeArrayContents(const CLKSCrashReportWriter* const writer,
                               const char* const key,
                               const uintptr_t objectAddress,
                               int* limit)
{
    const void* object = (const void*)objectAddress;
    uintptr_t firstObject;
    if(clksobjc_arrayContents(object, &firstObject, 1) == 1)
    {
        writeMemoryContents(writer, key, firstObject, limit);
    }
}

/** Write out ivar information about an unknown object.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param objectAddress The object's address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeUnknownObjectContents(const CLKSCrashReportWriter* const writer,
                                       const char* const key,
                                       const uintptr_t objectAddress,
                                       int* limit)
{
    (*limit)--;
    const void* object = (const void*)objectAddress;
    CLKSObjCIvar ivars[10];
    int8_t s8;
    int16_t s16;
    int sInt;
    int32_t s32;
    int64_t s64;
    uint8_t u8;
    uint16_t u16;
    unsigned int uInt;
    uint32_t u32;
    uint64_t u64;
    float f32;
    double f64;
    bool b;
    void* pointer;
    
    
    writer->beginObject(writer, key);
    {
        if(clksobjc_isTaggedPointer(object))
        {
            writer->addIntegerElement(writer, "tagged_payload", (int64_t)clksobjc_taggedPointerPayload(object));
        }
        else
        {
            const void* class = clksobjc_isaPointer(object);
            int ivarCount = clksobjc_ivarList(class, ivars, sizeof(ivars)/sizeof(*ivars));
            *limit -= ivarCount;
            for(int i = 0; i < ivarCount; i++)
            {
                CLKSObjCIvar* ivar = &ivars[i];
                switch(ivar->type[0])
                {
                    case 'c':
                        clksobjc_ivarValue(object, ivar->index, &s8);
                        writer->addIntegerElement(writer, ivar->name, s8);
                        break;
                    case 'i':
                        clksobjc_ivarValue(object, ivar->index, &sInt);
                        writer->addIntegerElement(writer, ivar->name, sInt);
                        break;
                    case 's':
                        clksobjc_ivarValue(object, ivar->index, &s16);
                        writer->addIntegerElement(writer, ivar->name, s16);
                        break;
                    case 'l':
                        clksobjc_ivarValue(object, ivar->index, &s32);
                        writer->addIntegerElement(writer, ivar->name, s32);
                        break;
                    case 'q':
                        clksobjc_ivarValue(object, ivar->index, &s64);
                        writer->addIntegerElement(writer, ivar->name, s64);
                        break;
                    case 'C':
                        clksobjc_ivarValue(object, ivar->index, &u8);
                        writer->addUIntegerElement(writer, ivar->name, u8);
                        break;
                    case 'I':
                        clksobjc_ivarValue(object, ivar->index, &uInt);
                        writer->addUIntegerElement(writer, ivar->name, uInt);
                        break;
                    case 'S':
                        clksobjc_ivarValue(object, ivar->index, &u16);
                        writer->addUIntegerElement(writer, ivar->name, u16);
                        break;
                    case 'L':
                        clksobjc_ivarValue(object, ivar->index, &u32);
                        writer->addUIntegerElement(writer, ivar->name, u32);
                        break;
                    case 'Q':
                        clksobjc_ivarValue(object, ivar->index, &u64);
                        writer->addUIntegerElement(writer, ivar->name, u64);
                        break;
                    case 'f':
                        clksobjc_ivarValue(object, ivar->index, &f32);
                        writer->addFloatingPointElement(writer, ivar->name, f32);
                        break;
                    case 'd':
                        clksobjc_ivarValue(object, ivar->index, &f64);
                        writer->addFloatingPointElement(writer, ivar->name, f64);
                        break;
                    case 'B':
                        clksobjc_ivarValue(object, ivar->index, &b);
                        writer->addBooleanElement(writer, ivar->name, b);
                        break;
                    case '*':
                    case '@':
                    case '#':
                    case ':':
                        clksobjc_ivarValue(object, ivar->index, &pointer);
                        writeMemoryContents(writer, ivar->name, (uintptr_t)pointer, limit);
                        break;
                    default:
                        CLKSLOG_DEBUG("%s: Unknown ivar type [%s]", ivar->name, ivar->type);
                }
            }
        }
    }
    writer->endContainer(writer);
}

static bool isRestrictedClass(const char* name)
{
    if(g_introspectionRules.restrictedClasses != NULL)
    {
        for(int i = 0; i < g_introspectionRules.restrictedClassesCount; i++)
        {
            if(strcmp(name, g_introspectionRules.restrictedClasses[i]) == 0)
            {
                return true;
            }
        }
    }
    return false;
}

static void writeZombieIfPresent(const CLKSCrashReportWriter* const writer,
                                 const char* const key,
                                 const uintptr_t address)
{
#if CLKSCRASH_HAS_OBJC
    const void* object = (const void*)address;
    const char* zombieClassName = clkszombie_className(object);
    if(zombieClassName != NULL)
    {
        writer->addStringElement(writer, key, zombieClassName);
    }
#endif
}

static bool writeObjCObject(const CLKSCrashReportWriter* const writer,
                            const uintptr_t address,
                            int* limit)
{
#if CLKSCRASH_HAS_OBJC
    const void* object = (const void*)address;
    switch(clksobjc_objectType(object))
    {
        case CLKSObjCTypeClass:
            writer->addStringElement(writer, CLKSCrashField_Type, CLKSCrashMemType_Class);
            writer->addStringElement(writer, CLKSCrashField_Class, clksobjc_className(object));
            return true;
        case CLKSObjCTypeObject:
        {
            writer->addStringElement(writer, CLKSCrashField_Type, CLKSCrashMemType_Object);
            const char* className = clksobjc_objectClassName(object);
            writer->addStringElement(writer, CLKSCrashField_Class, className);
            if(!isRestrictedClass(className))
            {
                switch(clksobjc_objectClassType(object))
                {
                    case CLKSObjCClassTypeString:
                        writeNSStringContents(writer, CLKSCrashField_Value, address, limit);
                        return true;
                    case CLKSObjCClassTypeURL:
                        writeURLContents(writer, CLKSCrashField_Value, address, limit);
                        return true;
                    case CLKSObjCClassTypeDate:
                        writeDateContents(writer, CLKSCrashField_Value, address, limit);
                        return true;
                    case CLKSObjCClassTypeArray:
                        if(*limit > 0)
                        {
                            writeArrayContents(writer, CLKSCrashField_FirstObject, address, limit);
                        }
                        return true;
                    case CLKSObjCClassTypeNumber:
                        writeNumberContents(writer, CLKSCrashField_Value, address, limit);
                        return true;
                    case CLKSObjCClassTypeDictionary:
                    case CLKSObjCClassTypeException:
                        // TODO: Implement these.
                        if(*limit > 0)
                        {
                            writeUnknownObjectContents(writer, CLKSCrashField_Ivars, address, limit);
                        }
                        return true;
                    case CLKSObjCClassTypeUnknown:
                        if(*limit > 0)
                        {
                            writeUnknownObjectContents(writer, CLKSCrashField_Ivars, address, limit);
                        }
                        return true;
                }
            }
            break;
        }
        case CLKSObjCTypeBlock:
            writer->addStringElement(writer, CLKSCrashField_Type, CLKSCrashMemType_Block);
            const char* className = clksobjc_objectClassName(object);
            writer->addStringElement(writer, CLKSCrashField_Class, className);
            return true;
        case CLKSObjCTypeUnknown:
            break;
    }
#endif

    return false;
}

/** Write the contents of a memory location.
 * Also writes meta information about the data.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param address The memory address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeMemoryContents(const CLKSCrashReportWriter* const writer,
                                const char* const key,
                                const uintptr_t address,
                                int* limit)
{
    (*limit)--;
    const void* object = (const void*)address;
    writer->beginObject(writer, key);
    {
        writer->addUIntegerElement(writer, CLKSCrashField_Address, address);
        writeZombieIfPresent(writer, CLKSCrashField_LastDeallocObject, address);
        if(!writeObjCObject(writer, address, limit))
        {
            if(object == NULL)
            {
                writer->addStringElement(writer, CLKSCrashField_Type, CLKSCrashMemType_NullPointer);
            }
            else if(isValidString(object))
            {
                writer->addStringElement(writer, CLKSCrashField_Type, CLKSCrashMemType_String);
                writer->addStringElement(writer, CLKSCrashField_Value, (const char*)object);
            }
            else
            {
                writer->addStringElement(writer, CLKSCrashField_Type, CLKSCrashMemType_Unknown);
            }
        }
    }
    writer->endContainer(writer);
}

static bool isValidPointer(const uintptr_t address)
{
    if(address == (uintptr_t)NULL)
    {
        return false;
    }

#if CLKSCRASH_HAS_OBJC
    if(clksobjc_isTaggedPointer((const void*)address))
    {
        if(!clksobjc_isValidTaggedPointer((const void*)address))
        {
            return false;
        }
    }
#endif

    return true;
}

static bool isNotableAddress(const uintptr_t address)
{
    if(!isValidPointer(address))
    {
        return false;
    }
    
    const void* object = (const void*)address;

#if CLKSCRASH_HAS_OBJC
    if(clkszombie_className(object) != NULL)
    {
        return true;
    }

    if(clksobjc_objectType(object) != CLKSObjCTypeUnknown)
    {
        return true;
    }
#endif

    if(isValidString(object))
    {
        return true;
    }

    return false;
}

/** Write the contents of a memory location only if it contains notable data.
 * Also writes meta information about the data.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param address The memory address.
 */
static void writeMemoryContentsIfNotable(const CLKSCrashReportWriter* const writer,
                                         const char* const key,
                                         const uintptr_t address)
{
    if(isNotableAddress(address))
    {
        int limit = kDefaultMemorySearchDepth;
        writeMemoryContents(writer, key, address, &limit);
    }
}

/** Look for a hex value in a string and try to write whatever it references.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param string The string to search.
 */
static void writeAddressReferencedByString(const CLKSCrashReportWriter* const writer,
                                           const char* const key,
                                           const char* string)
{
    uint64_t address = 0;
    if(string == NULL || !clksstring_extractHexValue(string, (int)strlen(string), &address))
    {
        return;
    }
    
    int limit = kDefaultMemorySearchDepth;
    writeMemoryContents(writer, key, (uintptr_t)address, &limit);
}

#pragma mark Backtrace

/** Write a backtrace to the report.
 *
 * @param writer The writer to write the backtrace to.
 *
 * @param key The object key, if needed.
 *
 * @param stackCursor The stack cursor to read from.
 */
static void writeBacktrace(const CLKSCrashReportWriter* const writer,
                           const char* const key,
                           CLKSStackCursor* stackCursor)
{
    writer->beginObject(writer, key);
    {
        writer->beginArray(writer, CLKSCrashField_Contents);
        {
            while(stackCursor->advanceCursor(stackCursor))
            {
                writer->beginObject(writer, NULL);
                {
                    if(stackCursor->symbolicate(stackCursor))
                    {
                        if(stackCursor->stackEntry.imageName != NULL)
                        {
                            writer->addStringElement(writer, CLKSCrashField_ObjectName, clksfu_lastPathEntry(stackCursor->stackEntry.imageName));
                        }
                        writer->addUIntegerElement(writer, CLKSCrashField_ObjectAddr, stackCursor->stackEntry.imageAddress);
                        if(stackCursor->stackEntry.symbolName != NULL)
                        {
                            writer->addStringElement(writer, CLKSCrashField_SymbolName, stackCursor->stackEntry.symbolName);
                        }
                        writer->addUIntegerElement(writer, CLKSCrashField_SymbolAddr, stackCursor->stackEntry.symbolAddress);
                    }
                    writer->addUIntegerElement(writer, CLKSCrashField_InstructionAddr, stackCursor->stackEntry.address);
                }
                writer->endContainer(writer);
            }
        }
        writer->endContainer(writer);
        writer->addIntegerElement(writer, CLKSCrashField_Skipped, 0);
    }
    writer->endContainer(writer);
}
                              

#pragma mark Stack

/** Write a dump of the stack contents to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param machineContext The context to retrieve the stack from.
 *
 * @param isStackOverflow If true, the stack has overflowed.
 */
static void writeStackContents(const CLKSCrashReportWriter* const writer,
                               const char* const key,
                               const struct CLKSMachineContext* const machineContext,
                               const bool isStackOverflow)
{
    uintptr_t sp = clkscpu_stackPointer(machineContext);
    if((void*)sp == NULL)
    {
        return;
    }

    uintptr_t lowAddress = sp + (uintptr_t)(kStackContentsPushedDistance * (int)sizeof(sp) * clkscpu_stackGrowDirection() * -1);
    uintptr_t highAddress = sp + (uintptr_t)(kStackContentsPoppedDistance * (int)sizeof(sp) * clkscpu_stackGrowDirection());
    if(highAddress < lowAddress)
    {
        uintptr_t tmp = lowAddress;
        lowAddress = highAddress;
        highAddress = tmp;
    }
    writer->beginObject(writer, key);
    {
        writer->addStringElement(writer, CLKSCrashField_GrowDirection, clkscpu_stackGrowDirection() > 0 ? "+" : "-");
        writer->addUIntegerElement(writer, CLKSCrashField_DumpStart, lowAddress);
        writer->addUIntegerElement(writer, CLKSCrashField_DumpEnd, highAddress);
        writer->addUIntegerElement(writer, CLKSCrashField_StackPtr, sp);
        writer->addBooleanElement(writer, CLKSCrashField_Overflow, isStackOverflow);
        uint8_t stackBuffer[kStackContentsTotalDistance * sizeof(sp)];
        int copyLength = (int)(highAddress - lowAddress);
        if(clksmem_copySafely((void*)lowAddress, stackBuffer, copyLength))
        {
            writer->addDataElement(writer, CLKSCrashField_Contents, (void*)stackBuffer, copyLength);
        }
        else
        {
            writer->addStringElement(writer, CLKSCrashField_Error, "Stack contents not accessible");
        }
    }
    writer->endContainer(writer);
}

/** Write any notable addresses near the stack pointer (above and below).
 *
 * @param writer The writer.
 *
 * @param machineContext The context to retrieve the stack from.
 *
 * @param backDistance The distance towards the beginning of the stack to check.
 *
 * @param forwardDistance The distance past the end of the stack to check.
 */
static void writeNotableStackContents(const CLKSCrashReportWriter* const writer,
                                      const struct CLKSMachineContext* const machineContext,
                                      const int backDistance,
                                      const int forwardDistance)
{
    uintptr_t sp = clkscpu_stackPointer(machineContext);
    if((void*)sp == NULL)
    {
        return;
    }

    uintptr_t lowAddress = sp + (uintptr_t)(backDistance * (int)sizeof(sp) * clkscpu_stackGrowDirection() * -1);
    uintptr_t highAddress = sp + (uintptr_t)(forwardDistance * (int)sizeof(sp) * clkscpu_stackGrowDirection());
    if(highAddress < lowAddress)
    {
        uintptr_t tmp = lowAddress;
        lowAddress = highAddress;
        highAddress = tmp;
    }
    uintptr_t contentsAsPointer;
    char nameBuffer[40];
    for(uintptr_t address = lowAddress; address < highAddress; address += sizeof(address))
    {
        if(clksmem_copySafely((void*)address, &contentsAsPointer, sizeof(contentsAsPointer)))
        {
            sprintf(nameBuffer, "stack@%p", (void*)address);
            writeMemoryContentsIfNotable(writer, nameBuffer, contentsAsPointer);
        }
    }
}


#pragma mark Registers

/** Write the contents of all regular registers to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param machineContext The context to retrieve the registers from.
 */
static void writeBasicRegisters(const CLKSCrashReportWriter* const writer,
                                const char* const key,
                                const struct CLKSMachineContext* const machineContext)
{
    char registerNameBuff[30];
    const char* registerName;
    writer->beginObject(writer, key);
    {
        const int numRegisters = clkscpu_numRegisters();
        for(int reg = 0; reg < numRegisters; reg++)
        {
            registerName = clkscpu_registerName(reg);
            if(registerName == NULL)
            {
                snprintf(registerNameBuff, sizeof(registerNameBuff), "r%d", reg);
                registerName = registerNameBuff;
            }
            writer->addUIntegerElement(writer, registerName,
                                       clkscpu_registerValue(machineContext, reg));
        }
    }
    writer->endContainer(writer);
}

/** Write the contents of all exception registers to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param machineContext The context to retrieve the registers from.
 */
static void writeExceptionRegisters(const CLKSCrashReportWriter* const writer,
                                    const char* const key,
                                    const struct CLKSMachineContext* const machineContext)
{
    char registerNameBuff[30];
    const char* registerName;
    writer->beginObject(writer, key);
    {
        const int numRegisters = clkscpu_numExceptionRegisters();
        for(int reg = 0; reg < numRegisters; reg++)
        {
            registerName = clkscpu_exceptionRegisterName(reg);
            if(registerName == NULL)
            {
                snprintf(registerNameBuff, sizeof(registerNameBuff), "r%d", reg);
                registerName = registerNameBuff;
            }
            writer->addUIntegerElement(writer,registerName,
                                       clkscpu_exceptionRegisterValue(machineContext, reg));
        }
    }
    writer->endContainer(writer);
}

/** Write all applicable registers.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param machineContext The context to retrieve the registers from.
 */
static void writeRegisters(const CLKSCrashReportWriter* const writer,
                           const char* const key,
                           const struct CLKSMachineContext* const machineContext)
{
    writer->beginObject(writer, key);
    {
        writeBasicRegisters(writer, CLKSCrashField_Basic, machineContext);
        if(clksmc_hasValidExceptionRegisters(machineContext))
        {
            writeExceptionRegisters(writer, CLKSCrashField_Exception, machineContext);
        }
    }
    writer->endContainer(writer);
}

/** Write any notable addresses contained in the CPU registers.
 *
 * @param writer The writer.
 *
 * @param machineContext The context to retrieve the registers from.
 */
static void writeNotableRegisters(const CLKSCrashReportWriter* const writer,
                                  const struct CLKSMachineContext* const machineContext)
{
    char registerNameBuff[30];
    const char* registerName;
    const int numRegisters = clkscpu_numRegisters();
    for(int reg = 0; reg < numRegisters; reg++)
    {
        registerName = clkscpu_registerName(reg);
        if(registerName == NULL)
        {
            snprintf(registerNameBuff, sizeof(registerNameBuff), "r%d", reg);
            registerName = registerNameBuff;
        }
        writeMemoryContentsIfNotable(writer,
                                     registerName,
                                     (uintptr_t)clkscpu_registerValue(machineContext, reg));
    }
}

#pragma mark Thread-specific

/** Write any notable addresses in the stack or registers to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param machineContext The context to retrieve the registers from.
 */
static void writeNotableAddresses(const CLKSCrashReportWriter* const writer,
                                  const char* const key,
                                  const struct CLKSMachineContext* const machineContext)
{
    writer->beginObject(writer, key);
    {
        writeNotableRegisters(writer, machineContext);
        writeNotableStackContents(writer,
                                  machineContext,
                                  kStackNotableSearchBackDistance,
                                  kStackNotableSearchForwardDistance);
    }
    writer->endContainer(writer);
}

/** Write information about a thread to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param crash The crash handler context.
 *
 * @param machineContext The context whose thread to write about.
 *
 * @param shouldWriteNotableAddresses If true, write any notable addresses found.
 */
static void writeThread(const CLKSCrashReportWriter* const writer,
                        const char* const key,
                        const CLKSCrash_MonitorContext* const crash,
                        const struct CLKSMachineContext* const machineContext,
                        const int threadIndex,
                        const bool shouldWriteNotableAddresses)
{
    bool isCrashedThread = clksmc_isCrashedContext(machineContext);
    CLKSThread thread = clksmc_getThreadFromContext(machineContext);
    CLKSLOG_DEBUG("Writing thread %x (index %d). is crashed: %d", thread, threadIndex, isCrashedThread);

    CLKSStackCursor stackCursor;
    bool hasBacktrace = getStackCursor(crash, machineContext, &stackCursor);

    writer->beginObject(writer, key);
    {
        if(hasBacktrace)
        {
            writeBacktrace(writer, CLKSCrashField_Backtrace, &stackCursor);
        }
        if(clksmc_canHaveCPUState(machineContext))
        {
            writeRegisters(writer, CLKSCrashField_Registers, machineContext);
        }
        writer->addIntegerElement(writer, CLKSCrashField_Index, threadIndex);
        const char* name = clksccd_getThreadName(thread);
        if(name != NULL)
        {
            writer->addStringElement(writer, CLKSCrashField_Name, name);
        }
        name = clksccd_getQueueName(thread);
        if(name != NULL)
        {
            writer->addStringElement(writer, CLKSCrashField_DispatchQueue, name);
        }
        writer->addBooleanElement(writer, CLKSCrashField_Crashed, isCrashedThread);
        writer->addBooleanElement(writer, CLKSCrashField_CurrentThread, thread == clksthread_self());
        if(isCrashedThread)
        {
            writeStackContents(writer, CLKSCrashField_Stack, machineContext, stackCursor.state.hasGivenUp);
            if(shouldWriteNotableAddresses)
            {
                writeNotableAddresses(writer, CLKSCrashField_NotableAddresses, machineContext);
            }
        }
    }
    writer->endContainer(writer);
}

/** Write information about all threads to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param crash The crash handler context.
 */
static void writeAllThreads(const CLKSCrashReportWriter* const writer,
                            const char* const key,
                            const CLKSCrash_MonitorContext* const crash,
                            bool writeNotableAddresses)
{
    const struct CLKSMachineContext* const context = crash->offendingMachineContext;
    CLKSThread offendingThread = clksmc_getThreadFromContext(context);
    int threadCount = clksmc_getThreadCount(context);
    CLKSMC_NEW_CONTEXT(machineContext);

    // Fetch info for all threads.
    writer->beginArray(writer, key);
    {
        CLKSLOG_DEBUG("Writing %d threads.", threadCount);
        for(int i = 0; i < threadCount; i++)
        {
            CLKSThread thread = clksmc_getThreadAtIndex(context, i);
            if(thread == offendingThread)
            {
                writeThread(writer, NULL, crash, context, i, writeNotableAddresses);
            }
            else
            {
                clksmc_getContextForThread(thread, machineContext, false);
                writeThread(writer, NULL, crash, machineContext, i, writeNotableAddresses);
            }
        }
    }
    writer->endContainer(writer);
}

#pragma mark Global Report Data

/** Write information about a binary image to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param index Which image to write about.
 */
static void writeBinaryImage(const CLKSCrashReportWriter* const writer,
                             const char* const key,
                             const int index)
{
    CLKSBinaryImage image = {0};
    if(!clksdl_getBinaryImage(index, &image))
    {
        return;
    }

    writer->beginObject(writer, key);
    {
        writer->addUIntegerElement(writer, CLKSCrashField_ImageAddress, image.address);
        writer->addUIntegerElement(writer, CLKSCrashField_ImageVmAddress, image.vmAddress);
        writer->addUIntegerElement(writer, CLKSCrashField_ImageSize, image.size);
        writer->addStringElement(writer, CLKSCrashField_Name, image.name);
        writer->addUUIDElement(writer, CLKSCrashField_UUID, image.uuid);
        writer->addIntegerElement(writer, CLKSCrashField_CPUType, image.cpuType);
        writer->addIntegerElement(writer, CLKSCrashField_CPUSubType, image.cpuSubType);
        writer->addUIntegerElement(writer, CLKSCrashField_ImageMajorVersion, image.majorVersion);
        writer->addUIntegerElement(writer, CLKSCrashField_ImageMinorVersion, image.minorVersion);
        writer->addUIntegerElement(writer, CLKSCrashField_ImageRevisionVersion, image.revisionVersion);
    }
    writer->endContainer(writer);
}

/** Write information about all images to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 */
static void writeBinaryImages(const CLKSCrashReportWriter* const writer, const char* const key)
{
    const int imageCount = clksdl_imageCount();

    writer->beginArray(writer, key);
    {
        for(int iImg = 0; iImg < imageCount; iImg++)
        {
            writeBinaryImage(writer, NULL, iImg);
        }
    }
    writer->endContainer(writer);
}

/** Write information about system memory to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 */
static void writeMemoryInfo(const CLKSCrashReportWriter* const writer,
                            const char* const key,
                            const CLKSCrash_MonitorContext* const monitorContext)
{
    writer->beginObject(writer, key);
    {
        writer->addUIntegerElement(writer, CLKSCrashField_Size, monitorContext->System.memorySize);
        writer->addUIntegerElement(writer, CLKSCrashField_Usable, monitorContext->System.usableMemory);
        writer->addUIntegerElement(writer, CLKSCrashField_Free, monitorContext->System.freeMemory);
    }
    writer->endContainer(writer);
}

/** Write information about the error leading to the crash to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param crash The crash handler context.
 */
static void writeError(const CLKSCrashReportWriter* const writer,
                       const char* const key,
                       const CLKSCrash_MonitorContext* const crash)
{
    writer->beginObject(writer, key);
    {
#if CLKSCRASH_HOST_APPLE
        writer->beginObject(writer, CLKSCrashField_Mach);
        {
            const char* machExceptionName = clksmach_exceptionName(crash->mach.type);
            const char* machCodeName = crash->mach.code == 0 ? NULL : clksmach_kernelReturnCodeName(crash->mach.code);
            writer->addUIntegerElement(writer, CLKSCrashField_Exception, (unsigned)crash->mach.type);
            if(machExceptionName != NULL)
            {
                writer->addStringElement(writer, CLKSCrashField_ExceptionName, machExceptionName);
            }
            writer->addUIntegerElement(writer, CLKSCrashField_Code, (unsigned)crash->mach.code);
            if(machCodeName != NULL)
            {
                writer->addStringElement(writer, CLKSCrashField_CodeName, machCodeName);
            }
            writer->addUIntegerElement(writer, CLKSCrashField_Subcode, (unsigned)crash->mach.subcode);
        }
        writer->endContainer(writer);
#endif
        writer->beginObject(writer, CLKSCrashField_Signal);
        {
            const char* sigName = clkssignal_signalName(crash->signal.signum);
            const char* sigCodeName = clkssignal_signalCodeName(crash->signal.signum, crash->signal.sigcode);
            writer->addUIntegerElement(writer, CLKSCrashField_Signal, (unsigned)crash->signal.signum);
            if(sigName != NULL)
            {
                writer->addStringElement(writer, CLKSCrashField_Name, sigName);
            }
            writer->addUIntegerElement(writer, CLKSCrashField_Code, (unsigned)crash->signal.sigcode);
            if(sigCodeName != NULL)
            {
                writer->addStringElement(writer, CLKSCrashField_CodeName, sigCodeName);
            }
        }
        writer->endContainer(writer);

        writer->addUIntegerElement(writer, CLKSCrashField_Address, crash->faultAddress);
        if(crash->crashReason != NULL)
        {
            writer->addStringElement(writer, CLKSCrashField_Reason, crash->crashReason);
        }

        // Gather specific info.
        switch(crash->crashType)
        {
            case CLKSCrashMonitorTypeMainThreadDeadlock:
                writer->addStringElement(writer, CLKSCrashField_Type, CLKSCrashExcType_Deadlock);
                break;
                
            case CLKSCrashMonitorTypeMachException:
                writer->addStringElement(writer, CLKSCrashField_Type, CLKSCrashExcType_Mach);
                break;

            case CLKSCrashMonitorTypeCPPException:
            {
                writer->addStringElement(writer, CLKSCrashField_Type, CLKSCrashExcType_CPPException);
                writer->beginObject(writer, CLKSCrashField_CPPException);
                {
                    writer->addStringElement(writer, CLKSCrashField_Name, crash->CPPException.name);
                }
                writer->endContainer(writer);
                break;
            }
            case CLKSCrashMonitorTypeNSException:
            {
                writer->addStringElement(writer, CLKSCrashField_Type, CLKSCrashExcType_NSException);
                writer->beginObject(writer, CLKSCrashField_NSException);
                {
                    writer->addStringElement(writer, CLKSCrashField_Name, crash->NSException.name);
                    writer->addStringElement(writer, CLKSCrashField_UserInfo, crash->NSException.userInfo);
                    writeAddressReferencedByString(writer, CLKSCrashField_ReferencedObject, crash->crashReason);
                }
                writer->endContainer(writer);
                break;
            }
            case CLKSCrashMonitorTypeSignal:
                writer->addStringElement(writer, CLKSCrashField_Type, CLKSCrashExcType_Signal);
                break;

            case CLKSCrashMonitorTypeUserReported:
            {
                writer->addStringElement(writer, CLKSCrashField_Type, CLKSCrashExcType_User);
                writer->beginObject(writer, CLKSCrashField_UserReported);
                {
                    writer->addStringElement(writer, CLKSCrashField_Name, crash->userException.name);
                    if(crash->userException.language != NULL)
                    {
                        writer->addStringElement(writer, CLKSCrashField_Language, crash->userException.language);
                    }
                    if(crash->userException.lineOfCode != NULL)
                    {
                        writer->addStringElement(writer, CLKSCrashField_LineOfCode, crash->userException.lineOfCode);
                    }
                    if(crash->userException.customStackTrace != NULL)
                    {
                        writer->addJSONElement(writer, CLKSCrashField_Backtrace, crash->userException.customStackTrace, true);
                    }
                }
                writer->endContainer(writer);
                break;
            }
            case CLKSCrashMonitorTypeSystem:
            case CLKSCrashMonitorTypeApplicationState:
            case CLKSCrashMonitorTypeZombie:
                CLKSLOG_ERROR("Crash monitor type 0x%x shouldn't be able to cause events!", crash->crashType);
                break;
        }
    }
    writer->endContainer(writer);
}

/** Write information about app runtime, etc to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param monitorContext The event monitor context.
 */
static void writeAppStats(const CLKSCrashReportWriter* const writer,
                          const char* const key,
                          const CLKSCrash_MonitorContext* const monitorContext)
{
    writer->beginObject(writer, key);
    {
        writer->addBooleanElement(writer, CLKSCrashField_AppActive, monitorContext->AppState.applicationIsActive);
        writer->addBooleanElement(writer, CLKSCrashField_AppInFG, monitorContext->AppState.applicationIsInForeground);

        writer->addIntegerElement(writer, CLKSCrashField_LaunchesSinceCrash, monitorContext->AppState.launchesSinceLastCrash);
        writer->addIntegerElement(writer, CLKSCrashField_SessionsSinceCrash, monitorContext->AppState.sessionsSinceLastCrash);
        writer->addFloatingPointElement(writer, CLKSCrashField_ActiveTimeSinceCrash, monitorContext->AppState.activeDurationSinceLastCrash);
        writer->addFloatingPointElement(writer, CLKSCrashField_BGTimeSinceCrash, monitorContext->AppState.backgroundDurationSinceLastCrash);

        writer->addIntegerElement(writer, CLKSCrashField_SessionsSinceLaunch, monitorContext->AppState.sessionsSinceLaunch);
        writer->addFloatingPointElement(writer, CLKSCrashField_ActiveTimeSinceLaunch, monitorContext->AppState.activeDurationSinceLaunch);
        writer->addFloatingPointElement(writer, CLKSCrashField_BGTimeSinceLaunch, monitorContext->AppState.backgroundDurationSinceLaunch);
    }
    writer->endContainer(writer);
}

/** Write information about this process.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 */
static void writeProcessState(const CLKSCrashReportWriter* const writer,
                              const char* const key,
                              const CLKSCrash_MonitorContext* const monitorContext)
{
    writer->beginObject(writer, key);
    {
        if(monitorContext->ZombieException.address != 0)
        {
            writer->beginObject(writer, CLKSCrashField_LastDeallocedNSException);
            {
                writer->addUIntegerElement(writer, CLKSCrashField_Address, monitorContext->ZombieException.address);
                writer->addStringElement(writer, CLKSCrashField_Name, monitorContext->ZombieException.name);
                writer->addStringElement(writer, CLKSCrashField_Reason, monitorContext->ZombieException.reason);
                writeAddressReferencedByString(writer, CLKSCrashField_ReferencedObject, monitorContext->ZombieException.reason);
            }
            writer->endContainer(writer);
        }
    }
    writer->endContainer(writer);
}

/** Write basic report information.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param type The report type.
 *
 * @param reportID The report ID.
 */
static void writeReportInfo(const CLKSCrashReportWriter* const writer,
                            const char* const key,
                            const char* const type,
                            const char* const reportID,
                            const char* const processName)
{
    writer->beginObject(writer, key);
    {
        writer->addStringElement(writer, CLKSCrashField_Version, CLKSCRASH_REPORT_VERSION);
        writer->addStringElement(writer, CLKSCrashField_ID, reportID);
        writer->addStringElement(writer, CLKSCrashField_ProcessName, processName);
        writer->addIntegerElement(writer, CLKSCrashField_Timestamp, time(NULL));
        writer->addStringElement(writer, CLKSCrashField_Type, type);
    }
    writer->endContainer(writer);
}

static void writeRecrash(const CLKSCrashReportWriter* const writer,
                         const char* const key,
                         const char* crashReportPath)
{
    writer->addJSONFileElement(writer, key, crashReportPath, true);
}


#pragma mark Setup

/** Prepare a report writer for use.
 *
 * @oaram writer The writer to prepare.
 *
 * @param context JSON writer contextual information.
 */
static void prepareReportWriter(CLKSCrashReportWriter* const writer, CLKSJSONEncodeContext* const context)
{
    writer->addBooleanElement = addBooleanElement;
    writer->addFloatingPointElement = addFloatingPointElement;
    writer->addIntegerElement = addIntegerElement;
    writer->addUIntegerElement = addUIntegerElement;
    writer->addStringElement = addStringElement;
    writer->addTextFileElement = addTextFileElement;
    writer->addTextFileLinesElement = addTextLinesFromFile;
    writer->addJSONFileElement = addJSONElementFromFile;
    writer->addDataElement = addDataElement;
    writer->beginDataElement = beginDataElement;
    writer->appendDataElement = appendDataElement;
    writer->endDataElement = endDataElement;
    writer->addUUIDElement = addUUIDElement;
    writer->addJSONElement = addJSONElement;
    writer->beginObject = beginObject;
    writer->beginArray = beginArray;
    writer->endContainer = endContainer;
    writer->context = context;
}


// ============================================================================
#pragma mark - Main API -
// ============================================================================

void clkscrashreport_writeRecrashReport(const CLKSCrash_MonitorContext *const monitorContext, const char *const path)
{
    char writeBuffer[1024];
    CLKSBufferedWriter bufferedWriter;
    static char tempPath[CLKSFU_MAX_PATH_LENGTH];
    strncpy(tempPath, path, sizeof(tempPath) - 10);
    strncpy(tempPath + strlen(tempPath) - 5, ".old", 5);
    CLKSLOG_INFO("Writing recrash report to %s", path);

    if(rename(path, tempPath) < 0)
    {
        CLKSLOG_ERROR("Could not rename %s to %s: %s", path, tempPath, strerror(errno));
    }
    if(!clksfu_openBufferedWriter(&bufferedWriter, path, writeBuffer, sizeof(writeBuffer)))
    {
        return;
    }

    clksccd_freeze();

    CLKSJSONEncodeContext jsonContext;
    jsonContext.userData = &bufferedWriter;
    CLKSCrashReportWriter concreteWriter;
    CLKSCrashReportWriter* writer = &concreteWriter;
    prepareReportWriter(writer, &jsonContext);

    clksjson_beginEncode(getJsonContext(writer), true, addJSONData, &bufferedWriter);

    writer->beginObject(writer, CLKSCrashField_Report);
    {
        writeRecrash(writer, CLKSCrashField_RecrashReport, tempPath);
        clksfu_flushBufferedWriter(&bufferedWriter);
        if(remove(tempPath) < 0)
        {
            CLKSLOG_ERROR("Could not remove %s: %s", tempPath, strerror(errno));
        }
        writeReportInfo(writer,
                        CLKSCrashField_Report,
                        CLKSCrashReportType_Minimal,
                        monitorContext->eventID,
                        monitorContext->System.processName);
        clksfu_flushBufferedWriter(&bufferedWriter);

        writer->beginObject(writer, CLKSCrashField_Crash);
        {
            writeError(writer, CLKSCrashField_Error, monitorContext);
            clksfu_flushBufferedWriter(&bufferedWriter);
            int threadIndex = clksmc_indexOfThread(monitorContext->offendingMachineContext,
                                                 clksmc_getThreadFromContext(monitorContext->offendingMachineContext));
            writeThread(writer,
                        CLKSCrashField_CrashedThread,
                        monitorContext,
                        monitorContext->offendingMachineContext,
                        threadIndex,
                        false);
            clksfu_flushBufferedWriter(&bufferedWriter);
        }
        writer->endContainer(writer);
    }
    writer->endContainer(writer);

    clksjson_endEncode(getJsonContext(writer));
    clksfu_closeBufferedWriter(&bufferedWriter);
    clksccd_unfreeze();
}

static void writeSystemInfo(const CLKSCrashReportWriter* const writer,
                            const char* const key,
                            const CLKSCrash_MonitorContext* const monitorContext)
{
    writer->beginObject(writer, key);
    {
        writer->addStringElement(writer, CLKSCrashField_SystemName, monitorContext->System.systemName);
        writer->addStringElement(writer, CLKSCrashField_SystemVersion, monitorContext->System.systemVersion);
        writer->addStringElement(writer, CLKSCrashField_Machine, monitorContext->System.machine);
        writer->addStringElement(writer, CLKSCrashField_Model, monitorContext->System.model);
        writer->addStringElement(writer, CLKSCrashField_KernelVersion, monitorContext->System.kernelVersion);
        writer->addStringElement(writer, CLKSCrashField_OSVersion, monitorContext->System.osVersion);
        writer->addBooleanElement(writer, CLKSCrashField_Jailbroken, monitorContext->System.isJailbroken);
        writer->addStringElement(writer, CLKSCrashField_BootTime, monitorContext->System.bootTime);
        writer->addStringElement(writer, CLKSCrashField_AppStartTime, monitorContext->System.appStartTime);
        writer->addStringElement(writer, CLKSCrashField_ExecutablePath, monitorContext->System.executablePath);
        writer->addStringElement(writer, CLKSCrashField_Executable, monitorContext->System.executableName);
        writer->addStringElement(writer, CLKSCrashField_BundleID, monitorContext->System.bundleID);
        writer->addStringElement(writer, CLKSCrashField_BundleName, monitorContext->System.bundleName);
        writer->addStringElement(writer, CLKSCrashField_BundleVersion, monitorContext->System.bundleVersion);
        writer->addStringElement(writer, CLKSCrashField_BundleShortVersion, monitorContext->System.bundleShortVersion);
        writer->addStringElement(writer, CLKSCrashField_AppUUID, monitorContext->System.appID);
        writer->addStringElement(writer, CLKSCrashField_CPUArch, monitorContext->System.cpuArchitecture);
        writer->addIntegerElement(writer, CLKSCrashField_CPUType, monitorContext->System.cpuType);
        writer->addIntegerElement(writer, CLKSCrashField_CPUSubType, monitorContext->System.cpuSubType);
        writer->addIntegerElement(writer, CLKSCrashField_BinaryCPUType, monitorContext->System.binaryCPUType);
        writer->addIntegerElement(writer, CLKSCrashField_BinaryCPUSubType, monitorContext->System.binaryCPUSubType);
        writer->addStringElement(writer, CLKSCrashField_TimeZone, monitorContext->System.timezone);
        writer->addStringElement(writer, CLKSCrashField_ProcessName, monitorContext->System.processName);
        writer->addIntegerElement(writer, CLKSCrashField_ProcessID, monitorContext->System.processID);
        writer->addIntegerElement(writer, CLKSCrashField_ParentProcessID, monitorContext->System.parentProcessID);
        writer->addStringElement(writer, CLKSCrashField_DeviceAppHash, monitorContext->System.deviceAppHash);
        writer->addStringElement(writer, CLKSCrashField_BuildType, monitorContext->System.buildType);
        writer->addIntegerElement(writer, CLKSCrashField_Storage, (int64_t)monitorContext->System.storageSize);

        writeMemoryInfo(writer, CLKSCrashField_Memory, monitorContext);
        writeAppStats(writer, CLKSCrashField_AppStats, monitorContext);
    }
    writer->endContainer(writer);

}

static void writeDebugInfo(const CLKSCrashReportWriter* const writer,
                            const char* const key,
                            const CLKSCrash_MonitorContext* const monitorContext)
{
    writer->beginObject(writer, key);
    {
        if(monitorContext->consoleLogPath != NULL)
        {
            addTextLinesFromFile(writer, CLKSCrashField_ConsoleLog, monitorContext->consoleLogPath);
        }
    }
    writer->endContainer(writer);
    
}

void clkscrashreport_writeStandardReport(const CLKSCrash_MonitorContext *const monitorContext, const char *const path)
{
    CLKSLOG_INFO("Writing crash report to %s", path);
    char writeBuffer[1024];
    CLKSBufferedWriter bufferedWriter;

    if(!clksfu_openBufferedWriter(&bufferedWriter, path, writeBuffer, sizeof(writeBuffer)))
    {
        return;
    }

    clksccd_freeze();
    
    CLKSJSONEncodeContext jsonContext;
    jsonContext.userData = &bufferedWriter;
    CLKSCrashReportWriter concreteWriter;
    CLKSCrashReportWriter* writer = &concreteWriter;
    prepareReportWriter(writer, &jsonContext);

    clksjson_beginEncode(getJsonContext(writer), true, addJSONData, &bufferedWriter);

    writer->beginObject(writer, CLKSCrashField_Report);
    {
        writeReportInfo(writer,
                        CLKSCrashField_Report,
                        CLKSCrashReportType_Standard,
                        monitorContext->eventID,
                        monitorContext->System.processName);
        clksfu_flushBufferedWriter(&bufferedWriter);

        writeBinaryImages(writer, CLKSCrashField_BinaryImages);
        clksfu_flushBufferedWriter(&bufferedWriter);

        writeProcessState(writer, CLKSCrashField_ProcessState, monitorContext);
        clksfu_flushBufferedWriter(&bufferedWriter);

        writeSystemInfo(writer, CLKSCrashField_System, monitorContext);
        clksfu_flushBufferedWriter(&bufferedWriter);

        writer->beginObject(writer, CLKSCrashField_Crash);
        {
            writeError(writer, CLKSCrashField_Error, monitorContext);
            clksfu_flushBufferedWriter(&bufferedWriter);
            writeAllThreads(writer,
                            CLKSCrashField_Threads,
                            monitorContext,
                            g_introspectionRules.enabled);
            clksfu_flushBufferedWriter(&bufferedWriter);
        }
        writer->endContainer(writer);

        if(g_userInfoJSON != NULL)
        {
            addJSONElement(writer, CLKSCrashField_User, g_userInfoJSON, false);
            clksfu_flushBufferedWriter(&bufferedWriter);
        }
        else
        {
            writer->beginObject(writer, CLKSCrashField_User);
        }
        if(g_userSectionWriteCallback != NULL)
        {
            clksfu_flushBufferedWriter(&bufferedWriter);
            if (monitorContext->currentSnapshotUserReported == false) {
                g_userSectionWriteCallback(writer);
            }
        }
        writer->endContainer(writer);
        clksfu_flushBufferedWriter(&bufferedWriter);

        writeDebugInfo(writer, CLKSCrashField_Debug, monitorContext);
    }
    writer->endContainer(writer);
    
    clksjson_endEncode(getJsonContext(writer));
    clksfu_closeBufferedWriter(&bufferedWriter);
    clksccd_unfreeze();
}



void clkscrashreport_setUserInfoJSON(const char* const userInfoJSON)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    CLKSLOG_TRACE("set userInfoJSON to %p", userInfoJSON);

    pthread_mutex_lock(&mutex);
    if(g_userInfoJSON != NULL)
    {
        free((void*)g_userInfoJSON);
    }
    if(userInfoJSON == NULL)
    {
        g_userInfoJSON = NULL;
    }
    else
    {
        g_userInfoJSON = strdup(userInfoJSON);
    }
    pthread_mutex_unlock(&mutex);
}

void clkscrashreport_setIntrospectMemory(bool shouldIntrospectMemory)
{
    g_introspectionRules.enabled = shouldIntrospectMemory;
}

void clkscrashreport_setDoNotIntrospectClasses(const char** doNotIntrospectClasses, int length)
{
    const char** oldClasses = g_introspectionRules.restrictedClasses;
    int oldClassesLength = g_introspectionRules.restrictedClassesCount;
    const char** newClasses = NULL;
    int newClassesLength = 0;
    
    if(doNotIntrospectClasses != NULL && length > 0)
    {
        newClassesLength = length;
        newClasses = malloc(sizeof(*newClasses) * (unsigned)newClassesLength);
        if(newClasses == NULL)
        {
            CLKSLOG_ERROR("Could not allocate memory");
            return;
        }
        
        for(int i = 0; i < newClassesLength; i++)
        {
            newClasses[i] = strdup(doNotIntrospectClasses[i]);
        }
    }
    
    g_introspectionRules.restrictedClasses = newClasses;
    g_introspectionRules.restrictedClassesCount = newClassesLength;
    
    if(oldClasses != NULL)
    {
        for(int i = 0; i < oldClassesLength; i++)
        {
            free((void*)oldClasses[i]);
        }
        free(oldClasses);
    }
}

void clkscrashreport_setUserSectionWriteCallback(const CLKSReportWriteCallback userSectionWriteCallback)
{
    CLKSLOG_TRACE("Set userSectionWriteCallback to %p", userSectionWriteCallback);
    g_userSectionWriteCallback = userSectionWriteCallback;
}
