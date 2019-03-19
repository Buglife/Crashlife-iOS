//
//  CRLFFootprint.h
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/12/19.
//

#import <Foundation/Foundation.h>

@class CRLFAttribute;

NS_ASSUME_NONNULL_BEGIN

@interface CRLFFootprint : NSObject
@property (nonatomic, copy) NSString *name;
@property (nonatomic) NSDate *date;
@property (nonatomic) NSMutableDictionary<NSString *, CRLFAttribute *> *attributes;
- (instancetype)initWithName:(NSString *)name;
- (instancetype)initWithName:(NSString *)name attributes:(NSDictionary *)attributes;
- (NSDictionary *)JSONDictionary;
+ (instancetype)fromJSONDictionary:(NSDictionary *)jsonDict;
@end

NS_ASSUME_NONNULL_END
