//
//  CLKSCrashMonitorType.h
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


#ifndef HDR_CLKSCrashMonitorType_h
#define HDR_CLKSCrashMonitorType_h

#ifdef __cplusplus
extern "C" {
#endif


/** Various aspects of the system that can be monitored:
 * - Mach kernel exception
 * - Fatal signal
 * - Uncaught C++ exception
 * - Uncaught Objective-C NSException
 * - Deadlock on the main thread
 * - User reported custom exception
 */
typedef enum
{
    /* Captures and reports Mach exceptions. */
    CLKSCrashMonitorTypeMachException      = 0x01,
    
    /* Captures and reports POSIX signals. */
    CLKSCrashMonitorTypeSignal             = 0x02,
    
    /* Captures and reports C++ exceptions.
     * Note: This will slightly slow down exception processing.
     */
    CLKSCrashMonitorTypeCPPException       = 0x04,
    
    /* Captures and reports NSExceptions. */
    CLKSCrashMonitorTypeNSException        = 0x08,
    
    /* Detects and reports a deadlock in the main thread. */
    CLKSCrashMonitorTypeMainThreadDeadlock = 0x10,
    
    /* Accepts and reports user-generated exceptions. */
    CLKSCrashMonitorTypeUserReported       = 0x20,
    
    /* Keeps track of and injects system information. */
    CLKSCrashMonitorTypeSystem             = 0x40,
    
    /* Keeps track of and injects application state. */
    CLKSCrashMonitorTypeApplicationState   = 0x80,
    
    /* Keeps track of zombies, and injects the last zombie NSException. */
    CLKSCrashMonitorTypeZombie             = 0x100,
} CLKSCrashMonitorType;

#define CLKSCrashMonitorTypeAll              \
(                                          \
    CLKSCrashMonitorTypeMachException      | \
    CLKSCrashMonitorTypeSignal             | \
    CLKSCrashMonitorTypeCPPException       | \
    CLKSCrashMonitorTypeNSException        | \
    CLKSCrashMonitorTypeMainThreadDeadlock | \
    CLKSCrashMonitorTypeUserReported       | \
    CLKSCrashMonitorTypeSystem             | \
    CLKSCrashMonitorTypeApplicationState   | \
    CLKSCrashMonitorTypeZombie               \
)

#define CLKSCrashMonitorTypeExperimental     \
(                                          \
    CLKSCrashMonitorTypeMainThreadDeadlock   \
)

#define CLKSCrashMonitorTypeDebuggerUnsafe   \
(                                          \
    CLKSCrashMonitorTypeMachException      | \
    CLKSCrashMonitorTypeSignal             | \
    CLKSCrashMonitorTypeCPPException       | \
    CLKSCrashMonitorTypeNSException          \
)

#define CLKSCrashMonitorTypeAsyncSafe        \
(                                          \
    CLKSCrashMonitorTypeMachException      | \
    CLKSCrashMonitorTypeSignal               \
)

#define CLKSCrashMonitorTypeOptional         \
(                                          \
    CLKSCrashMonitorTypeZombie               \
)
    
#define CLKSCrashMonitorTypeAsyncUnsafe (CLKSCrashMonitorTypeAll & (~CLKSCrashMonitorTypeAsyncSafe))

/** Monitors that are safe to enable in a debugger. */
#define CLKSCrashMonitorTypeDebuggerSafe (CLKSCrashMonitorTypeAll & (~CLKSCrashMonitorTypeDebuggerUnsafe))

/** Monitors that are safe to use in a production environment.
 * All other monitors should be considered experimental.
 */
#define CLKSCrashMonitorTypeProductionSafe (CLKSCrashMonitorTypeAll & (~CLKSCrashMonitorTypeExperimental))

/** Production safe monitors, minus the optional ones. */
#define CLKSCrashMonitorTypeProductionSafeMinimal (CLKSCrashMonitorTypeProductionSafe & (~CLKSCrashMonitorTypeOptional))

/** Monitors that are required for proper operation.
 * These add essential information to the reports, but do not trigger reporting.
 */
#define CLKSCrashMonitorTypeRequired (CLKSCrashMonitorTypeSystem | CLKSCrashMonitorTypeApplicationState)

/** Effectively disables automatica reporting. The only way to generate a report
 * in this mode is by manually calling clkscrash_reportUserException().
 */
#define CLKSCrashMonitorTypeManual (CLKSCrashMonitorTypeRequired | CLKSCrashMonitorTypeUserReported)

#define CLKSCrashMonitorTypeNone 0

const char* clkscrashmonitortype_name(CLKSCrashMonitorType monitorType);


#ifdef __cplusplus
}
#endif

#endif // HDR_CSKSCrashMonitorType_h
