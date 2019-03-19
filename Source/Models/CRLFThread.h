//
//  CRLFThread.h
//  Crashlife
//
//  Created by Daniel DeCovnick on 2/7/19.
//

#import <Foundation/Foundation.h>

@class CRLFStackFrame;

NS_ASSUME_NONNULL_BEGIN

@interface CRLFThread : NSObject
// Constructs a thread from a crash report subdictionary
- (instancetype)initWithKSDictionary:(NSDictionary *)dictionary;

// Construct a fake thread from a real backtrace, probably a caught exception.
- (instancetype)initWithBacktrace:(NSArray<CRLFStackFrame *> *)backtrace;

//Only call this with a fake thread from a real backtrace in the current process
//It *might* work on another run, but all the images might not be loaded on the 2nd run
- (NSArray *)denormalizedThreadInCurrentProcess;
// Thread number
@property (nonatomic, readonly) NSUInteger index;
@property (nonatomic, readonly) BOOL crashed;
@property (nonatomic, readonly) BOOL backtraceSkipped;
@property (nonatomic, readonly) NSArray<CRLFStackFrame *> *backtrace;
@property (nonatomic, readonly) BOOL currentThread;

//Queue name
@property (nonatomic, nullable, readonly) NSString *dispatchQueue;
//not filled: notable addresses, stack dump
// TODO: we should probably fill at least noteable addresses; maybe put them each into an attribute?
@end

NS_ASSUME_NONNULL_END
