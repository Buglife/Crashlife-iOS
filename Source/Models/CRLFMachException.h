//
//  CRLFMachException.h
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/7/19.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CRLFMachException : NSObject
- (instancetype)initWithKSDictionary:(NSDictionary *)dictionary;
@property (nonatomic, readonly) NSUInteger code;
@property (nonatomic, readonly) NSUInteger subcode;
@property (nonatomic, readonly) NSUInteger exception;
@property (nonatomic, readonly) NSString *exceptionName;
@end

NS_ASSUME_NONNULL_END
