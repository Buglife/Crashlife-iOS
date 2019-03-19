//
//  NSError+LIFEAdditions.h
//  Pods
//
//  Created by David Schukin on 12/9/15.
//
//

#import <Foundation/Foundation.h>

@interface CRLFNSError : NSObject

// Provides an informative user-facing error description.
// The purpose of this is to provide sufficient info
// that they can either debug themselves, or email customer support.
+ (NSString *)crlf_debugDescriptionForError:(NSError *)error;

#pragma mark - Factory methods

+ (NSError *)crlf_errorWithHTTPURLResponse:(NSHTTPURLResponse *)httpResponse;

@end
