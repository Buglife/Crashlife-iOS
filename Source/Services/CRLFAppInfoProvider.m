//
//  CRLFAppInfoProvider.m
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

#import "CRLFAppInfoProvider.h"
#import "CRLFAppInfo.h"
#import "CRLFMacros.h"

@interface CRLFAppInfoProvider ()

@property (nonatomic) dispatch_queue_t workQueue;

@end

@implementation CRLFAppInfoProvider

- (instancetype)init
{
    self = [super init];
    if (self) {
        _workQueue = dispatch_queue_create("com.buglife.crashlife.CRLFEAppInfoProvider.workQueue", DISPATCH_QUEUE_SERIAL);
    }
    return self;
}

#pragma mark - Public methods

- (void)asyncFetchAppInfoToQueue:(dispatch_queue_t)completionQueue completion:(void (^)(CRLFAppInfo *))completionHandler
{
    __weak typeof(self) weakSelf = self;

    dispatch_async(_workQueue, ^{
        __strong CRLFAppInfoProvider *strongSelf = weakSelf;
        
        if (strongSelf) {
            CRLFAppInfo *appInfo = [strongSelf _appInfo];
            
            dispatch_async(completionQueue, ^{
                completionHandler(appInfo);
            });
        } else {
            NSAssert(NO, @"weak ref zero'd out before strongifying");
//            CRLFLogIntError(@"Error getting app info (error code 136)"); // arbitrary error code. TODO: keep track of these somewhere
            dispatch_async(completionQueue, ^{
                completionHandler(nil);
            });
        }
    });
}

- (CRLFAppInfo *)syncFetchAppInfo
{
    return [self _appInfo];
}

#pragma mark - Private methods

- (CRLFAppInfo *)_appInfo
{
//    NSParameterAssert(![NSThread isMainThread]);
    CRLFAppInfo *appInfo = [[CRLFAppInfo alloc] init];
    NSBundle *bundle = [NSBundle mainBundle];
    
    appInfo.bundleShortVersion = [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
    appInfo.bundleVersion = [bundle objectForInfoDictionaryKey:(NSString *)kCFBundleVersionKey];
    appInfo.bundleIdentifier = [bundle bundleIdentifier];
    appInfo.bundleName = [bundle objectForInfoDictionaryKey:@"CFBundleName"];
    return appInfo;
}

@end
