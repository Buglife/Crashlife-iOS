//
// Created by Daniel DeCovnick on 2019-02-06.
// Copyright (c) 2019 Buglife, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface CRLFClient : NSObject
@property (nonatomic, readonly) NSString *apiKey;
@property (nonatomic, copy) NSString *userIdentifier;

- (instancetype)initWithAPIKey:(NSString *)apiKey sdkVersion:(NSString *)sdkVersion;
- (void)submitPendingReports;
+ (NSMutableDictionary *)mutableCLKSCrashUserInfoDict;
- (void)setStringValue:(nullable NSString *)value forAttribute:(nonnull NSString *)attribute;
- (void)leaveFootprint:(NSString *)name;
- (void)leaveFootprint:(NSString *)name withMetadata:(NSDictionary<NSString *, NSString *> *)metadata;
- (void)logException:(NSException *)exception;
- (void)logErrorObject:(NSError *)error;
- (void)logError:(NSString *)message;
- (void)logWarning:(NSString *)message;
- (void)logInfo:(NSString *)message;
- (void)logClientEventWithName:(nonnull NSString *)eventName afterDelay:(NSTimeInterval)delay;
@end
