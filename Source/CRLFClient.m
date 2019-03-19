//
// Created by Daniel DeCovnick on 2019-02-06.
// Copyright (c) 2019 Buglife, Inc. All rights reserved.
//

#import "CRLFClient.h"
#import "CRLFNetworkManager.h"
#import "CLKSCrashReportFilter.h"
#import "CLKSCrash.h"
#import "CRLFMacros.h"
#import "NSMutableDictionary+CRLFAdditions.h"
#import "CRLFAppInfoProvider.h"
#import "CRLFAppInfo.h"
#import "CRLFDeviceInfoProvider.h"
#import "NSError+CRLFAdditions.h"
#import "CRLFEvent.h"
#import "CRLFCrashReport.h"
#import "CRLFDeviceBatteryState.h"
#import <libkern/OSAtomic.h>
#import "CRLFTelephonyNetworkInfo.h"
#import "CRLFCompatibilityUtils.h"
#import "CRLFReachability.h"
#import "CRLFFootprint.h"
#import "CLKSCrashReportStore.h"
#import "CLKSJSONCodecObjC.h"
#import "CLKSCrashReportFields.h"
#import "NSBundle+CRLFAdditions.h"

@interface CRLFClient () <CLKSCrashReportFilter>
@property (nonatomic) CLKSCrashReportFilterCompletion completionHandler;
@property (nonatomic) CRLFNetworkManager *networkManager;
@property (nonatomic) dispatch_queue_t workQueue;
@property (nonatomic, nonnull) CRLFAppInfoProvider *appInfoProvider;
@property (nonatomic, nonnull) CRLFDeviceInfoProvider *deviceInfoProvider;
@property (nonatomic, copy) NSString *sdkVersion;
@property (nonatomic, copy) NSString *sdkName;
@property (nonatomic, copy) NSString *platform;
@property (nonatomic) CRLFTelephonyNetworkInfo *networkInfo;
@property (nonatomic) CRLFReachability *reachability;
@property (nonatomic) NSMutableDictionary<NSString *, CRLFAttribute *> *attributes;
@property (nonatomic) NSMutableArray<CRLFFootprint *> *footprints;

@end
@implementation CRLFClient
- (instancetype)initWithAPIKey:(NSString *)apiKey sdkVersion:(NSString *)sdkVersion {
    self = [super init];
    if (self != nil) {
        [self configureCrashReporter];
        _apiKey = apiKey;
        _networkManager = [[CRLFNetworkManager alloc] init];
        _networkInfo = [[CRLFTelephonyNetworkInfo alloc] init];
        _workQueue = dispatch_queue_create("com.buglife.crashlife.clientWorkQueue", DISPATCH_QUEUE_SERIAL);
        _appInfoProvider = [[CRLFAppInfoProvider alloc] init];
        _deviceInfoProvider = [[CRLFDeviceInfoProvider alloc] init];
        _reachability = [CRLFReachability reachabilityForLocalWiFi];
        // _*work* must exist before setting the sink.
        [CLKSCrash sharedInstance].sink = self;
        _sdkName = @"Crashlife iOS";
        _sdkVersion = sdkVersion;
        _platform = @"ios";
    }
    return self;
}

- (void)configureCrashReporter {
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(localeChanged:) name:NSCurrentLocaleDidChangeNotification object:nil];
    if (@available(iOS 11.0, *)) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(thermalStateChanged:) name:NSProcessInfoThermalStateDidChangeNotification object:nil];
    }
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(telephonyInfoChanged:) name:CRLFTelephonyNetworkInfoDidUpdateNotification object:nil];
    UIDevice.currentDevice.batteryMonitoringEnabled = YES;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(batteryLevelChanged:) name:UIDeviceBatteryLevelDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(batteryStateChanged:) name:UIDeviceBatteryStateDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(isOnWiFiChanged:) name:CRLFReachabilityWiFiStateChangedNotificationName object:nil];
    //We don't save this because its .sink is retained.
    CLKSCrash *crashReporter = [CLKSCrash sharedInstance];
    crashReporter.deleteBehaviorAfterSendAll = CLKSCDeleteOnSucess;
    crashReporter.maxReportCount = 1000; //TODO: maybe a better number here
    crashReporter.introspectMemory = YES;
    crashReporter.searchQueueNames = YES;
    crashReporter.addConsoleLogToReport = YES;
    //TODO: configure monitor types
    //TODO: determine if we want to set our own .onCrash
    [crashReporter install];
    
    // good a place as any for this:
    NSMutableDictionary *dict = [CRLFClient mutableCLKSCrashUserInfoDict];
    [dict crlf_safeSetObject:UIDevice.currentDevice.identifierForVendor.UUIDString forKey:@"device_identifier"];
    CLKSCrash.sharedInstance.userInfo = dict;
    
}

- (void)submitPendingReports {
    [[CLKSCrash sharedInstance] sendAllReportsWithCompletion:^(NSArray *filteredReports, BOOL completed, NSError *error) {
        if (error != nil) {
            CRLFLogExtWarn(@"Unable to submit pending crash reports");
        }
        //TODO: figure out what else to do here, if anything.
        // Note: deletion is handled by KS
    }];
}

#pragma mark metatdata changes as we go

- (void)localeChanged:(NSNotification *)notif {
    NSLocale *currentLocale = [NSLocale currentLocale];
    CRLFAttribute *localeAttribute = [CRLFAttribute attributeWithString:currentLocale.localeIdentifier flags:CRLFAttributeFlagSystem];
    [self _setAttribute:localeAttribute forKey:@"locale"];
}

- (void)thermalStateChanged:(NSNotification *)notif {
    if (@available(iOS 11.0, *)) {
        NSProcessInfoThermalState thermalState = [[NSProcessInfo processInfo] thermalState];
        CRLFAttribute *thermalStateAttribute = [CRLFAttribute attributeWithString:CRLFThermalStateStringFromThermalState(thermalState) flags:CRLFAttributeFlagSystem];
        [self _setAttribute:thermalStateAttribute forKey:@"thermal_state"];
    }
}

- (void)powerStateChanged:(NSNotification *)notif {
    BOOL lowPowerMode = [[NSProcessInfo processInfo] isLowPowerModeEnabled];
    CRLFAttribute *lowPowerModeAttribute = [CRLFAttribute attributeWithString:lowPowerMode?@"true":@"false" flags:CRLFAttributeFlagSystem];
    [self _setAttribute:lowPowerModeAttribute forKey:@"low_power_mode"];
}

- (void)telephonyInfoChanged:(NSNotification *)notif {
    NSString *carrierName = self.networkInfo.carrierName;
    NSString *radioAccessTechnology = self.networkInfo.currentRadioAccessTechnology;
    CRLFAttribute *carrierNameAttribute = [CRLFAttribute attributeWithString:carrierName flags:CRLFAttributeFlagSystem];
    CRLFAttribute *ratAttribute = [CRLFAttribute attributeWithString:radioAccessTechnology flags:CRLFAttributeFlagSystem];
    [self _setAttribute:carrierNameAttribute forKey:@"carrier_name"];
    [self _setAttribute:ratAttribute forKey:@"current_radio_access_technology"];
}

- (void)batteryStateChanged:(NSNotification *)notif {
    CRLFDeviceBatteryState batteryState = CRLFDeviceBatteryStateFromUIDeviceBatteryState(UIDevice.currentDevice.batteryState);
    CRLFAttribute *batteryStateAttribute = [CRLFAttribute attributeWithString:@(batteryState).description flags:CRLFAttributeFlagSystem];
    [self _setAttribute:batteryStateAttribute forKey:@"battery_state"];
}

- (void)batteryLevelChanged:(NSNotification *)notif {
    CRLFAttribute *batteryLevel = [CRLFAttribute attributeWithString:[NSString stringWithFormat:@"%.2f", UIDevice.currentDevice.batteryLevel] flags:CRLFAttributeFlagSystem];
    [self _setAttribute:batteryLevel forKey:@"battery_level"];
}

- (void)isOnWiFiChanged:(NSNotification *)notif {
    CRLFAttribute *onWifi = [CRLFAttribute attributeWithString:self.reachability.isReachableViaWiFi?@"true":@"false" flags:CRLFAttributeFlagSystem];
    [self _setAttribute:onWifi forKey:@"wifi_connected"];
}

+ (NSMutableDictionary *)mutableCLKSCrashUserInfoDict {
    NSMutableDictionary *newDict;
    NSDictionary *dict = CLKSCrash.sharedInstance.userInfo;
    if (dict == nil) {
        newDict = [NSMutableDictionary dictionary];
    }
    else {
        newDict = [NSMutableDictionary dictionaryWithDictionary:dict];
    }
    return newDict;
}

#pragma mark KSCrash filter/sender

- (void)filterReports:(NSArray<NSDictionary *> *)reports onCompletion:(CLKSCrashReportFilterCompletion)onCompletion {
    self.completionHandler = onCompletion;
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    // Missing thigs:
    // 1. File system free space at crash time - no way to get it async-safe (AND I LOOKED HARD) (but okay with UCH)
    //    Can't watch this one, it changes all the time.
    // 2. Identifier for vendor - can't get this at crash time, but unlikely to change by next launch, and we don't really care if it does.
    //    Can get at pre-crash and post-crash launch.
    // √3. Locale - Not safe at crash time by any reasonable means, but probably doesn't change on app restart?
    //    Register for locale change notification
    // √4. Carrier name - no way at crash time, but probably ok
    //    we can probably log changes here. Def pre- and post-crash readings okay.
    // √5. current radio access technology
    //    ditto 4, but leaning toward pre/post - this could change often
    // √6. Wifi connected
    //    ditto 5
    // √7. Battery level
    //    changes all the time, can monitor with notifications and save globally
    // √8. Battery state
    //    ditto
    // √9. low power mode
    //    can listen for the @"NSProcessInfoPowerStateDidChangeNotification" notification
    //    as long as [NSNotificationCenter defaultCenter] has been called
    // √10.Thermal state
    
    // but this is largely going to have to wait for v1.1.
    [params crlf_safeSetObject:self.apiKey forKey:@"api_key"];
    [params crlf_safeSetObject:self.appParamsJSON forKey:@"app"];
    
    NSMutableArray<NSDictionary *> *occurrences = [NSMutableArray array];
    for (NSDictionary *rawReport in reports)
    {
        NSNumber *preprocessedOccurrence = rawReport[@"preprocessed"];
        if (!preprocessedOccurrence.boolValue) {
            NSDictionary *userInfo = rawReport[@CLKSCrashField_User];
            NSArray *footprintsDicts = userInfo[@"footprints"];
            CRLFCrashReport *crashReport = [[CRLFCrashReport alloc] initWithKSCrashReport:rawReport];
            CRLFEvent *event = [CRLFEvent eventWithCrashReport:crashReport];
            [self addAttributesToEvent:event crashReport:rawReport];
            NSMutableArray *footprints = [NSMutableArray array];
            for (NSDictionary *footprintDict in footprintsDicts) {
                CRLFFootprint *footprint = [CRLFFootprint fromJSONDictionary:footprintDict];
                [footprints addObject:footprint];
            }
            event.footprints = footprints;
            [occurrences addObject:event.occurrenceDict];
        }
        else {
            NSMutableDictionary *preprocessedMinusPreprocessed = rawReport.mutableCopy;
            [preprocessedMinusPreprocessed removeObjectForKey:@"preprocessed"];
            [occurrences addObject:preprocessedMinusPreprocessed];
        }
        
    }
    [params crlf_safeSetObject:occurrences forKey:@"occurrences"];
    [_networkManager POST:@"api/v1/occurrences.json" parameters:params callbackQueue:self.workQueue success:^(id responseObject) {

        CRLFLogExtInfo(@"Report submitted!");
        
        clkscrash_callCompletion(onCompletion, reports, YES, nil);
    } failure:^(NSError *error) {
        CRLFLogExtInfo(@"Error submitting report; Error: %@", [CRLFNSError crlf_debugDescriptionForError:error]);

        clkscrash_callCompletion(onCompletion, @[], NO, error);
    }];

}

- (void)addAttributesToEvent:(CRLFEvent *)event crashReport:(NSDictionary *)rawReport {
    // Get the systm dictionary
    NSDictionary *systemDict = rawReport[@CLKSCrashField_System];
    NSDictionary *userInfo = rawReport[@CLKSCrashField_User];
    CRLFAttribute *freeMemory = [CRLFAttribute attributeWithString:[NSString stringWithFormat:@"%@", systemDict[@CLKSCrashField_Memory][@CLKSCrashField_Free]] flags:CRLFAttributeFlagSystem];
    CRLFAttribute *memorySize = [CRLFAttribute attributeWithString:[NSString stringWithFormat:@"%@", systemDict[@CLKSCrashField_Memory][@CLKSCrashField_Size]] flags:CRLFAttributeFlagSystem];
    CRLFAttribute *usableMmory = [CRLFAttribute attributeWithString:[NSString stringWithFormat:@"%@", systemDict[@CLKSCrashField_Memory][@CLKSCrashField_Usable]] flags:CRLFAttributeFlagSystem];
    CRLFAttribute *osVersion = [CRLFAttribute attributeWithString:systemDict[@CLKSCrashField_OSVersion] flags:CRLFAttributeFlagSystem];
    CRLFAttribute *systemVersion = [CRLFAttribute attributeWithString:systemDict[@CLKSCrashField_SystemVersion] flags:CRLFAttributeFlagSystem];
    CRLFAttribute *deviceMfr = [CRLFAttribute attributeWithString:@"Apple" flags:CRLFAttributeFlagSystem];
    CRLFAttribute *deviceModel = [CRLFAttribute attributeWithString:systemDict[@CLKSCrashField_Machine] flags:CRLFAttributeFlagSystem];
    NSString *identifierForVendor = userInfo[@"device_identifier"];
    CRLFAttribute *identifierForVendorAttr = [[CRLFAttribute alloc] initWithValueType:CRLFAttributeValueTypeString value:identifierForVendor flags:CRLFAttributeFlagSystem];
    CRLFAttribute *appActive = [CRLFAttribute attributeWithString:systemDict[@CLKSCrashField_AppStats][@CLKSCrashField_AppActive] flags:CRLFAttributeFlagSystem];
    CRLFAttribute *appInForeground = [CRLFAttribute attributeWithString:systemDict[@CLKSCrashField_AppStats][@CLKSCrashField_AppInFG] flags:CRLFAttributeFlagSystem];
    CRLFAttribute *jailbroken = [CRLFAttribute attributeWithString:((NSNumber *)systemDict[@CLKSCrashField_Jailbroken]).boolValue?@"true":@"false" flags:CRLFAttributeFlagSystem];
    CRLFAttribute *storage = [CRLFAttribute attributeWithString:[NSString stringWithFormat:@"%@", systemDict[@CLKSCrashField_Storage]] flags:CRLFAttributeFlagSystem];
    

    CRLFMutableAttributes *attributes = [CRLFAttribute mutableAttributesFromJSONDictionary:userInfo[@"attributes"]];
    [attributes crlf_safeSetObject:identifierForVendorAttr forKey:@"device_identifier"];
    [attributes crlf_safeSetObject:freeMemory forKey:@"free_memory_bytes"];
    [attributes crlf_safeSetObject:memorySize forKey:@"total_memory_bytes"];
    [attributes crlf_safeSetObject:usableMmory forKey:@"usable_memory_bytes"];
    [attributes crlf_safeSetObject:osVersion forKey:@"os_build"];
    [attributes crlf_safeSetObject:systemVersion forKey:@"operating_system_version"];
    [attributes crlf_safeSetObject:deviceMfr forKey:@"device_manufacturer"];
    [attributes crlf_safeSetObject:deviceModel forKey:@"device_model"];
    [attributes crlf_safeSetObject:appActive forKey:@"app_is_active"];
    [attributes crlf_safeSetObject:appInForeground forKey:@"app_in_foreground"];
    [attributes crlf_safeSetObject:jailbroken forKey:@"jailbroken"];
    [attributes crlf_safeSetObject:storage forKey:@"total_capacity_bytes"]; // sorry, free storage is not available at crash time - no way to make it async-safe
    
    
    event.attributes = [NSDictionary dictionaryWithDictionary:attributes];
    
    /* system =     {
        CFBundleExecutable = "Crashlife Example";
        CFBundleExecutablePath = "/var/containers/Bundle/Application/BBC49C35-2139-4B17-88CB-43657E54734D/Crashlife Example.app/Crashlife Example";
        CFBundleIdentifier = "com.Buglife.Crashlife-Example";
        CFBundleName = "Crashlife Example";
        CFBundleShortVersionString = "1.0";
        CFBundleVersion = 1;
        "app_start_time" = "2019-03-16T05:42:35Z";
        "app_uuid" = "9FCCA114-A031-38C1-83E7-8214308B39FA";
        "application_stats" =         {
            "active_time_since_last_crash" = 0;
            "active_time_since_launch" = 0;
            "application_active" = 1;
            "application_in_foreground" = 1;
            "background_time_since_last_crash" = 0;
            "background_time_since_launch" = 0;
            "launches_since_last_crash" = 2;
            "sessions_since_last_crash" = 2;
            "sessions_since_launch" = 1;
        };
        "binary_cpu_subtype" = 0;
        "binary_cpu_type" = 16777228;
        "boot_time" = "2019-03-12T07:05:10Z";
        "build_type" = debug;
        "cpu_arch" = arm64;
        "cpu_subtype" = 1;
        "cpu_type" = 16777228;
        "device_app_hash" = b91bee2ae17222fbef924e6288ca34e0d578a2e6;
        jailbroken = 0;
        "kernel_version" = "Darwin Kernel Version 18.2.0: Wed Dec 19 20:28:53 PST 2018; root:xnu-4903.242.2~1/RELEASE_ARM64_T8015";
        machine = "iPhone10,6";
        memory =         {
            free = 90423296;
            size = 2960130048;
            usable = 2025209856;
        };
        model = D221AP;
        "os_version" = 16D57;
        "parent_process_id" = 1;
        "process_id" = 8926;
        "process_name" = "Crashlife Example";
        storage = 255937040384;
        "system_name" = iOS;
        "system_version" = "12.1.4"; √
        "time_zone" = PDT;
    };
*/
}

- (NSDictionary *)appParamsJSON {
    NSDictionary *appInfoJSON = [self.appInfoProvider syncFetchAppInfo].JSONDictionary;
    NSMutableDictionary *mutableAppInfo = [appInfoJSON mutableCopy];
    [mutableAppInfo crlf_safeSetObject:self.sdkName forKey:@"sdk_name"];
    [mutableAppInfo crlf_safeSetObject:self.sdkVersion forKey:@"sdk_version"];
    [mutableAppInfo crlf_safeSetObject:self.platform forKey:@"platform"];
    [mutableAppInfo crlf_safeSetObject:[[NSBundle mainBundle] crlf_buildTypeString] forKey:@"release_stage"];
    return [NSDictionary dictionaryWithDictionary:mutableAppInfo];
}

- (void)postSingleEvent:(CRLFEvent *)event {
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    [params crlf_safeSetObject:self.apiKey forKey:@"api_key"];
    [params crlf_safeSetObject:[self.appInfoProvider syncFetchAppInfo].JSONDictionary forKey:@"app"];
    [params crlf_safeSetObject:self.sdkName forKey:@"sdk_name"];
    [params crlf_safeSetObject:self.sdkVersion forKey:@"sdk_version"];
    [params crlf_safeSetObject:@[event.occurrenceDict] forKey:@"occurrences"];
    [_networkManager POST:@"api/v1/occurrences.json" parameters:params callbackQueue:self.workQueue success:^(id responseObject) {
        
        CRLFLogExtInfo(@"Report submitted!");
        //TODO: anything else? Delete save file if there was one?
    } failure:^(NSError *error) {
        CRLFLogExtInfo(@"Error submitting report; Error: %@", [CRLFNSError crlf_debugDescriptionForError:error]);
        NSDictionary *occurrenceDict = event.occurrenceDict;
        NSMutableDictionary *preprocessed = occurrenceDict.mutableCopy;
        [preprocessed crlf_safeSetObject:@YES forKey:@"preprocessed"];
        NSError *jsonEncodingError = nil;
        NSData *data = [CLKSJSONCodec encode:preprocessed options:CLKSJSONEncodeOptionNone error:&error];
        if (jsonEncodingError != nil) {
            CRLFLogExtError(@"Unable to convert user event to JSON: %@. Please report this to team@buglife.com", jsonEncodingError);
        }
        NSUInteger reportLength = data.length;
        clkscrs_addUserReport(data.bytes, (int)reportLength);
    }];

}


#pragma mark - Attributes and Footprints

- (void)setObjectValue:(nullable id)object forAttribute:(nonnull NSString *)attribute
{
    NSString *value = [object description];
    [self setStringValue:value forAttribute:attribute];
}

- (void)setStringValue:(NSString *)stringValue forAttribute:(NSString *)attributeKey
{
    if (stringValue == nil) {
        [self removeAttribute:attributeKey];
    } else {
        CRLFAttribute *attribute = [[CRLFAttribute alloc] initWithValueType:CRLFAttributeValueTypeString value:stringValue flags:CRLFAttributeFlagCustom];
        [self _setAttribute:attribute forKey:attributeKey];
    }
}

- (void)removeAttribute:(NSString *)attributeKey
{
    if ([attributeKey length] > 0) {
        [_attributes removeObjectForKey:attributeKey];
    }
    NSMutableDictionary *newDict = [CRLFClient mutableCLKSCrashUserInfoDict];
    [newDict removeObjectForKey:attributeKey];
    CLKSCrash.sharedInstance.userInfo = newDict;
    
}

- (void)_setAttribute:(CRLFAttribute *)attribute forKey:(NSString *)attributeKey
{
    if ([attributeKey length] > 0) {
        _attributes[attributeKey] = attribute;
    } else {
        CRLFLogExtError(@"Attempted to set attribute with empty attribute key: \"%@\"", attributeKey);
    }
    NSMutableDictionary *newDict = [CRLFClient mutableCLKSCrashUserInfoDict];
    NSDictionary *attributesDict = newDict[@"attributes"];
    NSMutableDictionary *mutableAttributesDict = [NSMutableDictionary dictionaryWithDictionary:attributesDict];
    
    mutableAttributesDict[attributeKey] = attribute.JSONDictionary;
    newDict[@"attributes"] = mutableAttributesDict;
    CLKSCrash.sharedInstance.userInfo = newDict;
}

- (void)leaveFootprint:(NSString *)name {
    CRLFFootprint *namedFootprint = [[CRLFFootprint alloc] initWithName:name];
    [self.footprints addObject:namedFootprint];
    [self addFootprintToCrashInfo:namedFootprint];
}

- (void)leaveFootprint:(NSString *)name withMetadata:(NSDictionary<NSString *, NSString *> *)metadata {
    CRLFFootprint *footprint = [[CRLFFootprint alloc] initWithName:name attributes:metadata];
    [self.footprints addObject:footprint];
    [self addFootprintToCrashInfo:footprint];
    
}

- (void)addFootprintToCrashInfo:(CRLFFootprint *)footprint {
    NSMutableDictionary *dict = [CRLFClient mutableCLKSCrashUserInfoDict];
    NSArray *footprints = dict[@"footprints"];
    NSArray *newFootprints = [footprints arrayByAddingObject:footprint];
    dict[@"footprints"] = newFootprints;
    CLKSCrash.sharedInstance.userInfo = dict;
}


#pragma mark Log events
- (void)logException:(NSException *)exception {
    CRLFEvent *exceptionEvent = [[CRLFEvent alloc] initWithException:exception attributes:self.attributes footprints:self.footprints];
    [self postSingleEvent:exceptionEvent];
}

- (void)logErrorObject:(NSError *)error {
    NSString *localizedDescription = error.localizedDescription;
    NSString *localizedFailureReason = error.localizedFailureReason;
    NSString *domainAndCode = [NSString stringWithFormat: @"%@ code %ld", error.domain.description, (long)error.code];
    NSString *postedDescription = localizedDescription;
    NSString *postedReason = localizedFailureReason;
    if (localizedDescription.length == 0) {
        postedDescription = domainAndCode;
    } else if (localizedFailureReason.length == 0) {
        postedReason = domainAndCode;
    } else /* neither 0 */ {
        postedReason = [postedReason stringByAppendingFormat:@" %@", domainAndCode];
    }
    postedDescription = [postedDescription stringByAppendingFormat:@"\n%@", postedReason];
    CRLFEvent *errorEvent = [[CRLFEvent alloc] initWithSeverity:CRLFEventSeverityError message:postedDescription attributes:self.attributes footprints:self.footprints];
    [self postSingleEvent:errorEvent];
}

- (void)logError:(NSString *)message {
    CRLFEvent *errorEvent = [[CRLFEvent alloc] initWithSeverity:CRLFEventSeverityError message:message attributes:self.attributes footprints:self.footprints];
    [self postSingleEvent:errorEvent];
}

- (void)logWarning:(NSString *)message {
    CRLFEvent *warningEvent = [[CRLFEvent alloc] initWithSeverity:CRLFEventSeverityWarning message:message attributes:self.attributes footprints:self.footprints];
    [self postSingleEvent:warningEvent];
}

- (void)logInfo:(NSString *)message {
    CRLFEvent *infoEvent = [[CRLFEvent alloc] initWithSeverity:CRLFEventSeverityInfo message:message attributes:self.attributes footprints:self.footprints];
    [self postSingleEvent:infoEvent];
}

@end
