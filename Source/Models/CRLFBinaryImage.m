//
//  CRLFBinaryImage.m
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/7/19.
//

#import "CRLFBinaryImage.h"
#import "CLKSCrashReportFields.h"

@interface CRLFBinaryImage ()
@property (nonatomic, assign) NSUInteger majorVersion;
@property (nonatomic, assign) NSUInteger minorVersion;
@property (nonatomic, assign) NSUInteger revisionVersion;
@property (nonatomic, assign) NSUInteger cpuSubtype;
@property (nonatomic, readwrite) NSUUID *uuid;
@property (nonatomic, assign) NSUInteger imageVMAddr;
@property (nonatomic, assign) NSUInteger imageAddr;
@property (nonatomic, assign) NSUInteger imageSize;
@property (nonatomic, copy) NSString *name;
@property (nonatomic, assign) NSUInteger cpuType;
- (instancetype)initWithKSDictionry:(NSDictionary *)dictionary;
@end


@implementation CRLFBinaryImage

- (instancetype)initWithKSDictionry:(NSDictionary *)dictionary {
    self = [super init];
    if (self != nil) {
        _majorVersion    = ((NSNumber *)dictionary[@CLKSCrashField_ImageMajorVersion]).unsignedIntegerValue;
        _minorVersion    = ((NSNumber *)dictionary[@CLKSCrashField_ImageMinorVersion]).unsignedIntegerValue;
        _revisionVersion = ((NSNumber *)dictionary[@CLKSCrashField_ImageRevisionVersion]).unsignedIntegerValue;
        _cpuSubtype      = ((NSNumber *)dictionary[@CLKSCrashField_CPUSubType]).unsignedIntegerValue;
        _uuid            = [[NSUUID alloc] initWithUUIDString:dictionary[@CLKSCrashField_UUID]];
        _imageVMAddr     = ((NSNumber *)dictionary[@CLKSCrashField_ImageVmAddress]).unsignedIntegerValue;
        _imageAddr       = ((NSNumber *)dictionary[@CLKSCrashField_ImageAddress]).unsignedIntegerValue;
        _name            = dictionary[@CLKSCrashField_Name];
        _cpuType         = ((NSNumber *)dictionary[@CLKSCrashField_CPUType]).unsignedIntegerValue;
    }
    return self;
}
@end
