//
//  CLKSCrashDoctor.h
//  CLKSCrash
//
//  Created by Karl Stenerud on 2012-11-10.
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface CLKSCrashDoctor : NSObject

+ (CLKSCrashDoctor*) doctor;

- (NSString*) diagnoseCrash:(NSDictionary*) crashReport;

@end
