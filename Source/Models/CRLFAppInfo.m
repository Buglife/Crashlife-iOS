//
//  CRLFAppInfo.m
//  Copyright (C) 2017 Buglife, Inc.
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

#import "CRLFAppInfo.h"
#import "CRLFMacros.h"
#import "NSMutableDictionary+CRLFAdditions.h"

@implementation CRLFAppInfo

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super init];
    if (self) {
        for (NSString *key in [[self class] _objectPropertyKeys]) {
            id value = [coder decodeObjectForKey:key];
            [self setValue:value forKey:key];
        }
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    for (NSString *key in [[self class] _objectPropertyKeys]) {
        id value = [self valueForKey:key];
        [coder encodeObject:value forKey:key];
    }
}

+ (NSArray<NSString *> *)_objectPropertyKeys
{
    return @[CRLF_STRING_FROM_SELECTOR_NAMED(bundleShortVersion),
             CRLF_STRING_FROM_SELECTOR_NAMED(bundleVersion),
             CRLF_STRING_FROM_SELECTOR_NAMED(bundleIdentifier),
             CRLF_STRING_FROM_SELECTOR_NAMED(bundleName)];
}

- (NSDictionary *)JSONDictionary
{
    CRLFAppInfo *appInfo = self;
    NSMutableDictionary *appDict = @{}.mutableCopy;

    [appDict crlf_safeSetObject:appInfo.bundleIdentifier forKey:@"bundle_identifier"];
    [appDict crlf_safeSetObject:appInfo.bundleShortVersion forKey:@"bundle_short_version"];
    [appDict crlf_safeSetObject:appInfo.bundleVersion forKey:@"bundle_version"];
    [appDict crlf_safeSetObject:appInfo.bundleName forKey:@"bundle_name"];
    
    return [NSDictionary dictionaryWithDictionary:appDict];
}

@end
