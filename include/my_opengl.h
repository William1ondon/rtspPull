#pragma once

#include <iostream>
#include <string>
#include <pthread.h>
#include <EGL/eglplatform.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "ION_DRM/ion.h"
#include "ION_DRM/drm_fourcc.h"
#include "my_buffer.h"

using namespace std;

typedef struct fbdev_window
{
    unsigned short width;
    unsigned short height;
} fbdev_window1;

typedef struct
{
    unsigned int TexFormat;
    unsigned int tex_width;
    unsigned int tex_height;
    unsigned int view_width;
    unsigned int view_height;
} texture_config;

typedef enum
{
    E_CAMERA_RENDER = 0,
    E_VGA_HDMI_RENDER
} ERender_Type;

typedef struct
{
    EGLint egl_major, egl_minor;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
} EGL_TYPE;

class CT507Graphics
{
public:
    static CT507Graphics *getInstance();
    void EGLInfoDump();
    int load_texture(void *yuv_virAddr, int w, int h, int chn);
    int setupTexture();
    int initOpenGL();
    int picDraw();
    bool isRenderBusyOrPending();

private:
    CT507Graphics();
    ~CT507Graphics();
    CT507Graphics(const CT507Graphics &graphicsTmp) {};

    int initEGL();
    int readShaderSource(string &vShader, string &fShader, int index);
    int initShader(const char *vertexShader, const char *fragmentShader, int index);
    int createShader(unsigned int *shader_id, int shaderType, const char *shaderSource);
    int fullScreenMode();
    int quadSplitMode();
    int sixSplitMode();

private:
    EGL_TYPE m_egl;
    unsigned int vs_id;
    unsigned int fs_id;
    uint32_t *shader_program;

    bool bRender;
    bool bRenderBusy;
    bool m_useGlFinish;
    unsigned int Tex[DISP_CHN_NUM][T507_PREVIEW_BUF_NUM + T507_PLAYBACK_BUF_NUM];
    unsigned int TexIndex[DISP_CHN_NUM];

    unsigned int fboColorBufTex;
    unsigned int fbo;
    texture_config texConfig;

    int vTexSamplerHandler;
    pthread_mutex_t dataMutex;
    pthread_mutex_t RenderMutex;
    pthread_cond_t RenderCond;
};