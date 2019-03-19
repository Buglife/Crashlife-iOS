//
//  CRLFCrashRepot.m
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

#import "CRLFCrashReport.h"
#import "CLKSCrashReportFields.h"
#import "CRLFBinaryImage.h"
#import "CRLFThread.h"
#import "CRLFStackFrame.h"
#import "NSMutableDictionary+CRLFAdditions.h"
#import "CRLFCrashError.h"
#import "CRLFMacros.h"

@interface CRLFCrashReport ()
@property (nonatomic) NSDictionary<NSUUID *, CRLFBinaryImage *> *binariesByUUID;
@property (nonatomic) NSDictionary<NSNumber *, CRLFBinaryImage *> *binariesByStartAddress;
@property (nonatomic) NSArray<NSNumber *> *orderedStartAddresses;
@property (nonatomic) NSDictionary *rawKSCrashDict;
@property (nonatomic) NSDictionary *crashDict;
@property (nonatomic) NSArray<CRLFThread *> *threads;
@property (nonatomic) CRLFCrashError *crashError;
@property (nonatomic, copy) NSString *uuidString;
@property (nonatomic, copy) NSString *occurredAtString;
- (CRLFBinaryImage *)binaryImageAtAddress:(NSInteger)address;
@end

@implementation CRLFCrashReport
- (instancetype)initWithKSCrashReport:(NSDictionary *)ksCrashReport {
    self = [super init];
    if (self != nil) {
        _binariesByUUID = [NSMutableDictionary dictionary];
        _rawKSCrashDict = ksCrashReport;
        _crashDict = ksCrashReport[@CLKSCrashField_Crash];
        _crashError = [[CRLFCrashError alloc] initWithKSDictionary:_crashDict[@CLKSCrashField_Error]];
        NSArray *ksThreads = _crashDict[@CLKSCrashField_Threads];
        NSMutableArray *threads = [NSMutableArray array];
        for (NSDictionary *thread in ksThreads) {
            [threads addObject:[[CRLFThread alloc] initWithKSDictionary:thread]];
        }
        _threads = [NSArray arrayWithArray:threads];
        NSDictionary *ksBinaries = _rawKSCrashDict[@CLKSCrashField_BinaryImages];
        NSMutableDictionary *binaries = [NSMutableDictionary dictionary];
        NSMutableDictionary *binariesByStartAddr = [NSMutableDictionary dictionary];
        for (NSDictionary *binary in ksBinaries) {
            CRLFBinaryImage *binaryImage = [[CRLFBinaryImage alloc] initWithKSDictionry:binary];
            binaries[binaryImage.uuid] = binaryImage;
            binariesByStartAddr[@(binaryImage.imageAddr)] = binaryImage;
        }
        _binariesByUUID = [NSDictionary dictionaryWithDictionary:binaries];
        _binariesByStartAddress = [NSDictionary dictionaryWithDictionary:binariesByStartAddr];
        _orderedStartAddresses = [_binariesByStartAddress.allKeys sortedArrayUsingSelector:@selector(compare:)];
        NSDictionary *reportDict = ksCrashReport[@CLKSCrashField_Report];
        _uuidString = reportDict[@"id"];
        _occurredAtString = reportDict[@"timestamp"]; //TODO: make sure this timestamp is readable on the backend
        
    }
    return self;
}

- (CRLFBinaryImage *)binaryImageAtAddress:(NSInteger)address {
    //simple linear search should be fine.
    NSNumber *addressNumber = @(address);
    NSUInteger count = self.orderedStartAddresses.count;
    for (NSUInteger i = 0; i < count; i++) {
        NSComparisonResult result = [self.orderedStartAddresses[i] compare:addressNumber];
        
        // find the first start address that's larger than the target address, and then take the previous one
        if (result == NSOrderedDescending) {
            return self.binariesByStartAddress[self.orderedStartAddresses[i-1]];
        }
    }
    return self.binariesByStartAddress[self.orderedStartAddresses.lastObject];
}

// We could push the inner loop on this method down to CRLFThread, but then we'd have to pass around the binary images dict
//
- (NSArray *)denormalizedThreads {
    NSMutableArray *outputArray = [NSMutableArray array];
    for (CRLFThread *thread in self.threads) {
        NSMutableDictionary *threadOutput = [NSMutableDictionary dictionary];
        [threadOutput crlf_safeSetObject:[self denormalizeThread:thread] forKey:@"stack_frames"];
        [threadOutput crlf_safeSetObject:@(thread.index).description forKey:@"thread_id"];
        [threadOutput crlf_safeSetObject:thread.dispatchQueue forKey:@"name"];
        [outputArray addObject:threadOutput];
    }
    return [NSArray arrayWithArray:outputArray];
}

- (NSArray *)denormalizeThread:(CRLFThread *)thread {
    NSMutableArray *backTraceOuput = [NSMutableArray array];
    for (CRLFStackFrame *stackFrame in thread.backtrace) {
        NSMutableDictionary *stackFrameOutput = [NSMutableDictionary dictionary];
        [stackFrameOutput crlf_safeSetObject:@(stackFrame.symbolAddr).description forKey:@"symbol_address"];
        [stackFrameOutput crlf_safeSetObject:stackFrame.symbolName forKey:@"method_name"];
        CRLFBinaryImage *binaryImage = [self binaryImageAtAddress:stackFrame.symbolAddr];
        [stackFrameOutput crlf_safeSetObject:binaryImage.name forKey:@"mach_o_file"];
        [stackFrameOutput crlf_safeSetObject:binaryImage.uuid.UUIDString forKey:@"mach_o_uuid"]; // I think this is right, here.
        [stackFrameOutput crlf_safeSetObject:@(binaryImage.imageVMAddr).description forKey:@"mach_o_vm_address"];
        [stackFrameOutput crlf_safeSetObject:@(binaryImage.imageAddr).description forKey:@"mach_o_load_address"];
        [stackFrameOutput crlf_safeSetObject:@(stackFrame.instructionAddr).description forKey:@"address"];
        [backTraceOuput addObject:stackFrameOutput];
    }
    return [NSArray arrayWithArray:backTraceOuput];
}

- (NSArray *)denormalizedException {
    NSMutableDictionary *exception = [NSMutableDictionary dictionary]; // there's only one on iOS
    for (CRLFThread *thread in self.threads) {
        if (!thread.crashed) {
            continue;
        }
        NSArray *backtrace = [self denormalizeThread:thread];
        [exception crlf_safeSetObject:backtrace forKey:@"stack_frames"];
        NSDictionary *error;
        if ([self.crashError.type isEqualToString:@CLKSCrashExcType_Mach]) {
            error = self.crashDict[@CLKSCrashField_Mach];
            [exception crlf_safeSetObject:error[@CLKSCrashField_ExceptionName] forKey:@"name"];
            //TODO: other Fields
        }
        else if ([self.crashError.type isEqualToString:@CLKSCrashExcType_Signal]) {
            error = self.crashDict[@CLKSCrashField_Signal];
            [exception crlf_safeSetObject:error[@CLKSCrashField_Name] forKey:@"name"];
            //TODO: other fields?
        }
        else if ([self.crashError.type isEqualToString:@CLKSCrashExcType_CPPException]) {
            error = self.crashDict[@CLKSCrashField_CPPException];
            [exception crlf_safeSetObject:error[@CLKSCrashField_Name] forKey:@"name"];
            //no other fields
        }
        else if ([self.crashError.type isEqualToString:@CLKSCrashField_NSException]) {
            error = self.crashDict[@CLKSCrashField_NSException];
            [exception crlf_safeSetObject:error[@CLKSCrashField_Name] forKey:@"name"];
            //todo: userinfo?, crash reason?
        }
        else if ([self.crashError.type isEqualToString:@CLKSCrashExcType_User]) {
            error = self.crashDict[@CLKSCrashField_User];
            [exception crlf_safeSetObject:error[@CLKSCrashField_Name] forKey:@"name"];
            //TODO: other fields, including? embedded stack trace if applicable?
        }
        else if ([self.crashError.type isEqualToString:@CLKSCrashExcType_Deadlock]) {
            [exception crlf_safeSetObject:@"deadlock" forKey:@"name"]; // there doesn't seem to be a struct for deadlocks
        }
        else {
            CRLFLogExtError(@"Crashlife encountered an unknown crash type: \"%@\". Please alert support@buglife.com.", self.crashError.type);
        }
        [exception crlf_safeSetObject:self.crashDict[@CLKSCrashField_Reason] forKey:@"message"];
        //TODO should this be a separate field?
    }
    return @[[NSDictionary dictionaryWithDictionary:exception]];
}
@end
