//
//  CRLFCrashError.h
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/7/19.
//

#import <Foundation/Foundation.h>

@class CRLFMachException;
@class CRLFSignalException;

NS_ASSUME_NONNULL_BEGIN

@interface CRLFCrashError : NSObject
- (instancetype)initWithKSDictionary:(NSDictionary *)dictionary;
@property (nonatomic, readonly) CRLFMachException *mach;
@property (nonatomic, readonly) CRLFSignalException *signal;
@property (nonatomic, readonly) NSUInteger address;
@property (nonatomic, readonly) NSString *type;
@end

NS_ASSUME_NONNULL_END
