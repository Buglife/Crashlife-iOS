//
//  CRLFCrashError.m
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/7/19.
//

#import "CRLFCrashError.h"
#import "CRLFMachException.h"
#import "CRLFSignalException.h"
#import "CLKSCrashReportFields.h"

@interface CRLFCrashError ()
@property (nonatomic, readwrite) CRLFMachException *mach;
@property (nonatomic, readwrite) CRLFSignalException *signal;
@property (nonatomic, assign) NSUInteger address;
@property (nonatomic, copy) NSString *type;
@end

@implementation CRLFCrashError
- (instancetype)initWithKSDictionary:(NSDictionary *)dictionary {
    self = [super init];
    if (self != nil) {
        _address = ((NSNumber *)dictionary[@CLKSCrashField_Address]).unsignedIntegerValue;
        _type = dictionary[@CLKSCrashField_Type];
        _mach = [[CRLFMachException alloc] initWithKSDictionary:dictionary[@CLKSCrashField_Mach]];
        _signal = [[CRLFSignalException alloc] initWithKSDictionary:dictionary[@CLKSCrashField_Signal]];
    }
    return self;
}

@end
