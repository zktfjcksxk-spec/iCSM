#pragma once
struct SDL_Window;
TOGL_INTERFACE void *SourceMetalAttachWindow(SDL_Window *window);
TOGL_INTERFACE void SourceMetalDetachWindow(void *view);
TOGL_INTERFACE void SourceMetalSizeWindow(SDL_Window *window, unsigned width, unsigned height);
TOGL_INTERFACE void SourceMetalResizeWindow(unsigned width, unsigned height);
TOGL_INTERFACE void SourceMetalPrintWindowStats();
TOGL_INTERFACE void SourceMetalFillRendererInfo(GLMRendererInfoFields *info);
