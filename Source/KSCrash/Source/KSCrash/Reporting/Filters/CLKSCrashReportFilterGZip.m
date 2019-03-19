//
//  CLKSCrashReportFilterGZip.m
//
//  Created by Karl Stenerud on 2012-05-10.
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


#import "CLKSCrashReportFilterGZip.h"
#import "NSData+CRLFGZip.h"


@interface CLKSCrashReportFilterGZipCompress ()

@property(nonatomic,readwrite,assign) int compressionLevel;

@end

@implementation CLKSCrashReportFilterGZipCompress

@synthesize compressionLevel = _compressionLevel;

+ (CLKSCrashReportFilterGZipCompress*) filterWithCompressionLevel:(int) compressionLevel
{
    return [[self alloc] initWithCompressionLevel:compressionLevel];
}

- (id) initWithCompressionLevel:(int) compressionLevel
{
    if((self = [super init]))
    {
        self.compressionLevel = compressionLevel;
    }
    return self;
}

- (void) filterReports:(NSArray*) reports
          onCompletion:(CLKSCrashReportFilterCompletion) onCompletion
{
    NSMutableArray* filteredReports = [NSMutableArray arrayWithCapacity:[reports count]];
    for(NSData* report in reports)
    {
        NSError* error = nil;
        NSData* compressedData = [report crlf_gzippedWithCompressionLevel:self.compressionLevel
                                                                    error:&error];
        if(compressedData == nil)
        {
            clkscrash_callCompletion(onCompletion, filteredReports, NO, error);
            return;
        }
        else
        {
            [filteredReports addObject:compressedData];
        }
    }

    clkscrash_callCompletion(onCompletion, filteredReports, YES, nil);
}

@end


@implementation CLKSCrashReportFilterGZipDecompress

+ (CLKSCrashReportFilterGZipDecompress*) filter
{
    return [[self alloc] init];
}

- (void) filterReports:(NSArray*) reports
          onCompletion:(CLKSCrashReportFilterCompletion) onCompletion
{
    NSMutableArray* filteredReports = [NSMutableArray arrayWithCapacity:[reports count]];
    for(NSData* report in reports)
    {
        NSError* error = nil;
        NSData* decompressedData = [report crlf_gunzippedWithError:&error];
        if(decompressedData == nil)
        {
            clkscrash_callCompletion(onCompletion, filteredReports, NO, error);
            return;
        }
        else
        {
            [filteredReports addObject:decompressedData];
        }
    }

    clkscrash_callCompletion(onCompletion, filteredReports, YES, nil);
}

@end
