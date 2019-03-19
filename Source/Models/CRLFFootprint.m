//
//  CRLFFootprint.m
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/12/19.
//

#import "CRLFFootprint.h"

#import "CRLFAttribute.h"

@implementation CRLFFootprint
+ (NSDateFormatter *)dateFormatter { // Thread-safe since iOS 7 woohoo!
    static NSDateFormatter *dateFormatter = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSDateFormatter *iso8601DateFormatter = [[NSDateFormatter alloc] init];
        [iso8601DateFormatter setDateFormat:@"yyyy-MM-dd'T'HH:mm:ssZZZZ"];
        [iso8601DateFormatter setLocale:[NSLocale localeWithLocaleIdentifier:@"en_US"]];
        dateFormatter = iso8601DateFormatter;
    });
    return dateFormatter;
}
- (instancetype)initWithName:(NSString *)name attributes:(NSDictionary *)attributes {
    self = [super init];
    if (self != nil) {
        _name = [name copy];
        _attributes = [attributes copy];
        _date = [NSDate date];
    }
    return self;
}
- (instancetype)initWithName:(NSString *)name {
    NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
    return [self initWithName:name attributes:dict];
}

- (NSDictionary *)JSONDictionary {
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    dict[@"name"] = self.name;
    dict[@"left_at"] = [[CRLFFootprint dateFormatter] stringFromDate:self.date];
    dict[@"metadata"] = [CRLFAttribute JSONAttributesArrayFromAttributes:self.attributes];
    return [NSDictionary dictionaryWithDictionary:dict];
}
+ (instancetype)fromJSONDictionary:(NSDictionary *)jsonDict {
    CRLFFootprint *footprint = [[CRLFFootprint alloc] init];
    footprint.name = jsonDict[@"name"];
    NSString *dateString = jsonDict[@"left_at"];
    footprint.date = [[CRLFFootprint dateFormatter] dateFromString:dateString];
    footprint.attributes = [CRLFAttribute mutableAttributesFromJSONArray:jsonDict[@"metadata"]];
    return footprint;
}
@end
