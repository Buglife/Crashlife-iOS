//
//  Crashlife.h
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

#import <UIKit/UIKit.h>

@interface Crashlife : NSObject


/**
 *  Default shared initializer that returns the Crashlife singleton.
 *
 *  @return The shared Crashlife singleton
 */
@property (nonatomic, nonnull, readonly, class) Crashlife *sharedCrashlife;

/**
 *  Enables Crashlife bug reporting within your app.
 *
 *  The recommended way to enable Crashlife is to call this method
 *  in your app delegate's `-application:didFinishLaunchingWithOptions:` method.
 *  Don't worry, it won't impact your app's launch performance. 😉
 *
 *  @param apiKey The Crashlife API Key for your organization
 */
- (void)startWithAPIKey:(nonnull NSString *)apiKey;


/**
 *  Specifies a user identifier that will be visible in the Crashlife report viewer UI.
 * 
 *  @param identifier An arbitrary string that identifies a user for your app.
 */
- (void)setUserIdentifier:(nullable NSString *)identifier;

/**
 *  Adds custom data to bug reports. Set a `nil` value for a given attribute to delete
 *  its current value.
 */
- (void)setStringValue:(nullable NSString *)value forAttribute:(nonnull NSString *)attribute;

/**
 * Creates a footprint (a timestamped event) for viewing in the Crashlife web dashboard.
 *
 * @param name a name for this footprint. It might be related to a user event, or to a block completion.
 */
- (void)leaveFootprint:(NSString *)name;

/**
 * Creates a footprint (a timestamped event with arbitaray stringmetadata) for viewing in the Crashlife web dashboard.
 *
 * @param name a name for this footprint. It might be related to a user event, or to a block completion.
 */
- (void)leaveFootprint:(NSString *)name withMetadata:(NSDictionary<NSString *, NSString *> *)metadata;

/**
 * Log and post an exception to Crashlife.
 * @param exception the exception to log.
 */
- (void)logException:(NSException *)exception;

/**
 * Log a non-exception error to Crashlife.
 * @param error the `NSError` to log.
 */
- (void)logErrorObject:(NSError *)error;

/**
 * Log a non-exception error message to Crashlife.
 * @param message the error message to log.
 */
- (void)logError:(NSString *)message;

/**
 * Log a warning message to Crashlife.
 * @param message the warning message to log.
 */
- (void)logWarning:(NSString *)message;

/**
 * Log an informational message to Crashlife.
 * @param message the informational message to log.
 */
- (void)logInfo:(NSString *)message;

/**
 *  Sorry, Crashlife is a singleton 😁
 *  Please use the shared initializer `Crashlife.sharedCrashlife()`
 */
- (null_unspecified instancetype)init NS_UNAVAILABLE;

@end
