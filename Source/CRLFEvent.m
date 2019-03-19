//
// Created by Daniel DeCovnick on 2019-02-06.
// Copyright (c) 2019 Buglife, Inc. All rights reserved.
//

#import "CRLFEvent.h"
#import "NSMutableDictionary+CRLFAdditions.h"
#import "CRLFAttribute.h"
#import "CLKSCrashReportFields.h"
#import "CRLFCrashReport.h"
#import "CRLFFootprint.h"
#import "CLKSSymbolicator.h"
#import "CLKSStackCursor_Backtrace.h"
#import "CRLFStackFrame.h"
#import "CRLFThread.h"


CRLFEventSeverity const CRLFEventSeverityInfo = @"info";
CRLFEventSeverity const CRLFEventSeverityWarning = @"warning";
CRLFEventSeverity const CRLFEventSeverityError = @"error";
CRLFEventSeverity const CRLFEventSeverityCrash = @"crash";

@interface CRLFEvent ()
@property (nonatomic, nullable) CRLFCrashReport *crashReport;
@property (nonatomic, nullable, copy) NSString *message;
@property (nonatomic, nonnull) NSUUID *uuid;
@property (nonatomic, nullable) NSException *caughtException;
@end

@implementation CRLFEvent
+ (instancetype)eventWithCrashReport:(CRLFCrashReport *)crashReport {
    CRLFEvent *event = [[self alloc] init];
    event.crashReport = crashReport;
    event.severity = CRLFEventSeverityCrash;
    event.uuid = [[NSUUID alloc] initWithUUIDString:crashReport.uuidString];
    event.attributes = [NSMutableDictionary dictionary];
    event.footprints = [NSMutableArray array];
    event.timestampString = crashReport.occurredAtString;
    return event;
}

- (instancetype)initWithSeverity:(CRLFEventSeverity)severity message:(NSString *)message attributes:(NSDictionary<NSString *, CRLFAttribute *> *)attributes footprints:(NSArray *)footprints {
    self = [super init];
    if (self != nil) {
        _severity = severity;
        _message = [message copy];
        _attributes = [attributes mutableCopy];
        _footprints = [footprints mutableCopy];
        _uuid = NSUUID.UUID;
        _timestampString = [[CRLFEvent dateFormatter] stringFromDate:[NSDate date]];
    }
    return self;
}

- (instancetype)initWithException:(NSException *)exception attributes:(NSDictionary<NSString *, CRLFAttribute *> *)attributes footprints:(NSArray *)footprints {
    self = [super init];
    if (self != nil) {
        _severity = CRLFEventSeverityError;
        _message = exception.description;
        _attributes = attributes.mutableCopy;
        _footprints = footprints.mutableCopy;
        _uuid = NSUUID.UUID;
        _caughtException = exception;
        _timestampString = [[CRLFEvent dateFormatter] stringFromDate:[NSDate date]];
    }
    return self;
}

- (NSMutableDictionary *)occurrenceDict {
    
    NSMutableDictionary *ret = [NSMutableDictionary dictionary];
    [ret crlf_safeSetObject:self.severity forKey:@"severity"];
    [ret crlf_safeSetObject:self.message forKey:@"message"];
    [ret crlf_safeSetObject:self.uuid.UUIDString forKey:@"uuid"];
    [ret crlf_safeSetObject:self.timestampString forKey:@"occurred_at"];
    [ret crlf_safeSetObject:self.crashReport.denormalizedThreads forKey:@"threads"];
    [ret crlf_safeSetObject:self.crashReport.denormalizedException forKey:@"exceptions"];
    if (self.crashReport == nil && self.caughtException != nil) {
        NSArray<NSNumber *> *stackTraceArray = self.caughtException.callStackReturnAddresses;
        CLKSStackCursor stackCursor;
        uintptr_t *addresses = calloc(stackTraceArray.count, sizeof(uintptr_t));
        for (int i = 0; i < stackTraceArray.count; i++) {
            addresses[i] = (uintptr_t)stackTraceArray[i].unsignedLongLongValue;
        }
        clkssc_initWithBacktrace(&stackCursor, addresses, (int)stackTraceArray.count, 0);
        NSMutableArray<CRLFStackFrame *> *stackFrames = [NSMutableArray array];
        while (stackCursor.advanceCursor(&stackCursor)) {
            if (stackCursor.symbolicate(&stackCursor)) {
                NSString *symbolName = [NSString stringWithUTF8String:stackCursor.stackEntry.symbolName];
                NSString *objectName = [NSString stringWithUTF8String:stackCursor.stackEntry.imageName];
                CRLFStackFrame *stackFrame = [[CRLFStackFrame alloc] initWithSymbolName:symbolName symbolAddr:stackCursor.stackEntry.symbolAddress instructionAddr:stackCursor.stackEntry.address objectName:objectName objectAddr:stackCursor.stackEntry.imageAddress];
                [stackFrames addObject:stackFrame];
            }
        }
        CRLFThread *fakeThread = [[CRLFThread alloc] initWithBacktrace:stackFrames];
        [ret crlf_safeSetObject:fakeThread.denormalizedThreadInCurrentProcess forKey:@"threads"];
        [ret crlf_safeSetObject:@[fakeThread.denormalizedThreadInCurrentProcess] forKey:@"exceptions"];
    }
    [ret crlf_safeSetObject:[CRLFAttribute JSONAttributesArrayFromAttributes:self.attributes] forKey:@"attributes"];
    [ret crlf_safeSetObject:[self.footprints valueForKey:NSStringFromSelector(@selector(JSONDictionary))] forKey:@"footprints"];
    return ret;
}

+ (NSDateFormatter *)dateFormatter
{
    static dispatch_once_t onceToken;
    static NSDateFormatter *iso8601DateFormatter = nil;
    dispatch_once(&onceToken, ^{
        iso8601DateFormatter = [[NSDateFormatter alloc] init];
        [iso8601DateFormatter setDateFormat:@"yyyy-MM-dd'T'HH:mm:ss.SSSZZZZ"];
        [iso8601DateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
    });
    return iso8601DateFormatter;
}

@end
