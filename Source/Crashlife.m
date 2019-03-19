//
//  Crashlife.m
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

#import "Crashlife.h"
#import "CRLFAttribute.h"
#import "CRLFCompatibilityUtils.h"
#import "CRLFMacros.h"
#import "CLKSCrash.h"
#import "CRLFClient.h"
#import "CLKSCrash.h"
#import "CRLFFootprint.h"


static NSString * const kSDKVersion = @"1.0.0";

@interface Crashlife ()
@property (nonatomic, readonly) BOOL isStarted;
@property (nonatomic) NSString *userIdentifier;
@property (nonatomic) CRLFClient *client;
@end

@implementation Crashlife

#pragma mark - Public

+ (instancetype)sharedCrashlife
{
    static Crashlife *sSharedCrashlife = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if ([CRLFCompatibilityUtils isiOS9OrHigher]) {
            sSharedCrashlife = [[self alloc] initInternal];
        } else {
            CRLFLogExtWarn(@"Crashlife Warning: This version of the Crashlife SDK only supports iOS 9 or higher.");
        }
    });
    return sSharedCrashlife;
}

- (instancetype)initInternal
{
    self = [super init];
    if (self) {
//        _attributes = [[NSMutableDictionary alloc] init];
//        _footprints = [[NSMutableArray alloc] init];
    }
    return self;
}

- (instancetype)init
{
    CRLFLogExtError(@"Crashlife Error: Sorry, Crashlife is a singleton! 😁 Please initialize using +[Crashlife sharedCrashlife].");
    return nil;
}

#define CRLFLogErrorMultipleStartAttempts CRLFLogExtDebug(@"Crashlife Error: Attempted to call %@ more than once! Subsequent calls will be ignored.", NSStringFromSelector(@selector(startWithAPIKey:)))

- (BOOL)isStarted {
    return self.client != nil;
}

- (void)startWithAPIKey:(NSString *)apiKey
{
    if ([self isStarted]) {
        CRLFLogErrorMultipleStartAttempts;
        return;
    }

    if (apiKey == nil) {
        CRLFLogExtDebug(@"Crashlife Error: Attempted to call [%@ %@] with a nil API Key!", NSStringFromClass([self class]), NSStringFromSelector(@selector(startWithAPIKey:)));
        return;
    }
    self.client = [[CRLFClient alloc] initWithAPIKey:apiKey sdkVersion:kSDKVersion.copy];
    [self _startCrashlife];
}

- (void)_startCrashlife
{
    NSParameterAssert([self isStarted]);
    [self.client submitPendingReports];
    [self.client logClientEventWithName:@"app_launch" afterDelay:10.0];
}

- (void)setUserIdentifier:(NSString *)identifier {
    _userIdentifier = identifier;
    self.client.userIdentifier = identifier;
}

#pragma mark - Attributes and Footprints

- (void)setStringValue:(NSString *)stringValue forAttribute:(NSString *)attributeKey {
    [self.client setStringValue:stringValue forAttribute:attributeKey];
}

- (void)leaveFootprint:(NSString *)name {
    [self.client leaveFootprint:name];
}

- (void)leaveFootprint:(NSString *)name withMetadata:(NSDictionary<NSString *, NSString *> *)metadata {
    [self.client leaveFootprint:name withMetadata:metadata];
}

#pragma mark Log Event
- (void)logException:(NSException *)exception {
    [self.client logException:exception];
}

- (void)logErrorObject:(NSError *)error {
    [self.client logErrorObject:error];
}

- (void)logError:(NSString *)message {
    [self.client logError:message];
}

- (void)logWarning:(NSString *)message {
    [self.client logWarning:message];
}

- (void)logInfo:(NSString *)message {
    [self.client logInfo:message];
}

@end

UIInterfaceOrientationMask interfaceOrientationMaskFromInterfaceOrientation(UIInterfaceOrientation orientation) {
    switch (orientation) {
        case UIInterfaceOrientationPortrait:
            return UIInterfaceOrientationMaskPortrait;
        case UIInterfaceOrientationPortraitUpsideDown:
            return UIInterfaceOrientationMaskPortraitUpsideDown;
        case UIInterfaceOrientationLandscapeLeft:
            return UIInterfaceOrientationMaskLandscapeLeft;
        case UIInterfaceOrientationLandscapeRight:
            return UIInterfaceOrientationMaskLandscapeRight;
        default:
            return UIInterfaceOrientationMaskPortrait;
    }
}
