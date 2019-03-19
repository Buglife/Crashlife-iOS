//
//  CRLFTelephonyNetworkInfo.m
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

#import "CRLFTelephonyNetworkInfo.h"
#import "CRLFMacros.h"
#import <CoreTelephony/CTTelephonyNetworkInfo.h>
#import <CoreTelephony/CTCarrier.h>

#pragma mark - Protocols

#pragma mark - CRLFTelephonyNetworkInfo

@interface CRLFTelephonyNetworkInfo ()

@property (nonatomic, nullable) CTTelephonyNetworkInfo *networkInfo;
@property (nonatomic, nullable) CTCarrier *carrier;

@end

NSString *CRLFTelephonyNetworkInfoDidUpdateNotification = @"CRLFTelephonyNetworkInfoDidUpdateNotification";

@implementation CRLFTelephonyNetworkInfo

#pragma mark - Initialization

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self _crlf_privateInit];
    }
    return self;
}

- (void)_crlf_privateInit
{
    _networkInfo = [[CTTelephonyNetworkInfo alloc] init];
    //TODO: in iOS12+, use the newer method for getting multiple carriers on dual sim phones.
    _carrier = [_networkInfo subscriberCellularProvider];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(radioAccessChanged:) name:CTRadioAccessTechnologyDidChangeNotification object:nil];
}

- (NSString *)carrierName {
    return self.carrier.carrierName;
}

- (void)radioAccessChanged:(NSNotification *)notif {
    self.carrier = self.networkInfo.subscriberCellularProvider;
    _currentRadioAccessTechnology = self.networkInfo.currentRadioAccessTechnology;
    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter] postNotificationName:CRLFTelephonyNetworkInfoDidUpdateNotification object:nil];
    });
}

@end
