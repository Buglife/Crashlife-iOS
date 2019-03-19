//
//  CRLFMacros.h
//  Copyright (C) 2019 Buglife, Inc.
//  
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//  
//       http://www.apache.org/licenses/LICENSE-2.0
//  
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
//

#ifndef CRLFMacros_h
#define CRLFMacros_h

#define CRLF_THROW_UNAVAILABLE_EXCEPTION(useselector) @throw [NSException exceptionWithName:NSInternalInconsistencyException reason:[NSString stringWithFormat:@"[%@ %@] is unavailable; please use [%@ %@]", NSStringFromClass([self class]), NSStringFromSelector(_cmd), NSStringFromClass([self class]), NSStringFromSelector(@selector(useselector))] userInfo:nil]

#define CRLFAssertMainThread NSParameterAssert([NSThread isMainThread])
#define CRLFAssertIsKindOfClass(obj, clazz) NSParameterAssert([obj isKindOfClass:[clazz class]])

#define CRLFLogExtDebug(fmt, ...) NSLog(fmt, ##__VA_ARGS__)
#define CRLFLogExtInfo(fmt, ...) NSLog(fmt, ##__VA_ARGS__)

// This should *always* log to conosle
#define CRLFLogExtWarn(fmt, ...) NSLog(fmt, ##__VA_ARGS__)
#define CRLFLogExtError(fmt, ...) NSLog(fmt, ##__VA_ARGS__)

//////////////////////////////////////////////////
// Helper macros for model object serialization //
//////////////////////////////////////////////////

#define CRLF_DECODE_OBJECT_FOR_KEY(key) self.key = [coder decodeObjectForKey:NSStringFromSelector(@selector(key))]
#define CRLF_ENCODE_OBJECT_FOR_KEY(key) [coder encodeObject:self.key forKey:NSStringFromSelector(@selector(key))]
#define CRLF_STRING_FROM_SELECTOR_NAMED(selector_name) NSStringFromSelector(@selector(selector_name))

//////////////////////////
// Swiftier Objective-C //
//////////////////////////

#if defined(__cplusplus)
#define let auto const
#else
#define let const __auto_type
#endif

#if defined(__cplusplus)
#define var auto
#else
#define var __auto_type
#endif

//////////////////////////////////////////////////
// Demo mode stuff                              //
//////////////////////////////////////////////////

#define CRLF_DEMO_MODE false

#if CRLF_DEMO_MODE
#warning YOU ARE IN DEMO MODE
#warning DO NOT SHIP THIS
#warning SERIOUSLY DON'T SHIP THIS
#endif

#endif /* CRLFMacros_h */
