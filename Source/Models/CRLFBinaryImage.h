//
//  CRLFBinaryImage.h
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/7/19.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CRLFBinaryImage : NSObject
- (instancetype)initWithKSDictionry:(NSDictionary *)dictionary;
@property (nonatomic, readonly) NSUInteger majorVersion;
@property (nonatomic, readonly) NSUInteger minorVersion;
@property (nonatomic, readonly) NSUInteger revisionVersion;
@property (nonatomic, readonly) NSUInteger cpuSubtype;
@property (nonatomic, readonly) NSUUID *uuid;
@property (nonatomic, readonly) NSUInteger imageVMAddr;
@property (nonatomic, readonly) NSUInteger imageAddr;
@property (nonatomic, readonly) NSUInteger imageSize;
@property (nonatomic, readonly) NSString *name;
@property (nonatomic, readonly) NSUInteger cpuType;
@end

NS_ASSUME_NONNULL_END
