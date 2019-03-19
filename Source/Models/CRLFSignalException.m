//
//  CRLFSignalException.m
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/7/19.
//

#import "CRLFSignalException.h"
#import "CLKSCrashReportFields.h"

@interface CRLFSignalException ()
@property (nonatomic, assign) NSUInteger code;
@property (nonatomic, assign) NSUInteger signal;
@property (nonatomic, copy) NSString *name;

@end

@implementation CRLFSignalException
- (instancetype)initWithKSDictionary:(NSDictionary *)dictionary {
    self = [super init];
    if (self != nil) {
        _code   = ((NSNumber *)dictionary[@CLKSCrashField_Code]).unsignedIntegerValue;
        _signal = ((NSNumber *)dictionary[@CLKSCrashField_Signal]).unsignedIntegerValue;
        _name   = dictionary[@CLKSCrashField_Name];
    }
    return self;
}

@end
