#import "data_install.h"
#import <CommonCrypto/CommonDigest.h>
#import <Metal/Metal.h>
#include "../../vendor/minizip-1.3.1/unzip.h"
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <set>
#include <map>
#include <string>

static BOOL fail(NSError *__strong *error, NSString *message) {
    if(error)*error=[NSError errorWithDomain:@"iCSM.Data" code:1
                                  userInfo:@{NSLocalizedDescriptionKey:message}];
    return NO;
}
static NSString *hexDigest(const unsigned char *digest) {
    NSMutableString *hex=[NSMutableString string];
    for(unsigned i=0;i<32;++i)[hex appendFormat:@"%02x",digest[i]];
    return hex;
}
static BOOL safeName(NSString *name) {
    if(!name.length || [name hasPrefix:@"/"] || [name containsString:@"\\"] ||
       [name containsString:@":"] || [name rangeOfCharacterFromSet:[NSCharacterSet characterSetWithRange:NSMakeRange(0,1)]].location!=NSNotFound)return NO;
    for(NSString *part in [name componentsSeparatedByString:@"/"])
        if(!part.length || [part isEqual:@".."] || [part isEqual:@"."])return NO;
    return YES;
}
// Never follow links placed in the File Sharing directory by another app.
static BOOL safeParents(NSString *base, NSString *relative, NSError *__strong *error) {
    NSString *path=base;
    NSArray *parts=[relative componentsSeparatedByString:@"/"];
    for(NSUInteger i=0;i<parts.count;++i) {
        path=[path stringByAppendingPathComponent:parts[i]];
        struct stat s={};
        if(lstat(path.fileSystemRepresentation,&s)==0) {
            if(S_ISLNK(s.st_mode) || (i+1<parts.count && !S_ISDIR(s.st_mode)))
                return fail(error,@"数据目录包含符号链接或无效目录，请重新导入数据包。");
        }else if(errno!=ENOENT)return fail(error,@"无法读取数据目录。");
    }
    return YES;
}
static NSDictionary *stamp(NSString *path) {
    struct stat s={};
    if(lstat(path.fileSystemRepresentation,&s)!=0 || !S_ISREG(s.st_mode))return nil;
    return @{@"size":@(s.st_size),@"inode":@(s.st_ino),
             @"mtime_s":@(s.st_mtimespec.tv_sec),@"mtime_ns":@(s.st_mtimespec.tv_nsec)};
}
static NSString *targetPath(NSString *name,NSString *documents,NSString *cache) {
    if([name hasPrefix:@"shader-cache/"])
        return [[documents stringByAppendingPathComponent:@"game-assets/icsm-shaders"] stringByAppendingPathComponent:[name substringFromIndex:13]];
    return [documents stringByAppendingPathComponent:name];
}
static NSString *storageName(NSString *name) {
    return [name hasPrefix:@"shader-cache/"]?[@"game-assets/icsm-shaders/" stringByAppendingString:[name substringFromIndex:13]]:name;
}
static BOOL writeJSON(id value,NSString *path,NSError *__strong *error) {
    NSError *local=nil;
    NSData *data=[NSJSONSerialization dataWithJSONObject:value options:0 error:&local];
    BOOL ok=data && [data writeToFile:path options:NSDataWritingAtomic error:&local];
    if(!ok && error)*error=local;return ok;
}
static NSString *hashFile(NSString *path,ICSMInstallProgress progress,
                          double start,double span,unsigned long long size) {
    FILE *file=fopen(path.fileSystemRepresentation,"rb");if(!file)return nil;
    CC_SHA256_CTX ctx;CC_SHA256_Init(&ctx);
    std::vector<unsigned char> buffer(1024*1024);unsigned long long read=0;
    size_t count;while((count=fread(buffer.data(),1,buffer.size(),file))) {
        CC_SHA256_Update(&ctx,buffer.data(),(CC_LONG)count);read+=count;
        if(progress)progress([NSString stringWithFormat:@"正在检查游戏数据\n%@",path.lastPathComponent],start+span*double(read)/double(size?:1));
    }
    BOOL ok=!ferror(file);fclose(file);unsigned char digest[32];CC_SHA256_Final(digest,&ctx);
    return ok && read==size?hexDigest(digest):nil;
}
NSDictionary *ICSMDistributionManifest(void) {
    NSString *path=[NSBundle.mainBundle pathForResource:@"distribution-assets" ofType:@"json"];
    NSData *data=path?[NSData dataWithContentsOfFile:path]:nil;
    return data?[NSJSONSerialization JSONObjectWithData:data options:0 error:nil]:nil;
}

static NSMutableDictionary *installStep(NSMutableDictionary *context,NSString *phase,NSString *file=nil) {
    context[@"phase"]=phase;
    if(file)context[@"current_file"]=file;else [context removeObjectForKey:@"current_file"];
    NSMutableDictionary *operation=[NSMutableDictionary dictionary];context[@"operation"]=operation;return operation;
}
static NSError *posixError(int code,NSString *path) {
    return code?[NSError errorWithDomain:NSPOSIXErrorDomain code:code userInfo:path?@{NSFilePathErrorKey:path}:@{}]:nil;
}
static BOOL prepareInstallation(NSString *documents,NSString *cache,NSDictionary *manifest,
                               NSString *receiptPath,ICSMInstallProgress progress,NSError *__strong *error,
                               NSMutableDictionary *context,NSURL *selectedArchive) {
    NSFileManager *fm=NSFileManager.defaultManager;
    installStep(context,@"validate_manifest");
    if(![manifest[@"version"] isEqual:@1] || ![manifest[@"files"] isKindOfClass:NSArray.class] ||
       ![manifest[@"identity"] isKindOfClass:NSString.class])return fail(error,@"应用数据清单无效，请重新编译应用。");
    NSMutableDictionary *expected=[NSMutableDictionary dictionary];
    unsigned long long total=0;
    for(NSDictionary *entry in manifest[@"files"]) {
        NSString *name=entry[@"path"];
        if(![name isKindOfClass:NSString.class] || !safeName(name) || expected[name] ||
           (![name hasPrefix:@"game-assets/"] && ![name hasPrefix:@"shader-cache/"]) ||
           ![entry[@"sha256"] isKindOfClass:NSString.class] || [entry[@"sha256"] length]!=64 ||
           ![entry[@"size"] isKindOfClass:NSNumber.class])return fail(error,@"应用数据清单格式错误。");
        expected[name]=entry;total+=[entry[@"size"] unsignedLongLongValue];
    }
    if(!expected.count)return fail(error,@"应用数据清单为空。");
    context[@"data_identity"]=manifest[@"identity"];context[@"expected_files"]=@(expected.count);context[@"expected_bytes"]=@(total);
    for(NSString *directory in @[documents,cache,receiptPath.stringByDeletingLastPathComponent]) {
        installStep(context,@"create_directories",directory);NSError *local=nil;
        if(![fm createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:&local]){if(error)*error=local;return NO;}
    }
    installStep(context,@"read_receipts",receiptPath);
    NSData *old=[NSData dataWithContentsOfFile:receiptPath];
    NSDictionary *decoded=old?[NSJSONSerialization JSONObjectWithData:old options:0 error:nil]:nil;
    NSMutableDictionary *receipts=([decoded[@"identity"] isEqual:manifest[@"identity"]] &&
        [decoded[@"files"] isKindOfClass:NSDictionary.class])?[decoded[@"files"] mutableCopy]:[NSMutableDictionary dictionary];
    NSMutableDictionary *record=[@{@"identity":manifest[@"identity"],@"files":receipts} mutableCopy];
    // Prefer the current public filename and retain exact legacy names.
    // Never glob arbitrary ZIPs or broaden the asset whitelist.
    NSArray<NSString *> *archiveNames=@[@"iCSM-v2.zip",@"iCSM-v1.zip",@"iCSM-Data.zip"];
    NSString *archiveName=archiveNames.firstObject;
    for(NSString *candidate in archiveNames) {
        if([fm fileExistsAtPath:[documents stringByAppendingPathComponent:candidate]]){archiveName=candidate;break;}
    }
    NSString *archivePath=[documents stringByAppendingPathComponent:archiveName];
    if(selectedArchive){archivePath=selectedArchive.path;archiveName=selectedArchive.lastPathComponent;}
    context[@"archive_name"]=archiveName;context[@"archive_path"]=archivePath;context[@"accepted_package_names"]=archiveNames;
    context[@"archive_source"]=selectedArchive?@"document_picker":@"file_sharing";
    if(selectedArchive) {
        NSMutableDictionary *operation=installStep(context,@"open_selected_archive",archiveName);
        struct stat info={};errno=0;
        if(!selectedArchive.isFileURL || lstat(archivePath.fileSystemRepresentation,&info)!=0 || !S_ISREG(info.st_mode)) {
            operation[@"reason"]=@"selected_archive_unavailable";operation[@"errno"]=@(errno);
            return fail(error,@"无法读取所选 ZIP。请确认文件已下载，并重新选择；可导出日志查看原因。");
        }
    }
    NSDictionary *initialDisk=[fm attributesOfFileSystemForPath:documents error:nil];
    if(initialDisk[NSFileSystemFreeSize])context[@"free_bytes"]=initialDisk[NSFileSystemFreeSize];
    NSMutableArray *candidates=[NSMutableArray array];
    for(NSString *name in [[fm contentsOfDirectoryAtPath:documents error:nil] sortedArrayUsingSelector:@selector(compare:)]) {
        if(![name.pathExtension.lowercaseString isEqualToString:@"zip"] || candidates.count>=20)continue;
        NSDictionary *attributes=[fm attributesOfItemAtPath:[documents stringByAppendingPathComponent:name] error:nil];
        [candidates addObject:@{@"filename":name,@"bytes":attributes[NSFileSize]?:@0}];
    }
    context[@"zip_files_in_documents"]=candidates;
    NSDictionary *archiveAttributes=[fm attributesOfItemAtPath:archivePath error:nil];
    context[@"archive_exists"]=@([fm fileExistsAtPath:archivePath]);context[@"archive_bytes"]=archiveAttributes[NSFileSize]?:@0;
    unzFile zip=nullptr;
    std::map<std::string,unz64_file_pos> positions;
    if([fm fileExistsAtPath:archivePath]) {
        NSMutableDictionary *operation=installStep(context,@"open_zip",archiveName);
        if(progress)progress([@"正在读取数据包目录\n" stringByAppendingString:archiveName],-1);
        if(!selectedArchive && !safeParents(documents,archiveName,error))return NO;
        errno=0;zip=unzOpen64(archivePath.fileSystemRepresentation);
        if(!zip){operation[@"reason"]=@"zip_open_failed";operation[@"errno"]=@(errno);return fail(error,@"数据包尚未复制完成或已损坏。请完成复制后点击“检查并导入”。");}
        // Validate the complete central directory before modifying any asset.
        std::set<std::string> names;
        operation=installStep(context,@"validate_zip_directory");int status=unzGoToFirstFile(zip);
        while(status==UNZ_OK) {
            operation=installStep(context,@"validate_zip_directory");
            unz_file_info64 info={};
            int infoStatus=unzGetCurrentFileInfo64(zip,&info,nullptr,0,nullptr,0,nullptr,0);
            operation[@"zip_info_status"]=@(infoStatus);
            if(infoStatus!=UNZ_OK || info.size_filename>4096 || !info.size_filename){operation[@"reason"]=@"invalid_zip_entry_header";break;}
            std::vector<char> filename(info.size_filename+1,0);
            infoStatus=unzGetCurrentFileInfo64(zip,&info,filename.data(),(uLong)filename.size(),nullptr,0,nullptr,0);
            if(infoStatus!=UNZ_OK){operation[@"reason"]=@"zip_filename_read_failed";operation[@"zip_info_status"]=@(infoStatus);break;}
            NSString *name=[[NSString alloc] initWithBytes:filename.data() length:info.size_filename encoding:NSUTF8StringEncoding];
            if(name)context[@"current_file"]=name;
            NSDictionary *entry=name?expected[name]:nil;
            unsigned mode=(unsigned)(info.external_fa>>16);
            NSString *reason=nil;
            if(!name || !safeName(name) || strlen(filename.data())!=info.size_filename)reason=@"invalid_zip_path";
            else if(!entry)reason=@"unexpected_zip_entry";
            else if(!names.insert(std::string(filename.data())).second)reason=@"duplicate_zip_entry";
            else if(info.uncompressed_size!=[entry[@"size"] unsignedLongLongValue])reason=@"zip_entry_size_mismatch";
            else if(info.flag&1)reason=@"encrypted_zip_entry";
            else if(info.compression_method!=0 && info.compression_method!=8)reason=@"unsupported_zip_compression";
            else if((mode&S_IFMT)!=0 && (mode&S_IFMT)!=S_IFREG)reason=@"non_regular_zip_entry";
            if(reason){operation[@"reason"]=reason;operation[@"actual_bytes"]=@(info.uncompressed_size);operation[@"expected_bytes"]=entry[@"size"]?:@0;operation[@"compression_method"]=@(info.compression_method);break;}
            unz64_file_pos position={};
            int positionStatus=unzGetFilePos64(zip,&position);
            if(positionStatus!=UNZ_OK){operation[@"reason"]=@"zip_entry_position_failed";operation[@"zip_status"]=@(positionStatus);break;}
            positions[filename.data()]=position;
            status=unzGoToNextFile(zip);
        }
        if(status!=UNZ_END_OF_LIST_OF_FILE || names.size()!=expected.count) {
            operation[@"zip_directory_status"]=@(status);operation[@"validated_entries"]=@(positions.size());
            if(!operation[@"reason"]) {
                operation[@"reason"]=status!=UNZ_END_OF_LIST_OF_FILE?@"zip_directory_read_failed":@"zip_entry_count_mismatch";
                [context removeObjectForKey:@"current_file"];
                NSMutableArray *absent=[NSMutableArray array];
                for(NSString *name in [[expected allKeys] sortedArrayUsingSelector:@selector(compare:)])
                    if(positions.find(name.UTF8String)==positions.end() && absent.count<20)[absent addObject:name];
                operation[@"missing_entry_sample"]=absent;
            }
            unzClose(zip);return fail(error,@"数据包与当前应用不匹配、未复制完成，或包含无效路径。请使用配套的 iCSM-v2.zip。");
        }
    }
    NSMutableArray *missing=[NSMutableArray array];unsigned long long done=0,required=0;
    if(progress)progress(@"正在检查游戏数据",0);
    for(NSString *name in [[expected allKeys] sortedArrayUsingSelector:@selector(compare:)]) {
        @autoreleasepool {
            installStep(context,@"verify_existing_files",name);
            NSDictionary *entry=expected[name];NSString *path=targetPath(name,documents,cache);
            if(!safeParents(documents,storageName(name),error)){if(zip)unzClose(zip);return NO;}
            NSDictionary *current=stamp(path),*receipt=receipts[name];
            BOOL valid=current && [current[@"size"] isEqual:entry[@"size"]] &&
                [receipt[@"sha256"] isEqual:entry[@"sha256"]] && [receipt[@"stamp"] isEqual:current];
            // Existing installations migrate once, without changing user saves.
            if(!valid && current && [current[@"size"] isEqual:entry[@"size"]]) {
                NSString *digest=hashFile(path,progress,double(done)/double(total),double([entry[@"size"] unsignedLongLongValue])/double(total),[entry[@"size"] unsignedLongLongValue]);
                valid=[digest isEqual:entry[@"sha256"]];
                if(valid)receipts[name]=@{@"sha256":entry[@"sha256"],@"stamp":stamp(path)};
            }
            if(!valid){[missing addObject:name];required+=[entry[@"size"] unsignedLongLongValue];}
            done+=[entry[@"size"] unsignedLongLongValue];
            if(progress)progress([NSString stringWithFormat:@"正在检查游戏数据\n%@",name.lastPathComponent],double(done)/double(total?:1));
        }
    }
    // Persist migration and per-file success even if a subsequent import fails.
    context[@"missing_files"]=@(missing.count);context[@"missing_file_sample"]=[missing subarrayWithRange:NSMakeRange(0,MIN(missing.count,(NSUInteger)20))];context[@"import_bytes_required"]=@(required);
    installStep(context,@"write_receipts",receiptPath);
    if(!writeJSON(record,receiptPath,error)){if(zip)unzClose(zip);return NO;}
    if(missing.count && !zip){installStep(context,@"locate_data_package")[@"reason"]=@"package_missing";return fail(error,[NSString stringWithFormat:@"请导入配套数据包 iCSM-v2.zip（缺少 %lu 个文件）。\n点击左上角“选择数据包 ZIP”，从“文件”中选择。\n也可用 Finder / Apple Devices 复制到 iCSM，\n传输完成后点击“检查并导入”。",(unsigned long)missing.count]);}
    if(missing.count) {
        NSMutableDictionary *operation=installStep(context,@"check_free_space");NSError *diskError=nil;
        NSDictionary *disk=[fm attributesOfFileSystemForPath:documents error:&diskError];
        if(!disk){if(error)*error=diskError;unzClose(zip);return NO;}
        context[@"free_bytes"]=disk[NSFileSystemFreeSize]?:@0;context[@"required_free_bytes"]=@(required+512ULL*1024*1024);
        if([disk[NSFileSystemFreeSize] unsignedLongLongValue]<required+512ULL*1024*1024) {
            operation[@"reason"]=@"insufficient_free_space";
            unzClose(zip);return fail(error,[NSString stringWithFormat:@"剩余空间不足，解包还需要约 %.1f GB。",double(required)/1e9+0.5]);
        }
        for(NSString *name in missing)[receipts removeObjectForKey:name];
        installStep(context,@"write_receipts",receiptPath);
        if(!writeJSON(record,receiptPath,error)){unzClose(zip);return NO;}
        std::vector<unsigned char> buffer(1024*1024);unsigned long long imported=0;unsigned uncommitted=0;
        if(progress)progress(@"正在导入游戏资源",0);
        for(NSString *name in missing) {
            @autoreleasepool {
                NSDictionary *entry=expected[name];NSString *path=targetPath(name,documents,cache);
                NSString *temporary=[path stringByAppendingString:@".icsm-partial"];
                NSMutableDictionary *operation=installStep(context,@"extract_file",name);
                operation[@"expected_bytes"]=entry[@"size"];operation[@"expected_sha256"]=entry[@"sha256"];
                NSError *local=nil;
                if(!safeParents(documents,[storageName(name) stringByAppendingString:@".icsm-partial"],error)){unzClose(zip);return NO;}
                if(![fm createDirectoryAtPath:path.stringByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:&local]){if(error)*error=local;unzClose(zip);return NO;}
                auto position=positions.find(name.UTF8String);
                int openStatus=position==positions.end()?UNZ_PARAMERROR:unzGoToFilePos64(zip,&position->second);
                if(openStatus==UNZ_OK)openStatus=unzOpenCurrentFile(zip);
                if(openStatus!=UNZ_OK){operation[@"reason"]=@"zip_entry_open_failed";operation[@"zip_status"]=@(openStatus);unzClose(zip);return fail(error,@"无法读取数据包，请重新复制。");}
                FILE *out=fopen(temporary.fileSystemRepresentation,"wb");
                if(!out){int code=errno;operation[@"reason"]=@"file_open_for_write_failed";operation[@"errno"]=@(code);unzCloseCurrentFile(zip);unzClose(zip);fail(error,@"无法写入游戏数据，请检查剩余空间。");if(error && code){NSMutableDictionary *info=[(*error).userInfo mutableCopy];info[NSUnderlyingErrorKey]=posixError(code,temporary);*error=[NSError errorWithDomain:(*error).domain code:(*error).code userInfo:info];}return NO;}
                CC_SHA256_CTX ctx;CC_SHA256_Init(&ctx);unsigned long long written=0,outputBytes=0;int n=0,ioError=0;
                BOOL ok=YES;
                auto failure=[&](NSString *reason,int code){ok=NO;if(!operation[@"reason"])operation[@"reason"]=reason;if(!ioError && code)ioError=code;};
                while((n=unzReadCurrentFile(zip,buffer.data(),(unsigned)buffer.size()))>0) {
                    written+=(unsigned)n;
                    if(written>[entry[@"size"] unsignedLongLongValue]){failure(@"expanded_file_size_exceeded",0);break;}
                    size_t output=fwrite(buffer.data(),1,(size_t)n,out);outputBytes+=output;
                    if(output!=(size_t)n){failure(@"file_write_failed",errno);break;}
                    CC_SHA256_Update(&ctx,buffer.data(),(CC_LONG)n);
                    operation[@"bytes_read"]=@(written);operation[@"bytes_written"]=@(outputBytes);
                    if(progress)progress([NSString stringWithFormat:@"正在导入游戏资源\n%@",name.lastPathComponent],double(imported+written)/double(required?:1));
                }
                operation[@"zip_read_status"]=@(n);if(n<0)failure(@"zip_entry_read_failed",0);
                int flush=fflush(out);operation[@"flush_status"]=@(flush);if(flush!=0)failure(@"file_flush_failed",errno);
                int sync=fsync(fileno(out));operation[@"fsync_status"]=@(sync);if(sync!=0)failure(@"file_sync_failed",errno);
                int close=fclose(out);operation[@"file_close_status"]=@(close);if(close!=0)failure(@"file_close_failed",errno);
                int zipClose=unzCloseCurrentFile(zip);operation[@"zip_close_status"]=@(zipClose);
                if(zipClose!=UNZ_OK)failure(zipClose==UNZ_CRCERROR?@"zip_crc_mismatch":@"zip_entry_close_failed",0);
                unsigned char digest[32];CC_SHA256_Final(digest,&ctx);
                NSString *actualHash=hexDigest(digest);operation[@"actual_sha256"]=actualHash;
                operation[@"bytes_read"]=@(written);operation[@"bytes_written"]=@(outputBytes);operation[@"errno"]=@(ioError);
                if(written!=[entry[@"size"] unsignedLongLongValue])failure(@"extracted_file_size_mismatch",0);
                if(![actualHash isEqual:entry[@"sha256"]])failure(@"sha256_mismatch",0);
                if(!ok) {
                    [fm removeItemAtPath:temporary error:nil];unzClose(zip);
                    fail(error,@"数据包校验失败或写入被中断。请重新复制后重试；已完成的文件会保留。");
                    if(error && ioError){NSMutableDictionary *info=[(*error).userInfo mutableCopy];info[NSUnderlyingErrorKey]=posixError(ioError,temporary);*error=[NSError errorWithDomain:(*error).domain code:(*error).code userInfo:info];}
                    return NO;
                }
                operation=installStep(context,@"commit_file",name);
                if(rename(temporary.fileSystemRepresentation,path.fileSystemRepresentation)!=0){int code=errno;operation[@"errno"]=@(code);operation[@"reason"]=@"file_rename_failed";unzClose(zip);if(error)*error=posixError(code,path);return NO;}
                receipts[name]=@{@"sha256":entry[@"sha256"],@"stamp":stamp(path)};
                // A crash between commits only rehashes at most 32 completed files.
                if(++uncommitted>=32 || written>=64ULL*1024*1024) {
                    installStep(context,@"write_receipts",receiptPath);
                    if(!writeJSON(record,receiptPath,error)){unzClose(zip);return NO;}
                    uncommitted=0;
                }
                imported+=written;
                context[@"imported_bytes"]=@(imported);context[@"completed_files"]=@(receipts.count);
            }
        }
    }
    if(zip)unzClose(zip);
    // Generic iOS metallibs are portable; PSO archives stay device-specific.
    // Load each library on this device once, release it immediately, and let
    // the renderer create/record actual pipeline permutations when requested.
    installStep(context,@"create_metal_device");id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    if(!device)return fail(error,@"当前设备无法创建 Metal 设备。");
    NSString *deviceKey=[NSString stringWithFormat:@"%@/%@/%llu",manifest[@"identity"],NSProcessInfo.processInfo.operatingSystemVersionString,(unsigned long long)device.registryID];
    NSMutableArray *shaders=[NSMutableArray array];
    for(NSString *name in expected)if([name hasPrefix:@"shader-cache/"])[shaders addObject:name];
    BOOL restoredCache=NO;
    NSUInteger checkedShaders=0;
    if(shaders.count && progress)progress(@"正在检查 Metal 着色器缓存",0);
    for(NSString *name in shaders) {
        NSMutableDictionary *operation=installStep(context,@"restore_shader_cache",name);
        NSString *filename=[name substringFromIndex:13],*cached=[cache stringByAppendingPathComponent:filename];
        if(!safeParents(cache,filename,error))return NO;
        if(![fm fileExistsAtPath:cached]) {
            NSString *partial=[cached stringByAppendingString:@".icsm-partial"];
            if(!safeParents(cache,[filename stringByAppendingString:@".icsm-partial"],error))return NO;
            [fm removeItemAtPath:partial error:nil];
            NSError *local=nil;
            if(![fm copyItemAtPath:targetPath(name,documents,cache) toPath:partial error:&local]){if(error)*error=local;return NO;}
            if(rename(partial.fileSystemRepresentation,cached.fileSystemRepresentation)!=0){int code=errno;operation[@"reason"]=@"shader_cache_rename_failed";operation[@"errno"]=@(code);if(error)*error=posixError(code,cached);return NO;}
            restoredCache=YES;
        }
        if(progress)progress([NSString stringWithFormat:@"正在检查 Metal 着色器缓存\n%@",filename],double(++checkedShaders)/double(shaders.count));
    }
    if(![decoded[@"shader_device"] isEqual:deviceKey] || missing.count || restoredCache) {
        NSUInteger count=0;
        if(shaders.count && progress)progress(@"正在准备 Metal 着色器",0);
        for(NSString *name in [shaders sortedArrayUsingSelector:@selector(compare:)]) {
            @autoreleasepool {
                installStep(context,@"load_metal_library",name);
                NSString *path=targetPath(name,documents,cache);
                NSData *data=[NSData dataWithContentsOfFile:path];
                if(!data)return fail(error,@"离线着色器文件缺失，请重新导入数据包。");
                // Close file before handing owned data to Metal (no open-FD accumulation).
                dispatch_data_t bytes=dispatch_data_create(data.bytes,data.length,nullptr,^{(void)data;});
                NSError *metalError=nil;id<MTLLibrary> library=[device newLibraryWithData:bytes error:&metalError];
                if(!library || !library.functionNames.count) {
                    NSMutableDictionary *details=[@{NSLocalizedDescriptionKey:[NSString stringWithFormat:@"着色器无法在当前系统加载：%@",metalError.localizedDescription?:name],@"shader_file":name,@"stage":@"newLibraryWithData"} mutableCopy];
                    if(metalError)details[NSUnderlyingErrorKey]=metalError;
                    if(error)*error=[NSError errorWithDomain:@"iCSM.Data" code:2 userInfo:details];
                    return NO;
                }
                if(progress)progress([NSString stringWithFormat:@"正在准备 Metal 着色器\n%@",name.lastPathComponent],double(++count)/double(shaders.count));
            }
        }
    }
    record[@"shader_device"]=deviceKey;record[@"ready"]=@YES;
    installStep(context,@"write_receipts",receiptPath);
    if(!writeJSON(record,receiptPath,error))return NO;
    [@[documents,[documents stringByAppendingPathComponent:@"game-assets"]] enumerateObjectsUsingBlock:^(NSString *path,NSUInteger index,BOOL *stop){
        if(index)[[NSURL fileURLWithPath:path] setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];
    }];
    // Only remove the exact archive after every hash and Metal load succeeded.
    if(!selectedArchive && [fm fileExistsAtPath:archivePath])[fm removeItemAtPath:archivePath error:nil];
    if(progress)progress(@"游戏资源已就绪",1);
    installStep(context,@"ready");
    return YES;
}

static BOOL coordinatedInstallation(NSURL *archive,NSString *documents,NSString *cache,NSDictionary *manifest,
                                    NSString *receiptPath,ICSMInstallProgress progress,NSError *__strong *error) {
    NSMutableDictionary *context=[@{@"attempt_id":NSUUID.UUID.UUIDString,@"result":@"in_progress",@"phase":@"begin"} mutableCopy];
    NSString *directory=[documents stringByAppendingPathComponent:@"I5"],*path=[directory stringByAppendingPathComponent:@"import-diagnostics.json"];
    [NSFileManager.defaultManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:nil];
    __block CFAbsoluteTime lastWrite=0;__block NSString *lastPhase=nil;
    void (^checkpoint)(BOOL)=^(BOOL force){
        CFAbsoluteTime now=CFAbsoluteTimeGetCurrent();NSString *phase=context[@"phase"];
        if(!force && [phase isEqualToString:lastPhase] && now-lastWrite<1)return;
        lastWrite=now;lastPhase=phase;context[@"updated_utc"]=[[NSISO8601DateFormatter new] stringFromDate:NSDate.date];
        NSData *data=[NSJSONSerialization dataWithJSONObject:context options:NSJSONWritingPrettyPrinted error:nil];
        [data writeToFile:path atomically:YES];
    };
    checkpoint(YES);__block NSError *failure=nil;__block BOOL ready=NO;
    ICSMInstallProgress report=^(NSString *stage,double fraction){
        context[@"progress_stage"]=stage;context[@"progress_fraction"]=@(fraction);checkpoint(NO);
        if(progress)progress(stage,fraction);
    };
    if(archive) {
        context[@"archive_source"]=@"document_picker";context[@"archive_name"]=archive.lastPathComponent?:@"";
        context[@"archive_path"]=archive.path?:@"";
        installStep(context,@"coordinate_selected_archive");
        report(@"正在读取所选数据包\n如文件在云端，请等待下载完成并保持应用在前台。",-1);
        BOOL scoped=[archive startAccessingSecurityScopedResource];context[@"security_scope_acquired"]=@(scoped);checkpoint(YES);
        @try {
            NSFileCoordinator *coordinator=[[NSFileCoordinator alloc] initWithFilePresenter:nil];
            NSError *coordinationError=nil;__block BOOL accessed=NO;
            [coordinator coordinateReadingItemAtURL:archive options:0 error:&coordinationError byAccessor:^(NSURL *readURL){
                accessed=YES;
                ready=prepareInstallation(documents,cache,manifest,receiptPath,report,&failure,context,readURL);
            }];
            if(!accessed) {
                installStep(context,@"coordinate_selected_archive")[@"reason"]=@"file_provider_read_failed";
                NSMutableDictionary *details=[@{NSLocalizedDescriptionKey:@"无法从“文件”读取数据包。请确认下载完成、网络正常，再重新选择。"} mutableCopy];
                if(coordinationError)details[NSUnderlyingErrorKey]=coordinationError;
                failure=[NSError errorWithDomain:@"iCSM.Data" code:3 userInfo:details];
            }
        } @finally {if(scoped)[archive stopAccessingSecurityScopedResource];}
    }else ready=prepareInstallation(documents,cache,manifest,receiptPath,report,&failure,context,nil);
    context[@"result"]=ready?@"ready":@"failed";
    if(!ready) {
        if(!failure)failure=[NSError errorWithDomain:@"iCSM.Data" code:1 userInfo:@{NSLocalizedDescriptionKey:@"数据导入失败，请导出报错日志。"}];
        context[@"error_domain"]=failure.domain;context[@"error_code"]=@(failure.code);context[@"error_description"]=failure.localizedDescription;
        NSMutableDictionary *details=[failure.userInfo mutableCopy];details[@"import_details"]=[context copy];
        if(!details[@"stage"])details[@"stage"]=context[@"phase"];
        failure=[NSError errorWithDomain:failure.domain code:failure.code userInfo:details];
        fprintf(stderr,"ICSM_IMPORT_FAILED phase=%s file=%s domain=%s code=%ld\n",[context[@"phase"] UTF8String],[context[@"current_file"] UTF8String]?:"",failure.domain.UTF8String,(long)failure.code);
    }else fprintf(stderr,"ICSM_IMPORT_READY files=%lu\n",(unsigned long)[manifest[@"files"] count]);
    checkpoint(YES);if(error)*error=failure;return ready;
}

BOOL ICSMPrepareInstallation(NSString *documents,NSString *cache,NSDictionary *manifest,
                            NSString *receiptPath,ICSMInstallProgress progress,NSError *__strong *error) {
    return coordinatedInstallation(nil,documents,cache,manifest,receiptPath,progress,error);
}
BOOL ICSMPrepareInstallationFromArchive(NSURL *archive,NSString *documents,NSString *cache,
                                       NSDictionary *manifest,NSString *receiptPath,
                                       ICSMInstallProgress progress,NSError *__strong *error) {
    if(!archive)return fail(error,@"请先选择数据包 ZIP。");
    return coordinatedInstallation(archive,documents,cache,manifest,receiptPath,progress,error);
}
