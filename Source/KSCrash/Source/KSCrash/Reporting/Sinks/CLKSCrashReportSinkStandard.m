//
//  CLKSCrashReportSinkStandard.m
//
//  Created by Karl Stenerud on 2012-02-18.
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
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


#import "CLKSCrashReportSinkStandard.h"

#import "CLKSHTTPMultipartPostBody.h"
#import "CLKSHTTPRequestSender.h"
#import "NSData+CRLFGZip.h"
#import "CLKSJSONCodecObjC.h"
#import "CLKSReachabilityKSCrash.h"
#import "NSError+CRLFSimpleConstructor.h"

//#define CLKSLogger_LocalLevel TRACE
#import "CLKSLogger.h"


@interface CLKSCrashReportSinkStandard ()

@property(nonatomic,readwrite,retain) NSURL* url;

@property(nonatomic,readwrite,retain) CLKSReachableOperationKSCrash* reachableOperation;


@end


@implementation CLKSCrashReportSinkStandard

@synthesize url = _url;
@synthesize reachableOperation = _reachableOperation;

+ (CLKSCrashReportSinkStandard*) sinkWithURL:(NSURL*) url
{
    return [[self alloc] initWithURL:url];
}

- (id) initWithURL:(NSURL*) url
{
    if((self = [super init]))
    {
        self.url = url;
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
    // TODO: Disabled gzip compression until support is added server side,
    // and I've fixed a bug in appendUTF8String.
//    [body appendUTF8String:@"json"
//                      name:@"encoding"
//               contentType:@"string"
//                  filename:nil];

    request.HTTPMethod = @"POST";
    request.HTTPBody = [body data];
    [request setValue:body.contentType forHTTPHeaderField:@"Content-Type"];
    [request setValue:@"CLKSCrashReporter" forHTTPHeaderField:@"User-Agent"];

//    [request setHTTPBody:[[body data] gzippedWithError:nil]];
//    [request setValue:@"gzip" forHTTPHeaderField:@"Content-Encoding"];

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
                     [NSError            errorWithDomain:[[self class] description]
                                         code:response.statusCode
                                     userInfo:[NSDictionary               dictionaryWithObject:text
                                                                          forKey:NSLocalizedDescriptionKey]
                     ]);
         } onError:^(NSError* error2)
         {
             clkscrash_callCompletion(onCompletion, reports, NO, error2);
         }];
    }];
}

@end
