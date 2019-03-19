//
// Created by Daniel DeCovnick on 2019-02-06.
// Copyright (c) 2019 Buglife, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class CRLFAttribute;
@class CRLFCrashReport;
@class CRLFFootprint;

typedef NSString * CRLFEventSeverity NS_STRING_ENUM;
extern CRLFEventSeverity const CRLFEventSeverityInfo;
extern CRLFEventSeverity const CRLFEventSeverityWarning;
extern CRLFEventSeverity const CRLFEventSeverityError;
extern CRLFEventSeverity const CRLFEventSeverityCrash;

@interface CRLFEvent : NSObject
@property (nonatomic, nullable, readonly) CRLFCrashReport *crashReport;
@property (nonatomic, nonnull, copy) CRLFEventSeverity severity;
@property (nonatomic, readonly) NSDictionary *occurrenceDict;
@property (nonatomic, nonnull) NSDictionary<NSString *, CRLFAttribute *> *attributes;
@property (nonatomic, nonnull) NSArray<CRLFFootprint *> *footprints;
@property (nonatomic, nullable) NSString *timestampString;
+ (instancetype)eventWithCrashReport:(CRLFCrashReport *)crashReport;

- (instancetype)initWithSeverity:(CRLFEventSeverity)severity message:(NSString *)message attributes:(NSDictionary<NSString *, CRLFAttribute *> *)attributes footprints:(NSArray *)footprints;
- (instancetype)initWithException:(NSException *)exception attributes:(NSDictionary<NSString *, CRLFAttribute *> *)attributes footprints:(NSArray *)footprints;
@end
