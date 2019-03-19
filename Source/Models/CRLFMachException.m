//
//  CRLFMachException.m
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/7/19.
//

#import "CRLFMachException.h"
#import "CLKSCrashReportFields.h"

@interface CRLFMachException ()
@property (nonatomic, assign) NSUInteger code;
@property (nonatomic, assign) NSUInteger subcode;
@property (nonatomic, assign) NSUInteger exception;
@property (nonatomic, copy) NSString *exceptionName;
@end

@implementation CRLFMachException
- (instancetype)initWithKSDictionary:(NSDictionary *)dictionary {
    self = [super init];
    if (self != nil) {
        _code          = ((NSNumber *)dictionary[@CLKSCrashField_Code]).unsignedIntegerValue;
        _subcode       = ((NSNumber *)dictionary[@CLKSCrashField_Subcode]).unsignedIntegerValue;
        _exception     = ((NSNumber *)dictionary[@CLKSCrashField_Exception]).unsignedIntegerValue;
        _exceptionName = dictionary[@CLKSCrashField_ExceptionName];
    }
    return self;
}
@end
