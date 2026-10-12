#import "diagnostics_export.h"
#import <CommonCrypto/CommonDigest.h>
#include "../../vendor/minizip-1.3.1/zip.h"
#include <sys/stat.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <unistd.h>

static const NSUInteger LogLimit=2*1024*1024;
static NSError *diagnosticError(NSString *message) {
 return [NSError errorWithDomain:@"iCSM.Diagnostics" code:1 userInfo:@{NSLocalizedDescriptionKey:message}];
}
static NSString *stamp() {
 NSDateFormatter *f=[NSDateFormatter new];f.locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
 f.timeZone=[NSTimeZone timeZoneForSecondsFromGMT:0];f.dateFormat=@"yyyy-MM-dd'T'HH:mm:ss.SSS'Z'";
 return [f stringFromDate:NSDate.date];
}
static NSString *redact(NSString *text) {
 // Omit complete credential-bearing lines, including quoted / JSON-escaped
 // console commands. The raw command mailbox and configs are never exported.
 NSRegularExpression *secrets=[NSRegularExpression regularExpressionWithPattern:@"(?im)^.*\\b(?:password|rcon_password|sv_password|token|authorization|auth_ticket)\\b[^\\r\\n]*" options:0 error:nil];
 text=[secrets stringByReplacingMatchesInString:text options:0 range:NSMakeRange(0,text.length) withTemplate:@"[credential line removed]"];
 NSRegularExpression *name=[NSRegularExpression regularExpressionWithPattern:@"(?im)^.*(?:^|[; >])name[ \\t]+[^\\r\\n]*" options:0 error:nil];
 text=[name stringByReplacingMatchesInString:text options:0 range:NSMakeRange(0,text.length) withTemplate:@"[player name command removed]"];
 return [text stringByReplacingOccurrencesOfString:NSHomeDirectory() withString:@"<app-container>"];
}
static id safeJSON(id value, NSUInteger depth=0) {
 if(depth>16)return @"[depth limit]";
 if([value isKindOfClass:NSString.class])return redact(value);
 if([value isKindOfClass:NSDictionary.class]) {
  NSMutableDictionary *copy=[NSMutableDictionary dictionary];
  for(NSString *key in value) {
   NSString *lower=key.lowercaseString;
   if([lower containsString:@"password"] || [lower containsString:@"token"] || [lower containsString:@"ticket"] ||
      [lower isEqualToString:@"name"] || [lower containsString:@"nickname"] || [lower isEqualToString:@"command"] || [lower isEqualToString:@"authorization"])continue;
   copy[key]=safeJSON(value[key],depth+1);
  }return copy;
 }
 if([value isKindOfClass:NSArray.class]) {
  NSMutableArray *copy=[NSMutableArray array];for(id row in value)[copy addObject:safeJSON(row,depth+1)];return copy;
 }
 return value?:NSNull.null;
}
static NSData *jsonData(id value) {return [NSJSONSerialization dataWithJSONObject:safeJSON(value) options:NSJSONWritingPrettyPrinted error:nil];}
static NSData *boundedRead(NSString *path, BOOL tail) {
 int fd=open(path.fileSystemRepresentation,O_RDONLY|O_NOFOLLOW);if(fd<0)return nil;
 struct stat s={};if(fstat(fd,&s)!=0 || !S_ISREG(s.st_mode) || (!tail && s.st_size>(off_t)LogLimit)){close(fd);return nil;}
 off_t start=tail?MAX((off_t)0,s.st_size-(off_t)LogLimit):0;
 NSMutableData *data=[NSMutableData dataWithLength:(NSUInteger)MIN(s.st_size,(off_t)LogLimit)];
 ssize_t count=pread(fd,data.mutableBytes,data.length,start);close(fd);if(count<0)return nil;data.length=(NSUInteger)count;
 if(start>0) {
  const char *bytes=(const char*)data.bytes;NSUInteger n=0;while(n<data.length && bytes[n]!='\n')++n;
  if(n<data.length)[data replaceBytesInRange:NSMakeRange(0,n+1) withBytes:nullptr length:0];
 }
 return data;
}
static NSDictionary *environment() {
 struct utsname system={};uname(&system);NSDictionary *info=NSBundle.mainBundle.infoDictionary;
 return @{@"created_utc":stamp(),@"pid":@(getpid()),@"hardware":@(system.machine),
  @"os_version":NSProcessInfo.processInfo.operatingSystemVersionString,
  @"app_version":info[@"CFBundleShortVersionString"]?:@"",@"app_build":info[@"CFBundleVersion"]?:@"",@"bundle_id":NSBundle.mainBundle.bundleIdentifier?:@""};
}
BOOL ICSMDiagnosticsBeginSession(NSString *documents,NSString *evidence,NSError **error) {
 NSFileManager *fm=NSFileManager.defaultManager;
 if(![fm createDirectoryAtPath:evidence withIntermediateDirectories:YES attributes:nil error:error])return NO;
 NSMutableDictionary *paths=[NSMutableDictionary dictionary];
 for(NSString *name in @[@"client.log",@"status.json",@"session.json",@"startup-error.json",@"startup-progress.json",@"import-diagnostics.json"])
  paths[name]=[evidence stringByAppendingPathComponent:name];
 paths[@"console.log"]=[documents stringByAppendingPathComponent:@"runtime-i5/csgo/i5_console.log"];
 for(NSString *name in paths) {
  NSString *previous=[evidence stringByAppendingPathComponent:[@"previous-" stringByAppendingString:name]];
  NSString *older=[evidence stringByAppendingPathComponent:[@"older-" stringByAppendingString:name]];
  [fm removeItemAtPath:older error:nil];
  if([fm fileExistsAtPath:previous] && ![fm moveItemAtPath:previous toPath:older error:error])return NO;
  NSString *current=paths[name];
  if([fm fileExistsAtPath:current] && ![fm moveItemAtPath:current toPath:previous error:error])return NO;
 }
 return [jsonData(environment()) writeToFile:[evidence stringByAppendingPathComponent:@"session.json"] options:NSDataWritingAtomic error:error];
}
static NSDictionary *errorDetails(NSError *error,NSUInteger depth=0) {
 if(!error || depth>4)return @{};
 NSMutableDictionary *details=[@{@"domain":error.domain,@"code":@(error.code),@"description":error.localizedDescription?:@""} mutableCopy];
 for(NSString *key in @[NSLocalizedFailureReasonErrorKey,NSLocalizedRecoverySuggestionErrorKey,NSFilePathErrorKey,@"shader_file",@"stage",@"import_details"])if(error.userInfo[key])details[key]=error.userInfo[key];
 NSError *underlying=error.userInfo[NSUnderlyingErrorKey];if([underlying isKindOfClass:NSError.class])details[@"underlying"]=errorDetails(underlying,depth+1);
 return details;
}
void ICSMDiagnosticsRecordError(NSString *evidence,NSString *stage,NSError *error) {
 NSMutableDictionary *record=[environment() mutableCopy];record[@"stage"]=stage?:@"unknown";record[@"error"]=errorDetails(error);
 [jsonData(record) writeToFile:[evidence stringByAppendingPathComponent:@"startup-error.json"] atomically:YES];
 fprintf(stderr,"ICSM_ERROR stage=%s domain=%s code=%ld description=%s\n",stage.UTF8String,error.domain.UTF8String,(long)error.code,redact(error.localizedDescription?:@"").UTF8String);
}
void ICSMDiagnosticsSaveSystemReport(NSString *evidence,NSData *json) {
 if(!json || json.length>LogLimit)return;
 id object=[NSJSONSerialization JSONObjectWithData:json options:0 error:nil];if(!object)return;
 NSString *directory=[evidence stringByAppendingPathComponent:@"system-reports"];NSFileManager *fm=NSFileManager.defaultManager;
 [fm createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:nil];
 unsigned char hash[CC_SHA256_DIGEST_LENGTH];CC_SHA256(json.bytes,(CC_LONG)json.length,hash);
 NSMutableString *name=[NSMutableString stringWithString:@"report-"];for(int i=0;i<12;++i)[name appendFormat:@"%02x",hash[i]];[name appendString:@".json"];
 NSString *path=[directory stringByAppendingPathComponent:name];
 if(![fm fileExistsAtPath:path])[jsonData(object) writeToFile:path atomically:YES];
 NSArray *files=[fm contentsOfDirectoryAtPath:directory error:nil];
 NSArray *sorted=[files sortedArrayUsingComparator:^NSComparisonResult(NSString *a,NSString *b){
  NSDate *da=[fm attributesOfItemAtPath:[directory stringByAppendingPathComponent:a] error:nil][NSFileModificationDate];
  NSDate *db=[fm attributesOfItemAtPath:[directory stringByAppendingPathComponent:b] error:nil][NSFileModificationDate];return [db compare:da];
 }];for(NSUInteger i=5;i<sorted.count;++i)[fm removeItemAtPath:[directory stringByAppendingPathComponent:sorted[i]] error:nil];
}
static NSData *statusSummary(NSData *data) {
 NSDictionary *status=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:nil]:nil;
 if(![status isKindOfClass:NSDictionary.class])return nil;
 NSMutableDictionary *safe=[NSMutableDictionary dictionary];
 for(NSString *key in @[@"app_version",@"app_build",@"pid",@"updated_utc",@"client_result",@"physical_footprint",@"foreground",@"host_ready",@"video_state",@"metal",@"startup",@"screen_policy",@"loaded_images"])
  if(status[key])safe[key]=status[key];
 return jsonData(safe);
}
NSURL *ICSMDiagnosticsCreateArchive(NSString *documents,NSString *evidence,NSError **error) {
 @autoreleasepool {
  fflush(stdout);fflush(stderr);
  NSFileManager *fm=NSFileManager.defaultManager;NSString *directory=[documents stringByAppendingPathComponent:@"Diagnostics"];
  if(![fm createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:error])return nil;
  NSString *filename=[NSString stringWithFormat:@"iCSM-logs-%@-%@.zip",[stamp() stringByReplacingOccurrencesOfString:@":" withString:@"-"],NSUUID.UUID.UUIDString];
  NSString *path=[directory stringByAppendingPathComponent:filename],*partial=[path stringByAppendingString:@".partial"];
  NSMutableDictionary<NSString*,NSData*> *entries=[NSMutableDictionary dictionary];NSMutableArray *omitted=[NSMutableArray array];
  entries[@"environment.json"]=jsonData(environment());
  entries[@"README.txt"]=[@"iCSM 报错日志 / Diagnostic logs\n当前运行以及之前两次运行的日志（每个文本日志最多最后 2 MiB）。\n包含版本、系统、机型、画面与 Metal 状态、加载失败详情，以及系统已提供的 MetricKit 诊断。\n未导出配置、命令文件、存档、数据包；常见密码命令已过滤。日志仍可能包含玩家及服务器信息，分享前可自行检查。\n保留上次运行不代表已经判定其发生闪退。系统未提供崩溃诊断时，此包不含完整崩溃堆栈。\nFull system crash reports may also be obtained from iOS Settings > Privacy & Security > Analytics & Improvements > Analytics Data.\n" dataUsingEncoding:NSUTF8StringEncoding];
  for(NSString *prefix in @[@"",@"previous-",@"older-"])for(NSString *file in @[@"client.log",@"console.log",@"status.json",@"session.json",@"startup-error.json",@"startup-progress.json",@"import-diagnostics.json"]) {
   NSString *name=[prefix stringByAppendingString:file];
   NSString *source=(prefix.length==0 && [file isEqualToString:@"console.log"])?[documents stringByAppendingPathComponent:@"runtime-i5/csgo/i5_console.log"]:[evidence stringByAppendingPathComponent:name];
   BOOL log=[file hasSuffix:@".log"];NSData *data=boundedRead(source,log);if(!data){[omitted addObject:name];continue;}
   if(log) {
    // Source console output can contain non-UTF8 bytes; replace those through
    // the total Latin-1 decoder rather than dropping the entire crash log.
    NSString *text=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]?:[[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding];
    data=[redact(text?:@"") dataUsingEncoding:NSUTF8StringEncoding];
   } else if([file isEqualToString:@"status.json"])data=statusSummary(data);
   else {id object=[NSJSONSerialization JSONObjectWithData:data options:0 error:nil];data=object?jsonData(object):nil;}
   if(data)entries[name]=data;else [omitted addObject:name];
  }
  // Calibration contains timing/GPU samples, never the recovery preferences.
  NSData *calibration=boundedRead([evidence stringByAppendingPathComponent:@"frame-calibration.json"],NO);
  id calibrationJSON=calibration?[NSJSONSerialization JSONObjectWithData:calibration options:0 error:nil]:nil;
  if([calibrationJSON isKindOfClass:NSDictionary.class])entries[@"frame-calibration.json"]=jsonData(calibrationJSON);
  NSString *reports=[evidence stringByAppendingPathComponent:@"system-reports"];
  NSUInteger count=0;for(NSString *name in [[fm contentsOfDirectoryAtPath:reports error:nil] sortedArrayUsingSelector:@selector(compare:)]) {
   if(++count>5)break;if(![name hasPrefix:@"report-"] || ![name hasSuffix:@".json"])continue;
   NSData *data=boundedRead([reports stringByAppendingPathComponent:name],NO);id object=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:nil]:nil;
   if(object)entries[[@"system-reports/" stringByAppendingString:name]]=jsonData(object);
  }
  NSData *manifest=[NSData dataWithContentsOfFile:[NSBundle.mainBundle pathForResource:@"distribution-assets" ofType:@"json"]?:@""];
  NSDictionary *asset=manifest?[NSJSONSerialization JSONObjectWithData:manifest options:0 error:nil]:nil;
  entries[@"export.json"]=jsonData(@{@"omitted_or_unavailable":omitted,@"system_reports":@(count),@"log_tail_limit_bytes":@(LogLimit),@"data_identity":asset[@"identity"]?:@"",@"data_files":@([asset[@"files"] count])});
  zipFile archive=zipOpen64(partial.fileSystemRepresentation,APPEND_STATUS_CREATE);BOOL ok=archive!=nullptr;
  if(ok)for(NSString *name in [[entries allKeys] sortedArrayUsingSelector:@selector(compare:)]) {
   NSData *data=entries[name];zip_fileinfo info={};
   if(zipOpenNewFileInZip64(archive,name.UTF8String,&info,nullptr,0,nullptr,0,nullptr,Z_DEFLATED,6,0)!=ZIP_OK){ok=NO;break;}
   int write=zipWriteInFileInZip(archive,data.bytes,(unsigned)data.length),close=zipCloseFileInZip(archive);
   if(write!=ZIP_OK || close!=ZIP_OK){ok=NO;break;}
  }
  if(archive && zipClose(archive,nullptr)!=ZIP_OK)ok=NO;
  if(ok)ok=[fm moveItemAtPath:partial toPath:path error:error];
  if(!ok){[fm removeItemAtPath:partial error:nil];if(error && !*error)*error=diagnosticError(@"报错日志打包失败，请检查设备剩余空间。");return nil;}
  // Keep three finished exports, never grow into another game data package.
  NSArray *exports=[[[fm contentsOfDirectoryAtPath:directory error:nil] filteredArrayUsingPredicate:[NSPredicate predicateWithFormat:@"SELF BEGINSWITH 'iCSM-logs-' AND SELF ENDSWITH '.zip'"]] sortedArrayUsingSelector:@selector(compare:)];
  for(NSUInteger i=0;i+3<exports.count;++i)[fm removeItemAtPath:[directory stringByAppendingPathComponent:exports[i]] error:nil];
  return [NSURL fileURLWithPath:path];
 }
}
