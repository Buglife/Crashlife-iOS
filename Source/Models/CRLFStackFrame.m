//
//  CRLFStackFrame.m
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/7/19.
//

#import "CRLFStackFrame.h"
#import "CLKSCrashReportFields.h"

@interface CRLFStackFrame ()
@property (nonatomic, copy) NSString *symbolName;
@property (nonatomic, assign) NSUInteger symbolAddr;
@property (nonatomic, assign) NSUInteger instructionAddr;
@property (nonatomic, copy) NSString *objectName;
@property (nonatomic, assign) NSUInteger objectAddr;
@end

@implementation CRLFStackFrame
- (instancetype)initWithKSDictionary:(NSDictionary *)dictionary {
    self = [super init];
    if (self != nil) {
        _symbolName      = dictionary[@CLKSCrashField_SymbolName];
        _symbolAddr      = ((NSNumber *)dictionary[@CLKSCrashField_SymbolAddr]).unsignedIntegerValue;
        _instructionAddr = ((NSNumber *)dictionary[@CLKSCrashField_InstructionAddr]).unsignedIntegerValue;
        _objectName      = dictionary[@CLKSCrashField_ObjectName];
        _objectAddr      = ((NSNumber *)dictionary[@CLKSCrashField_ObjectAddr]).unsignedIntegerValue;
    }
    return self;
}
- (instancetype)initWithSymbolName:(NSString *)symbolName symbolAddr:(NSUInteger)symbolAddr instructionAddr:(NSUInteger)instructionAddr objectName:(NSString *)objectName objectAddr:(NSUInteger)objectAddr {
    self = [super init];
    if (self != nil) {
        _symbolName = symbolName;
        _symbolAddr = symbolAddr;
        _instructionAddr = instructionAddr;
        _objectName = objectName;
        _objectAddr = objectAddr;
    }
    return self;
}

@end
