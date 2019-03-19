//
//  CRLFDeviceInfoProvider.h
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

#import <Foundation/Foundation.h>
#import "CRLFAttribute.h"
#import "CRLFDeviceBatteryState.h"

@class CRLFDeviceInfo;

@interface CRLFDeviceInfoProvider : NSObject

- (void)fetchDeviceInfoToQueue:(dispatch_queue_t)completionQueue completion:(void (^)(CRLFDeviceInfo *deviceInfo, CRLFAttributes *systemAttributes))completionHandler;

@end

API_AVAILABLE(ios(11.0))
extern NSString *CRLFThermalStateStringFromThermalState(NSProcessInfoThermalState thermalState);
extern CRLFDeviceBatteryState CRLFDeviceBatteryStateFromUIDeviceBatteryState(UIDeviceBatteryState batteryState);
