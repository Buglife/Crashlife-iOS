//
//  NSBundle+CRLFAdditions.m
//  Crashlife
//
//  Created by Daniel DeCovnick on 3/18/19.
//

#import "NSBundle+CRLFAdditions.h"

@implementation NSBundle (CRLFAdditions)
- (NSURL *)crlf_embeddedMobileProvision
{
    return [self URLForResource:@"embedded" withExtension:@"mobileprovision"];
}

- (NSDictionary *)crlf_embeddedMobileProvisionPlist
{
    static NSDictionary *plist = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSURL *embedded = [self crlf_embeddedMobileProvision];
        if (!embedded)
        {
            return;
        }
        // HACK
        // TODO FIXME with DTFoundation/DTASN1 later
        // This file is ASN.1 format, with the human-readable part as
        // (ironically) an octet stream
        NSError *readError = nil;
        NSData *mpData = [NSData dataWithContentsOfURL:embedded options:kNilOptions error:&readError];
        if (readError != nil)
        {
            return;
        }
        NSData *startPlist = [@"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">" dataUsingEncoding:NSUTF8StringEncoding];
        NSRange startPlistRange = [mpData rangeOfData:startPlist options:kNilOptions range:NSMakeRange(0, mpData.length)];
        if (startPlistRange.location == NSNotFound)
        {
            return;
        }
        NSData *endPlist = [@"</plist>" dataUsingEncoding:NSUTF8StringEncoding];
        NSRange endPlistRange = [mpData rangeOfData:endPlist options:kNilOptions range:NSMakeRange(0, mpData.length)];
        if (endPlistRange.location == NSNotFound)
        {
            return;
        }
        if (mpData.length < endPlistRange.location + endPlistRange.length)
        {
            return; //WTF happened?
        }
        NSData *plistData = [mpData subdataWithRange:NSMakeRange(startPlistRange.location, endPlistRange.location+endPlistRange.length-startPlistRange.location)];
        NSError *plistError = nil;
        plist = [NSPropertyListSerialization propertyListWithData:plistData options:NSPropertyListImmutable format:nil error:&plistError];
        if (plistError)
        {
            return;
        }
    });
    return plist;
}

- (BOOL)crlf_isAppStoreBuild
{
    return [self crlf_embeddedMobileProvision] == nil && ![self crlf_isTestFlightBuild];
}

- (BOOL)crlf_isTestFlightBuild
{
    return [[[self appStoreReceiptURL] lastPathComponent] isEqualToString:@"sandboxReceipt"] && ![self crlf_isDevBuild] && ![self crlf_isEnterpriseBuild];
}

- (BOOL)crlf_isEnterpriseBuild
{
    NSDictionary *plist = [self crlf_embeddedMobileProvisionPlist];
    return [(NSNumber *)[plist objectForKey:@"ProvisionsAllDevices"] boolValue];
}

- (BOOL)crlf_isAdHocDistributionBuild
{
    NSDictionary *plist = [self crlf_embeddedMobileProvisionPlist];
    NSDictionary *entitlements = plist[@"Entitlements"];
    if ([(NSNumber *)[entitlements objectForKey:@"get-task-allow"] boolValue] == NO && ((NSDictionary *)plist[@"ProvisionedDevices"]).count > 0)
    {
        return YES;
    }
    return NO;
}

- (BOOL)crlf_isDevBuild
{
    NSDictionary *plist = [self crlf_embeddedMobileProvisionPlist];
    NSDictionary *entitlements = plist[@"Entitlements"];
    return [(NSNumber *)[entitlements objectForKey:@"get-task-allow"] boolValue];
}
- (NSString *)crlf_buildTypeString {
    if (self.crlf_isDevBuild) {
        return @"development";
    }
    if (self.crlf_isAdHocDistributionBuild) {
        return @"ad_hoc";
    }
    if (self.crlf_isEnterpriseBuild) {
        return @"enterprise";
    }
    if (self.crlf_isTestFlightBuild) {
        return @"test_flight";
    }
    if (self.crlf_isAppStoreBuild) {
        return @"app_store";
    }
    return @"unknown";
}
@end
