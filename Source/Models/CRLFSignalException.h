//
//  CRLFSignalException.h
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/7/19.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CRLFSignalException : NSObject
- (instancetype)initWithKSDictionary:(NSDictionary *)dictionary;
@property (nonatomic, readonly) NSUInteger code;
@property (nonatomic, readonly) NSUInteger signal;
@property (nonatomic, readonly) NSString *name;
@end

NS_ASSUME_NONNULL_END
