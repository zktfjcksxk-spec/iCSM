#!/usr/bin/env python3
"""Check actual foreground clock and completion storage without app/user data."""
import pathlib
import subprocess
import tempfile

P = pathlib.Path(__file__).resolve().parents[3]
source = (P / 'app/i5/startup_announcement.inc').read_text()
helpers = source[source.index('static NSString *const ICSMAnnouncementCompletedKey'):source.index('static NSString *const ICSMAnnouncementText')]
fixture = r'''
#import <Foundation/Foundation.h>
#include <cassert>
#include "announcement_clock.h"
'''
cases = r'''
int main(){@autoreleasepool{
    NSString *suite=[@"iCSM.AnnouncementTest." stringByAppendingString:NSUUID.UUID.UUIDString];
    NSUserDefaults *defaults=[[NSUserDefaults alloc] initWithSuiteName:suite];
    NSString *directory=[NSTemporaryDirectory() stringByAppendingPathComponent:suite];
    auto *fm=NSFileManager.defaultManager;
    assert([fm createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:nil]);
    NSString *path=[directory stringByAppendingPathComponent:@"startup-announcement.json"];
    auto write=[&](id value){assert([[NSJSONSerialization dataWithJSONObject:value options:0 error:nil] writeToFile:path atomically:YES]);};
    // Failed/missing imports never consume the first successful-start gate.
    assert(!ICSMHasCompletedStartupAnnouncement(defaults,directory));
    assert(![defaults boolForKey:ICSMAnnouncementCompletedKey]);
    write(@[]);assert(!ICSMHasCompletedStartupAnnouncement(defaults,directory));
    write(@{@"complete":@NO,@"foreground_seconds":@30,@"required_seconds":@20});
    assert(!ICSMHasCompletedStartupAnnouncement(defaults,directory));
    write(@{@"complete":@YES,@"foreground_seconds":@9.99,@"required_seconds":@10});
    assert(!ICSMHasCompletedStartupAnnouncement(defaults,directory));
    write(@{@"complete":@YES,@"foreground_seconds":@20,@"required_seconds":@"invalid"});
    assert(!ICSMHasCompletedStartupAnnouncement(defaults,directory));
    write(@{@"complete":@YES,@"foreground_seconds":@19.99,@"required_seconds":@20});
    assert(!ICSMHasCompletedStartupAnnouncement(defaults,directory));
    // A previous complete 20-second display migrates once, surviving log cleanup.
    write(@{@"complete":@YES,@"foreground_seconds":@20.5,@"required_seconds":@20});
    assert(ICSMHasCompletedStartupAnnouncement(defaults,directory));
    assert([fm removeItemAtPath:path error:nil]);
    assert(ICSMHasCompletedStartupAnnouncement([[NSUserDefaults alloc] initWithSuiteName:suite],directory));
    [defaults removePersistentDomainForName:suite];
    assert(!ICSMHasCompletedStartupAnnouncement(defaults,directory));
    // First new display counts only foreground time and finishes at ten seconds.
    ICSMAnnouncementClock clock;
    static_assert(ICSMAnnouncementClock::requiredSeconds==10.0);
    clock.begin(100,true);clock.advance(104);assert(!clock.complete());
    clock.setActive(105,false);clock.advance(205);assert(clock.elapsed==5 && !clock.complete());
    assert(!ICSMHasCompletedStartupAnnouncement(defaults,directory));
    clock.setActive(205,true);clock.advance(209.99);assert(!clock.complete());
    clock.advance(210);assert(clock.complete() && clock.elapsed==10);
    ICSMMarkStartupAnnouncementCompleted(defaults);
    assert(ICSMHasCompletedStartupAnnouncement([[NSUserDefaults alloc] initWithSuiteName:suite],directory));
    // Current ten-second receipts migrate too; interrupted displays do not.
    [defaults removePersistentDomainForName:suite];
    write(@{@"complete":@YES,@"foreground_seconds":@10,@"required_seconds":@10});
    assert(ICSMHasCompletedStartupAnnouncement(defaults,directory));
    [defaults removePersistentDomainForName:suite];
    assert([fm removeItemAtPath:directory error:nil]);
    puts("PASS: fresh first start, interrupted/retried display, ten foreground seconds, subsequent launch skip, old receipt migration, log cleanup, invalid receipt guards");
}}
'''
with tempfile.TemporaryDirectory(prefix='icsm-announcement-test-') as directory:
    mm, binary = pathlib.Path(directory) / 'announcement.mm', pathlib.Path(directory) / 'announcement'
    mm.write_text(fixture + helpers + cases)
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-fobjc-arc', '-fsanitize=address,undefined',
                    '-I' + str(P / 'app/i5'), '-framework', 'Foundation', str(mm), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
