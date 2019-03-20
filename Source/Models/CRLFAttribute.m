//
//  CRLFAttribute.m
//  Copyright (C) 2019 Buglife, Inc.
//  
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//  
//       http://www.apache.org/licenses/LICENSE-2.0
//  
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
//

#import "CRLFAttribute.h"
#import "NSMutableDictionary+CRLFAdditions.h"

@interface CRLFAttribute ()

@property (nonatomic) CRLFAttributeValueType valueType;
@property (nonatomic) NSObject *value;
@property (nonatomic) CRLFAttributeFlags flags;

@end

static NSString *CRLFAttributeNameForAttributeFlag(CRLFAttributeFlags flag) {
    switch (flag) {
        case CRLFAttributeFlagCustom:
            return nil;
            break;
        case CRLFAttributeFlagSystem:
            return @"system";
        case CRLFAttributeFlagPublic:
            return @"public";
        case CRLFAttributeFlagInternal:
            return @"internal";
        default:
            return [NSString stringWithFormat: @"%lu", (unsigned long)flag];
    }
}
static CRLFAttributeFlags CRLFAttributeFlagForName(NSString * name) {
    if (name == nil) {
        return CRLFAttributeFlagCustom;
    }
    if ([name isEqualToString:@"system"]) {
        return CRLFAttributeFlagSystem;
    }
    if ([name isEqualToString:@"public"]) {
        return CRLFAttributeFlagPublic;
    }
    if ([name isEqualToString:@"internal"]) {
        return CRLFAttributeFlagInternal;
    }
    return CRLFAttributeFlagCustom;
}

@implementation CRLFAttribute

- (instancetype)initWithValueType:(CRLFAttributeValueType)valueType value:(NSObject *)value flags:(CRLFAttributeFlags)flags
{
    self = [super init];
    if (self) {
        _valueType = valueType;
        _value = value;
        _flags = flags;
    }
    return self;
}

+ (instancetype)attributeWithBool:(BOOL)value flags:(CRLFAttributeFlags)flags
{
    NSNumber *boxedValue = [NSNumber numberWithBool:value];
    return [[self alloc] initWithValueType:CRLFAttributeValueTypeBool value:boxedValue flags:flags];
}

+ (instancetype)attributeWithString:(NSString *)stringValue flags:(CRLFAttributeFlags)flags
{
    return [[self alloc] initWithValueType:CRLFAttributeValueTypeString value:stringValue flags:flags];
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super init];
    if (self) {
        _valueType = [coder decodeIntegerForKey:NSStringFromSelector(@selector(valueType))];
        _value = [coder decodeObjectForKey:NSStringFromSelector(@selector(value))];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeInteger:_valueType forKey:NSStringFromSelector(@selector(valueType))];
    [coder encodeObject:_value forKey:NSStringFromSelector(@selector(value))];
}

#pragma mark - Public methods

- (NSString *)stringValue
{
    if (_valueType == CRLFAttributeValueTypeString) {
        return (NSString *)_value;
    } else {
        return [_value description];
    }
}

#pragma mark - JSON serialization

- (NSDictionary *)JSONDictionary
{
    NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
    
//    [dict crlf_safeSetObject:@(_valueType) forKey:@"attribute_type"];
    [dict crlf_safeSetObject:_value forKey:@"value"];
    [dict crlf_safeSetObject:CRLFAttributeNameForAttributeFlag(_flags) forKey:@"flag"];
    
    return [NSDictionary dictionaryWithDictionary:dict];
}

+ (NSDictionary *)JSONDictionaryForAttributesDictionary:(NSMutableDictionary<NSString *, CRLFAttribute *> *)attributes {
    NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
    for (NSString *key in attributes) {
        dict[key] = attributes[key].JSONDictionary;
    }
    return [NSDictionary dictionaryWithDictionary:dict];
}
+ (CRLFMutableAttributes *)mutableSystemAttributesFromDictionary:(NSDictionary *)dictionary {
    CRLFMutableAttributes *mutableAttributes = [[NSMutableDictionary alloc] init];
    for (NSString *key in dictionary) {
        CRLFAttribute *attribute = [[CRLFAttribute alloc] initWithValueType:CRLFAttributeValueTypeString value:((NSObject *)dictionary[key]).description flags:CRLFAttributeFlagSystem];
        [mutableAttributes crlf_safeSetObject:attribute forKey:key];
    }
    return mutableAttributes;
}
// TODO: refactor ^/V into a single method with a Flags param
+ (CRLFMutableAttributes *)mutableAttributesFromJSONDictionary:(NSDictionary *)dictionary {
    CRLFMutableAttributes *mutableAttributes = [[NSMutableDictionary alloc] init];
    for (NSString *key in dictionary) {
        CRLFAttribute *attribute = [[CRLFAttribute alloc] initWithValueType:CRLFAttributeValueTypeString value:((NSObject *)dictionary[key][@"value"]).description flags:CRLFAttributeFlagForName(dictionary[key][@"flag"])];
        [mutableAttributes crlf_safeSetObject:attribute forKey:key];
    }
    return mutableAttributes;
}
+ (NSMutableArray<NSDictionary *> *)JSONAttributesArrayFromAttributes:(CRLFAttributes *)attributes {
    NSMutableArray *ret = [NSMutableArray array];
    for (NSString *key in attributes) {
        NSMutableDictionary *fullAttr = [NSMutableDictionary dictionary];
        if (attributes[key].value == nil || ![attributes[key].value isKindOfClass:[NSString class]] || [((NSString *)attributes[key].value) isEqualToString:@""]) {
            continue;
        }
        fullAttr[@"key"] = key;        
        fullAttr[@"value"] = attributes[key].value;
        fullAttr[@"flag"] = CRLFAttributeNameForAttributeFlag(attributes[key].flags);
        [ret addObject:fullAttr];
    }
    return ret;
}
+ (CRLFMutableAttributes *)mutableAttributesFromJSONArray:(NSArray<NSDictionary *> *)array {
    CRLFMutableAttributes *mutableAttributes = [[NSMutableDictionary alloc] init];
    for (NSDictionary *fullDict in array) {
        CRLFAttribute *attribute = [CRLFAttribute attributeWithString:fullDict[@"value"] flags:CRLFAttributeFlagForName(fullDict[@"flag"])];
        mutableAttributes[fullDict[@"key"]] = attribute;
    }
    return mutableAttributes;
}
@end
