#ifdef __OBJC__
#import <UIKit/UIKit.h>
#else
#ifndef FOUNDATION_EXPORT
#if defined(__cplusplus)
#define FOUNDATION_EXPORT extern "C"
#else
#define FOUNDATION_EXPORT extern
#endif
#endif
#endif

#import "Crashlife.h"
#import "CRLFClient.h"
#import "CRLFEvent.h"
#import "CRLFNetworkManager.h"

FOUNDATION_EXPORT double CrashlifeVersionNumber;
FOUNDATION_EXPORT const unsigned char CrashlifeVersionString[];

