//
//  NSMutableDictionary+CRLFAdditions.m
//  Pods
//
//  Created by David Schukin on 11/26/15.
//  Forked by Daniel DeCovnick on 2/5/19.
//

#import "NSMutableDictionary+CRLFAdditions.h"

@implementation NSMutableDictionary (CRLFAdditions)

+ (void)crlf_loadCategory_NSMutableDictionaryCRLFAdditions { }

- (void)crlf_safeSetObject:(id)object forKey:(id<NSCopying>)key
{
    if (object) {
        [self setObject:object forKey:key];
    }
}

@end

void CRLFLoadCategoryFor_NSMutableDictionaryCRLFAdditions() {
    [NSMutableDictionary crlf_loadCategory_NSMutableDictionaryCRLFAdditions];
}
