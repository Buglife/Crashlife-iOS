//
//  CRLFDeviceInfo.m
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

#import "CRLFDeviceInfo.h"
#import "CRLFMacros.h"

@implementation CRLFDeviceInfo

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super init];
    if (self) {
        for (NSString *key in [[self class] _objectPropertyKeys]) {
            id value = [coder decodeObjectForKey:key];
            [self setValue:value forKey:key];
        }

        self.wifiConnected = [coder decodeBoolForKey:NSStringFromSelector(@selector(wifiConnected))];
        self.batteryLevel = [coder decodeFloatForKey:NSStringFromSelector(@selector(batteryLevel))];
        self.batteryState = [coder decodeIntegerForKey:NSStringFromSelector(@selector(batteryState))];
        self.lowPowerMode = [coder decodeBoolForKey:NSStringFromSelector(@selector(lowPowerMode))];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    for (NSString *key in [[self class] _objectPropertyKeys]) {
        id value = [self valueForKey:key];
        [coder encodeObject:value forKey:key];
    }
    
    [coder encodeBool:self.wifiConnected forKey:NSStringFromSelector(@selector(wifiConnected))];
    [coder encodeFloat:self.batteryLevel forKey:NSStringFromSelector(@selector(batteryLevel))];
    [coder encodeInteger:self.batteryState forKey:NSStringFromSelector(@selector(batteryState))];
    [coder encodeBool:self.lowPowerMode forKey:NSStringFromSelector(@selector(lowPowerMode))];
}

+ (NSArray<NSString *> *)_objectPropertyKeys
{
    return @[CRLF_STRING_FROM_SELECTOR_NAMED(operatingSystemVersion),
             CRLF_STRING_FROM_SELECTOR_NAMED(deviceModel),
             CRLF_STRING_FROM_SELECTOR_NAMED(fileSystemSizeInBytes),
             CRLF_STRING_FROM_SELECTOR_NAMED(freeFileSystemSizeInBytes),
             CRLF_STRING_FROM_SELECTOR_NAMED(freeMemory),
             CRLF_STRING_FROM_SELECTOR_NAMED(usableMemory),
             CRLF_STRING_FROM_SELECTOR_NAMED(identifierForVendor),
             CRLF_STRING_FROM_SELECTOR_NAMED(localeIdentifier),
             CRLF_STRING_FROM_SELECTOR_NAMED(carrierName),
             CRLF_STRING_FROM_SELECTOR_NAMED(currentRadioAccessTechnology)];
}

@end
