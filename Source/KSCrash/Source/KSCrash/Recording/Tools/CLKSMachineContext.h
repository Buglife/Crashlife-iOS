//
//  CLKSMachineContext.h
//
//  Created by Karl Stenerud on 2016-12-02.
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


#ifndef HDR_CLKSMachineContext_h
#define HDR_CLKSMachineContext_h

#ifdef __cplusplus
extern "C" {
#endif

#include "CLKSThread.h"
#include <stdbool.h>

/** Suspend the runtime environment.
 */
void clksmc_suspendEnvironment(void);

/** Resume the runtime environment.
 */
void clksmc_resumeEnvironment(void);

/** Create a new machine context on the stack.
 * This macro creates a storage object on the stack, as well as a pointer of type
 * struct CLKSMachineContext* in the current scope, which points to the storage object.
 *
 * Example usage: CLKSMC_NEW_CONTEXT(a_context);
 * This creates a new pointer at the current scope that behaves as if:
 *     struct CLKSMachineContext* a_context = some_storage_location;
 *
 * @param NAME The C identifier to give the pointer.
 */
#define CLKSMC_NEW_CONTEXT(NAME) \
    char clksmc_##NAME##_storage[clksmc_contextSize()]; \
    struct CLKSMachineContext* NAME = (struct CLKSMachineContext*)clksmc_##NAME##_storage

struct CLKSMachineContext;

/** Get the internal size of a machine context.
 */
int clksmc_contextSize(void);

/** Fill in a machine context from a thread.
 *
 * @param thread The thread to get information from.
 * @param destinationContext The context to fill.
 * @param isCrashedContext Used to indicate that this is the thread that crashed,
 *
 * @return true if successful.
 */
bool clksmc_getContextForThread(CLKSThread thread, struct CLKSMachineContext *destinationContext, bool isCrashedContext);

/** Fill in a machine context from a signal handler.
 * A signal handler context is always assumed to be a crashed context.
 *
 * @param signalUserContext The signal context to get information from.
 * @param destinationContext The context to fill.
 *
 * @return true if successful.
 */
bool clksmc_getContextForSignal(void *signalUserContext, struct CLKSMachineContext *destinationContext);

/** Get the thread associated with a machine context.
 *
 * @param context The machine context.
 *
 * @return The associated thread.
 */
CLKSThread clksmc_getThreadFromContext(const struct CLKSMachineContext *const context);

/** Get the number of threads stored in a machine context.
 *
 * @param context The machine context.
 *
 * @return The number of threads.
 */
int clksmc_getThreadCount(const struct CLKSMachineContext *const context);

/** Get a thread from a machine context.
 *
 * @param context The machine context.
 * @param index The index of the thread to retrieve.
 *
 * @return The thread.
 */
CLKSThread clksmc_getThreadAtIndex(const struct CLKSMachineContext *const context, int index);

/** Get the index of a thread.
 *
 * @param context The machine context.
 * @param thread The thread.
 *
 * @return The thread's index, or -1 if it couldn't be determined.
 */
int clksmc_indexOfThread(const struct CLKSMachineContext *const context, CLKSThread thread);

/** Check if this is a crashed context.
 */
bool clksmc_isCrashedContext(const struct CLKSMachineContext *const context);

/** Check if this context can have stored CPU state.
 */
bool clksmc_canHaveCPUState(const struct CLKSMachineContext *const context);

/** Check if this context has valid exception registers.
 */
bool clksmc_hasValidExceptionRegisters(const struct CLKSMachineContext *const context);

/** Add a thread to the reserved threads list.
 *
 * @param thread The thread to add to the list.
 */
void clksmc_addReservedThread(CLKSThread thread);


#ifdef __cplusplus
}
#endif

#endif // HDR_CLKSMachineContext_h
