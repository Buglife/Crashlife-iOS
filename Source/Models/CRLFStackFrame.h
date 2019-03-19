//
//  CRLFStackFrame.h
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/7/19.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CRLFStackFrame : NSObject
- (instancetype)initWithKSDictionary:(NSDictionary *)dictionary;
- (instancetype)initWithSymbolName:(NSString *)symbolName symbolAddr:(NSUInteger)symbolAddr instructionAddr:(NSUInteger)instructionAddr objectName:(NSString *)objectName objectAddr:(NSUInteger)objectAddr;
@property (nonatomic, readonly) NSString *symbolName;
@property (nonatomic, readonly) NSUInteger symbolAddr;
@property (nonatomic, readonly) NSUInteger instructionAddr;
@property (nonatomic, readonly) NSString *objectName;
@property (nonatomic, readonly) NSUInteger objectAddr;
@end

NS_ASSUME_NONNULL_END
