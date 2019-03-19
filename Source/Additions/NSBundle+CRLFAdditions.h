//
//  NSBundle+CRLFAdditions.h
//  Crashlife
//
//  Created by Daniel DeCovnick on 3/18/19.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSBundle (CRLFAdditions)
// Behavior is undefined if not invoked on -mainBundle
- (BOOL)crlf_isAppStoreBuild;
- (BOOL)crlf_isTestFlightBuild;
- (BOOL)crlf_isEnterpriseBuild;
- (BOOL)crlf_isAdHocDistributionBuild;
- (BOOL)crlf_isDevBuild;
- (NSString *)crlf_buildTypeString;
@end

NS_ASSUME_NONNULL_END
