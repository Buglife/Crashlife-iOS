//
//  CRLFThread.m
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/7/19.
//

#import "CRLFThread.h"
#import "CRLFStackFrame.h"
#import "CLKSCrashReportFields.h"
#import "NSMutableDictionary+CRLFAdditions.h"
#import "CLKSDynamicLinker.h"

@interface CRLFThread ()
@property (nonatomic, assign) NSUInteger index;
@property (nonatomic, assign) BOOL crashed;
@property (nonatomic, assign) BOOL backtraceSkipped;
@property (nonatomic, copy) NSArray<CRLFStackFrame *> *backtrace;
@property (nonatomic, assign) BOOL currentThread;
@property (nonatomic, nullable, copy) NSString *dispatchQueue;
@end

@implementation CRLFThread
- (instancetype)initWithKSDictionary:(NSDictionary *)dictionary {
    self = [super init];
    if (self != nil) {
        _index   = ((NSNumber *)dictionary[@CLKSCrashField_Index]).unsignedIntegerValue;
        _crashed = ((NSNumber *)dictionary[@CLKSCrashField_Crashed]).boolValue;
        NSArray<NSDictionary *> *backtrace = dictionary[@CLKSCrashField_Backtrace][@CLKSCrashField_Contents];
        NSMutableArray<CRLFStackFrame *> *stackFrames = [NSMutableArray array];
        for (NSDictionary *stackFrame in backtrace) {
            [stackFrames addObject:[[CRLFStackFrame alloc] initWithKSDictionary:stackFrame]];
        }
        _backtrace        = [NSArray arrayWithArray:stackFrames];
        _backtraceSkipped = ((NSNumber *)dictionary[@CLKSCrashField_Backtrace][@CLKSCrashField_Skipped]).boolValue;
        _currentThread    = ((NSNumber *)dictionary[@CLKSCrashField_CurrentThread]).boolValue;
        _dispatchQueue    = dictionary[@CLKSCrashField_DispatchQueue];
    }
    return self;
}
- (instancetype)initWithBacktrace:(NSArray<CRLFStackFrame *> *)backtrace {
    self = [super init];
    if (self != nil) {
        _backtrace = backtrace;
        _dispatchQueue = nil;
    }
    return self;
}

- (NSArray *)denormalizedThreadInCurrentProcess {
    NSMutableArray *backTraceOuput = [NSMutableArray array];
    for (CRLFStackFrame *stackFrame in self.backtrace) {
        NSMutableDictionary *stackFrameOutput = [NSMutableDictionary dictionary];
        [stackFrameOutput crlf_safeSetObject:@(stackFrame.symbolAddr).description forKey:@"symbol_address"];
        [stackFrameOutput crlf_safeSetObject:stackFrame.symbolName forKey:@"method_name"];
        
        Dl_info info;
        clksdl_dladdr((uintptr_t)stackFrame.symbolAddr, &info);
        [stackFrameOutput crlf_safeSetObject:[NSString stringWithUTF8String:info.dli_fname] forKey:@"mach_o_file"];
        const uint8_t *imageUUID = clksdl_imageUUID(info.dli_fname, false);
        NSUUID *uuid = [[NSUUID alloc] initWithUUIDBytes:imageUUID];
        [stackFrameOutput crlf_safeSetObject:uuid.UUIDString forKey:@"mach_o_uuid"];
        
        CLKSBinaryImage binaryImage;
        uint32_t imageHandle = clksdl_imageNamed(info.dli_fname, false);
        clksdl_getBinaryImage(imageHandle, &binaryImage);
        [stackFrameOutput crlf_safeSetObject:@(binaryImage.vmAddress).description forKey:@"mach_o_vm_address"];
        [stackFrameOutput crlf_safeSetObject:@(binaryImage.address).description forKey:@"mach_o_load_address"];
        [stackFrameOutput crlf_safeSetObject:@(stackFrame.instructionAddr).description forKey:@"address"];
        [backTraceOuput addObject:stackFrameOutput];
    }
    return [NSArray arrayWithArray:backTraceOuput];
}
@end
