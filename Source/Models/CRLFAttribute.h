//
//  CRLFAttribute.h
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

#import <Foundation/Foundation.h>

/**
 @warning These map to enums in the Crashlife backend API!
 */
typedef NS_ENUM(NSUInteger, CRLFAttributeValueType) {
    CRLFAttributeValueTypeString = 0,
    CRLFAttributeValueTypeInt = 1,
    CRLFAttributeValueTypeFloat = 2,
    CRLFAttributeValueTypeBool = 3
};

typedef NS_OPTIONS(NSUInteger, CRLFAttributeFlags) {
    CRLFAttributeFlagCustom   = 1 << 1, // The new default for dev-set attributes
    CRLFAttributeFlagSystem   = 1 << 2, // The new default for Crashlife-gathered attributes
    CRLFAttributeFlagPublic   = 1 << 3, // Set this to show this attribute in public when not logged in (not supported yet)
    CRLFAttributeFlagInternal = 1 << 4, // This is for Crashlife metrics only. Do not use.
};

@class CRLFAttribute;

typedef NSDictionary<NSString *, CRLFAttribute *> CRLFAttributes;
typedef NSMutableDictionary<NSString *, CRLFAttribute *> CRLFMutableAttributes;

@interface CRLFAttribute : NSObject <NSCoding>

@property (nonatomic, readonly) CRLFAttributeValueType valueType;
@property (nonatomic, readonly) CRLFAttributeFlags flags;

+ (instancetype)attributeWithBool:(BOOL)boolValue flags:(CRLFAttributeFlags)flags;
+ (instancetype)attributeWithString:(NSString *)stringValue flags:(CRLFAttributeFlags)flags;

- (instancetype)initWithValueType:(CRLFAttributeValueType)valueType value:(NSObject *)value flags:(CRLFAttributeFlags)flags;

- (NSString *)stringValue;

- (NSDictionary *)JSONDictionary;

+ (NSDictionary *)JSONDictionaryForAttributesDictionary:(NSMutableDictionary<NSString *, CRLFAttribute *> *)attributes;
+ (CRLFMutableAttributes *)mutableSystemAttributesFromDictionary:(NSDictionary *)dictionary;
+ (CRLFMutableAttributes *)mutableAttributesFromJSONDictionary:(NSDictionary *)dictionary;
+ (NSMutableArray<NSDictionary *> *)JSONAttributesArrayFromAttributes:(CRLFAttributes *)attributes;
+ (CRLFMutableAttributes *)mutableAttributesFromJSONArray:(NSArray<NSDictionary *> *)array;
@end
