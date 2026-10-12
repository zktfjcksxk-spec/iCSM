// Explicit one-frame diagnostic only; never read back during normal gameplay.
struct FGCapturePlane {
    id<MTLBuffer> buffer=nil;
    NSString *name=nil;
    unsigned width=0,height=0,pitch=0;
    unsigned bytesPerPixel=4;
};
static void fgCapture(Device &d,const std::shared_ptr<FGSlot> &current,const std::shared_ptr<FGSlot> &previous) {
    std::vector<FGCapturePlane> planes;
    auto blit=[d.command blitCommandEncoder];
    for(auto entry:{std::make_pair(previous->scene,@"previous.bgra"),std::make_pair(current->scene,@"current.bgra"),
                   std::make_pair(current->generated,@"generated.bgra"),std::make_pair(current->ui,@"ui.bgra"),
                   std::make_pair(current->depth,@"depth.f32"),std::make_pair(current->motion,@"motion.rg16"),
                   std::make_pair(previous->depth,@"previous-depth.f32"),std::make_pair(current->flow,@"flow.rgba16"),
                   std::make_pair(current->mask,@"viewmodel-mask.r8"),std::make_pair(previous->mask,@"previous-viewmodel-mask.r8"),
                   std::make_pair(current->compactColor,@"native-current.bgra"),std::make_pair(previous->compactColor,@"native-previous.bgra"),
                   std::make_pair(current->intermediate,@"native-generated.bgra")}) {
        FGCapturePlane plane;plane.name=entry.second;plane.width=entry.first.width;plane.height=entry.first.height;
        plane.bytesPerPixel=entry.first.pixelFormat==MTLPixelFormatR8Unorm?1:
            entry.first.pixelFormat==MTLPixelFormatRGBA16Float?8:4;
        plane.pitch=(plane.width*plane.bytesPerPixel+255)&~255u;
        plane.buffer=[device() newBufferWithLength:plane.pitch*plane.height options:MTLResourceStorageModeShared];
        [blit copyFromTexture:entry.first sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(plane.width,plane.height,1) toBuffer:plane.buffer destinationOffset:0
            destinationBytesPerRow:plane.pitch destinationBytesPerImage:plane.pitch*plane.height];
        planes.push_back(plane);
    }
    [blit endEncoding];
    NSMutableArray *metadata=[NSMutableArray array];
    for(const auto &plane:planes)[metadata addObject:@{@"file":plane.name,@"width":@(plane.width),@"height":@(plane.height),@"pitch":@(plane.pitch),@"bytes_per_pixel":@(plane.bytesPerPixel)}];
    auto matrix=[](const float *values) {NSMutableArray *a=[NSMutableArray array];for(unsigned i=0;i<16;i++)[a addObject:@(values[i])];return a;};
    NSDictionary *header=@{@"source_frame":@(current->frame),@"previous_source_frame":@(previous->frame),
        @"current_world_to_clip":matrix(current->view.worldToClip),@"previous_world_to_clip":matrix(previous->view.worldToClip),
        @"delta_time":@(current->time-previous->time),@"near":@(current->view.nearPlane),@"far":@(current->view.farPlane),
        @"vertical_fov":@(current->view.verticalFOV),@"planes":metadata,
        @"source_simulation_delta":@(current->view.simulationTime-previous->view.simulationTime),
        @"viewmodel_protected":@(icsm_metal_frame_protect_viewmodel.GetBool()),
        @"interpolation_resolution":@[@(fg.effectWidth),@(fg.effectHeight)],@"compact_scene":@(fg.compact),
        @"motion_resolution":@[@(fg.inputWidth),@(fg.inputHeight)],@"quality_motion_guard":@(icsm_metal_frame_quality.GetBool()),
        @"scope":@"Actual aligned MetalFX inputs/output, captured after GPU interpolation and before presentation gamma"};
    [d.command addCompletedHandler:^(id<MTLCommandBuffer> done) {
        @autoreleasepool {
        if(done.error)return;
        NSString *folder=[NSHomeDirectory() stringByAppendingPathComponent:@"Documents/I5/frame-interpolation-capture"];
        [[NSFileManager defaultManager] createDirectoryAtPath:folder withIntermediateDirectories:YES attributes:nil error:nil];
        for(const auto &plane:planes) {
            NSData *data=[NSData dataWithBytes:plane.buffer.contents length:plane.pitch*plane.height];
            [data writeToFile:[folder stringByAppendingPathComponent:plane.name] atomically:YES];
        }
        [[NSJSONSerialization dataWithJSONObject:header options:NSJSONWritingPrettyPrinted error:nil]
            writeToFile:[folder stringByAppendingPathComponent:@"metadata.json"] atomically:YES];
        }
    }];
}
