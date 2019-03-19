//
//  CLKSCrashReportSinkVictory.m
//
//  Created by Kelp on 2013-03-14.
//
//  Copyright (c) 2013 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#import "CLKSSystemCapabilities.h"

#if CLKSCRASH_HAS_UIKIT
#import <UIKit/UIKit.h>
#endif
#import "CLKSCrashReportSinkVictory.h"

#import "CLKSHTTPMultipartPostBody.h"
#import "CLKSHTTPRequestSender.h"
#import "NSData+CRLFGZip.h"
#import "CLKSJSONCodecObjC.h"
#import "CLKSReachabilityKSCrash.h"
#import "NSError+CRLFSimpleConstructor.h"
#import "CLKSSystemCapabilities.h"

//#define CLKSLogger_LocalLevel TRACE
#import "CLKSLogger.h"


@interface CLKSCrashReportSinkVictory ()

@property(nonatomic,readwrite,retain) NSURL* url;
@property(nonatomic,readwrite,retain) NSString* userName;
@property(nonatomic,readwrite,retain) NSString* userEmail;

@property(nonatomic,readwrite,retain) CLKSReachableOperationKSCrash* reachableOperation;


@end


@implementation CLKSCrashReportSinkVictory

@synthesize url = _url;
@synthesize userName = _userName;
@synthesize userEmail = _userEmail;
@synthesize reachableOperation = _reachableOperation;

+ (CLKSCrashReportSinkVictory*) sinkWithURL:(NSURL*) url
                                   userName:(NSString*) userName
                                  userEmail:(NSString*) userEmail
{
    return [[self alloc] initWithURL:url userName:userName userEmail:userEmail];
}

- (id) initWithURL:(NSURL*) url
          userName:(NSString*) userName
         userEmail:(NSString*) userEmail
{
    if((self = [super init]))
    {
        self.url = url;
        if (userName == nil || [userName length] == 0) {
#if CLKSCRASH_HAS_UIDEVICE
            self.userName = UIDevice.currentDevice.name;
#else
            self.userName = @"unknown";
#endif
        }
        else {
            self.userName = userName;
        }
        self.userEmail = userEmail;
    }
    return self;
}

- (id <CLKSCrashReportFilter>) defaultCrashReportFilterSet
{
    return self;
}

- (void) filterReports:(NSArray*) reports
          onCompletion:(CLKSCrashReportFilterCompletion) onCompletion
{
    // update user information in reports with KVC
    for (NSDictionary *report in reports) {
        NSDictionary *userDict = [report objectForKey:@"user"];
        if (userDict) {
            // user member is exist
            [userDict setValue:self.userName forKey:@"name"];
            [userDict setValue:self.userEmail forKey:@"email"];
        }
        else {
            // no user member, append user dictionary
            [report setValue:@{@"name": self.userName, @"email": self.userEmail} forKey:@"user"];
        }
    }
    
    NSError* error = nil;
    NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:self.url
                                                           cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                                                       timeoutInterval:15];
    CLKSHTTPMultipartPostBody* body = [CLKSHTTPMultipartPostBody body];
    NSData* jsonData = [CLKSJSONCodec encode:reports
                                   options:CLKSJSONEncodeOptionSorted
                                     error:&error];
    if(jsonData == nil)
    {
        clkscrash_callCompletion(onCompletion, reports, NO, error);
        return;
    }
    
    [body appendData:jsonData
                name:@"reports"
         contentType:@"application/json"
            filename:@"reports.json"];

    // POST http request
    // Content-Type: multipart/form-data; boundary=xxx
    // Content-Encoding: gzip
    request.HTTPMethod = @"POST";
    request.HTTPBody = [[body data] crlf_gzippedWithCompressionLevel:-1 error:nil];
    [request setValue:body.contentType forHTTPHeaderField:@"Content-Type"];
    [request setValue:@"gzip" forHTTPHeaderField:@"Content-Encoding"];
    [request setValue:@"CLKSCrashReporter" forHTTPHeaderField:@"User-Agent"];

    self.reachableOperation = [CLKSReachableOperationKSCrash operationWithHost:[self.url host]
                                                                   allowWWAN:YES
                                                                       block:^
    {
        [[CLKSHTTPRequestSender sender] sendRequest:request
                                        onSuccess:^(__unused NSHTTPURLResponse* response, __unused NSData* data)
         {
             clkscrash_callCompletion(onCompletion, reports, YES, nil);
         } onFailure:^(NSHTTPURLResponse* response, NSData* data)
         {
             NSString* text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
             clkscrash_callCompletion(onCompletion, reports, NO,
                     [NSError                 crlf_errorWithDomain:[[self class] description]
                                              code:response.statusCode
                                       description:text]);
         } onError:^(NSError* error2)
         {
             clkscrash_callCompletion(onCompletion, reports, NO, error2);
         }];
    }];
}

@end
