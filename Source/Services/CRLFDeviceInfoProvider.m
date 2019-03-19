//
//  CRLFDeviceInfoProvider.m
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

#import "CRLFDeviceInfoProvider.h"
#import "CRLFMacros.h"
#import <mach/mach.h>
#import <mach/mach_host.h>
#import <sys/utsname.h>
#import <UIKit/UIKit.h>
#import "CRLFTelephonyNetworkInfo.h"
#import "CRLFReachability.h"
#import "CRLFDeviceInfo.h"
#import "CRLFAttribute.h"
#import <CoreLocation/CoreLocation.h>

NSNumber *crlf_mach_freeMemory(void);
NSNumber *crlf_mach_usableMemory(void);
CRLFDeviceBatteryState CRLFDeviceBatteryStateFromUIDeviceBatteryState(UIDeviceBatteryState batteryState);
NSString *CRLFThermalStateStringFromThermalState(NSProcessInfoThermalState thermalState) API_AVAILABLE(ios(11.0));;
static NSString *CRLFContentSizeCategoryFromUIContentSizeCategory(UIContentSizeCategory contentSizeCategory);

@implementation CRLFDeviceInfoProvider

#pragma mark - Public methods

- (void)fetchDeviceInfoToQueue:(dispatch_queue_t)completionQueue completion:(void (^)(CRLFDeviceInfo *, CRLFAttributes *))completionHandler
{
    CRLFDeviceInfo *deviceInfo = [[CRLFDeviceInfo alloc] init];
    
    NSDictionary *fileSystemAttributes = [self _fileSystemAttributes];
    deviceInfo.fileSystemSizeInBytes = fileSystemAttributes[NSFileSystemSize];
    deviceInfo.freeFileSystemSizeInBytes = fileSystemAttributes[NSFileSystemFreeSize];
    
    deviceInfo.freeMemory = crlf_mach_freeMemory();
    deviceInfo.usableMemory = crlf_mach_usableMemory();
    
    CRLFTelephonyNetworkInfo *networkInfo = [[CRLFTelephonyNetworkInfo alloc] init];
    deviceInfo.carrierName = networkInfo.carrierName;
    deviceInfo.currentRadioAccessTechnology = networkInfo.currentRadioAccessTechnology;
    
    CRLFReachability *reachability = [CRLFReachability reachabilityForLocalWiFi];
    deviceInfo.wifiConnected = [reachability isReachableViaWiFi];
    
    dispatch_async(dispatch_get_main_queue(), ^{
        CRLFMutableAttributes *attributes = [[CRLFMutableAttributes alloc] init];
        BOOL reduceMotionEnabled = UIAccessibilityIsReduceMotionEnabled();
        attributes[@"Reduce motion enabled"] = [CRLFAttribute attributeWithBool:reduceMotionEnabled flags:CRLFAttributeFlagSystem];
        
        UIContentSizeCategory uiContentSizeCategory = [[UIApplication sharedApplication] preferredContentSizeCategory];
        NSString *contentSizeCategory = CRLFContentSizeCategoryFromUIContentSizeCategory(uiContentSizeCategory);
        
        if (contentSizeCategory) {
            attributes[@"Content size category"] = [CRLFAttribute attributeWithString:contentSizeCategory flags:CRLFAttributeFlagSystem];
        }
        
        // Probably shouldn't be accessing UIKit off the main thread.
        // (I'd almost consider UIDevice an exception, but let's play it safe)
        UIDevice *currentDevice = [UIDevice currentDevice];
        deviceInfo.operatingSystemVersion = [currentDevice systemVersion];
        deviceInfo.identifierForVendor = [currentDevice identifierForVendor].UUIDString;
        deviceInfo.deviceModel = [CRLFDeviceInfoProvider _deviceModel];
        
        // I also can't seem to find anything that confirms the thread safety of NSLocale :-/
        deviceInfo.localeIdentifier = [NSLocale currentLocale].localeIdentifier;
        
        BOOL wasBatteryMonitoringEnabled = currentDevice.batteryMonitoringEnabled;
        currentDevice.batteryMonitoringEnabled = YES;
        
        if (currentDevice.batteryMonitoringEnabled) {
            deviceInfo.batteryLevel = currentDevice.batteryLevel;
            deviceInfo.batteryState = CRLFDeviceBatteryStateFromUIDeviceBatteryState(currentDevice.batteryState);
            
            NSProcessInfo *processInfo = [NSProcessInfo processInfo];
            
            if ([processInfo respondsToSelector:@selector(isLowPowerModeEnabled)]) {
                deviceInfo.lowPowerMode = [[NSProcessInfo processInfo] isLowPowerModeEnabled];
            }
            
            if (@available(iOS 11.0, *)) {
                NSString *thermalState = CRLFThermalStateStringFromThermalState(processInfo.thermalState);
                
                if (thermalState) {
                    attributes[@"Thermal state"] = [CRLFAttribute attributeWithString:thermalState flags:CRLFAttributeFlagSystem];
                }
            }
        }
        
        currentDevice.batteryMonitoringEnabled = wasBatteryMonitoringEnabled;
        
        CLLocation *lastLocation = [[[CLLocationManager alloc] init] location];
        // this is a synchronous method, so no need to save the CLLocationManager... or is there? Does this even work
        // without the original CLLocationManager, since it's not a singleton? TBD.
        CLLocationDegrees latitude = 0.0;
        CLLocationDegrees longitude = 0.0;
        if (lastLocation != nil) {
            if (CLLocationCoordinate2DIsValid(lastLocation.coordinate)) {
                latitude = lastLocation.coordinate.latitude;
                longitude = lastLocation.coordinate.longitude;
                attributes[@"Device location"] = [CRLFAttribute attributeWithString:[NSString stringWithFormat:@"%f,%f", latitude, longitude] flags:CRLFAttributeFlagSystem];
            }
        }


        
        dispatch_async(completionQueue, ^{
            completionHandler(deviceInfo, attributes.copy);
        });
    });
}

#pragma mark - Private methods

+ (NSString *)_deviceModel
{
    struct utsname systemInfo;
    uname(&systemInfo);
    return [NSString stringWithCString:systemInfo.machine encoding:NSUTF8StringEncoding];
}

// Keys of interest: NSFileSystemFreeSize and NSFileSystemSize
- (NSDictionary *)_fileSystemAttributes
{
    NSArray *documentPaths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentPath = documentPaths.lastObject;
    
    if (documentPath) {
        NSError *error;
        NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfFileSystemForPath:documentPath error:&error];
        
        if (attributes) {
            return attributes;
        } else {
            NSParameterAssert(NO);
            CRLFLogExtDebug(@"Error getting file system attributes: %@", error);
        }
    } else {
        NSParameterAssert(NO);
        CRLFLogExtDebug(@"Error accessing documentsPath");
    }
    
    return nil;
}

#pragma mark - Mach stuff

bool crlf_mach_i_VMStats(vm_statistics_data_t* const vmStats,
                      vm_size_t* const pageSize)
{
    kern_return_t kr;
    const mach_port_t hostPort = mach_host_self();
    
    if((kr = host_page_size(hostPort, pageSize)) != KERN_SUCCESS)
    {
        // TODO: Log this?
        //KSLOG_ERROR("host_page_size: %s", mach_error_string(kr));
        return false;
    }
    
    mach_msg_type_number_t hostSize = sizeof(*vmStats) / sizeof(natural_t);
    kr = host_statistics(hostPort,
                         HOST_VM_INFO,
                         (host_info_t)vmStats,
                         &hostSize);
    if(kr != KERN_SUCCESS)
    {
        // TODO: Log this?
        //KSLOG_ERROR("host_statistics: %s", mach_error_string(kr));
        return false;
    }
    
    return true;
}

NSNumber *crlf_mach_freeMemory()
{
    vm_statistics_data_t vmStats;
    vm_size_t pageSize;
    if(crlf_mach_i_VMStats(&vmStats, &pageSize))
    {
        uint64_t result = ((uint64_t)pageSize) * vmStats.free_count;
        return [NSNumber numberWithUnsignedLongLong:result];
    }
    return nil;
}

NSNumber *crlf_mach_usableMemory()
{
    vm_statistics_data_t vmStats;
    vm_size_t pageSize;
    if(crlf_mach_i_VMStats(&vmStats, &pageSize))
    {
        uint64_t result = ((uint64_t)pageSize) * (vmStats.active_count +
                                       vmStats.inactive_count +
                                       vmStats.wire_count +
                                       vmStats.free_count);
        return [NSNumber numberWithUnsignedLongLong:result];
    }
    return nil;
}

@end

CRLFDeviceBatteryState CRLFDeviceBatteryStateFromUIDeviceBatteryState(UIDeviceBatteryState batteryState) {
    switch (batteryState) {
        case UIDeviceBatteryStateUnknown:
            return CRLFDeviceBatteryStateUnknown;
        case UIDeviceBatteryStateUnplugged:
            return CRLFDeviceBatteryStateUnplugged;
        case UIDeviceBatteryStateCharging:
            return CRLFDeviceBatteryStateCharging;
        case UIDeviceBatteryStateFull:
            return CRLFDeviceBatteryStateFull;
        default:
            break;
    }
}

NSString *CRLFThermalStateStringFromThermalState(NSProcessInfoThermalState thermalState) {
    switch (thermalState) {
        case NSProcessInfoThermalStateCritical:
            return @"critical";
        case NSProcessInfoThermalStateFair:
            return @"fair";
        case NSProcessInfoThermalStateNominal:
            return @"nominal";
        case NSProcessInfoThermalStateSerious:
            return @"serious";
        default:
            return nil;
    }
}

static NSString *CRLFContentSizeCategoryFromUIContentSizeCategory(UIContentSizeCategory contentSizeCategory) {
    if ([contentSizeCategory isEqualToString:UIContentSizeCategoryExtraSmall]) {
        return @"extra small";
    } else if ([contentSizeCategory isEqualToString:UIContentSizeCategorySmall]) {
        return @"small";
    } else if ([contentSizeCategory isEqualToString:UIContentSizeCategoryMedium]) {
        return @"medium";
    } else if ([contentSizeCategory isEqualToString:UIContentSizeCategoryLarge]) {
        return @"large";
    } else if ([contentSizeCategory isEqualToString:UIContentSizeCategoryExtraLarge]) {
        return @"extra large";
    } else if ([contentSizeCategory isEqualToString:UIContentSizeCategoryExtraExtraLarge]) {
        return @"extra extra large";
    } else if ([contentSizeCategory isEqualToString:UIContentSizeCategoryExtraExtraExtraLarge]) {
        return @"extra extra extra large";
    } else if ([contentSizeCategory isEqualToString:UIContentSizeCategoryAccessibilityMedium]) {
        return @"accessibility medium";
    } else if ([contentSizeCategory isEqualToString:UIContentSizeCategoryAccessibilityLarge]) {
        return @"accessibility large";
    } else if ([contentSizeCategory isEqualToString:UIContentSizeCategoryAccessibilityExtraLarge]) {
        return @"accessibility extra large";
    } else if ([contentSizeCategory isEqualToString:UIContentSizeCategoryAccessibilityExtraExtraLarge]) {
        return @"accessibility extra extra large";
    } else if ([contentSizeCategory isEqualToString:UIContentSizeCategoryAccessibilityExtraExtraExtraLarge]) {
        return @"accessibility extra extra extra large";
    }
    
    return nil;
}
