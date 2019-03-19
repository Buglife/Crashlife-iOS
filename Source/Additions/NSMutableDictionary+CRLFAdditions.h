//
//  NSMutableDictionary+CRLFAdditions.h
//  Pods
//
//  Created by David Schukin on 11/26/15.
//  Forked by Daniel DeCovnick on 2/5/19.
//

#import <Foundation/Foundation.h>

void LIFELoadCategoryFor_NSMutableDictionaryCRLFAdditions(void);

@interface NSMutableDictionary<KeyType, ObjectType> (CRLFAdditions)

- (void)crlf_safeSetObject:(ObjectType _Nonnull)object forKey:(KeyType <NSCopying> _Nonnull)key;

@end
