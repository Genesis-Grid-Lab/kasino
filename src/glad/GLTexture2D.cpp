#include "gfx/glad/GLTexture2D.h"
#include "glad/glad.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <cstdio>

GLTexture2D::GLTexture2D(){ glGenTextures(1,&m_id); }
GLTexture2D::~GLTexture2D(){ if(m_id) glDeleteTextures(1,&m_id); }

void GLTexture2D::allocate(uint32_t w, uint32_t h, int channels){
    m_w=w; m_h=h; m_channels=channels;
    GLenum fmt = (channels==4)? GL_RGBA : GL_RGB;
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, (channels==4?GL_RGBA8:GL_RGB8), w, h, 0, fmt, GL_UNSIGNED_BYTE, nullptr);
}

bool GLTexture2D::Create(uint32_t w, uint32_t h, int channels, const void* pixels){
    if(channels!=3 && channels!=4) { std::fprintf(stderr,"[GLTexture2D] channels must be 3 or 4\n"); return false; }
    allocate(w,h,channels);
    GLenum fmt = (channels==4)? GL_RGBA : GL_RGB;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0,0, w,h, fmt, GL_UNSIGNED_BYTE, pixels);
    return true;
}

bool GLTexture2D::LoadFromFile(const char* path, bool flipY){
    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);
    int w,h,n;
    unsigned char* data = stbi_load(path, &w, &h, &n, 0);
    if(!data){ std::fprintf(stderr,"[GLTexture2D] failed to load %s\n", path); return false; }
    if(n!=3 && n!=4) { // convert to RGBA
        unsigned char* rgba = (unsigned char*)malloc((size_t)w*h*4);
        for(int i=0;i<w*h;i++){
            rgba[i*4+0] = data[i*n+0];
            rgba[i*4+1] = data[i*n+ (n>1?1:0)];
            rgba[i*4+2] = data[i*n+ (n>2?2:0)];
            rgba[i*4+3] = 255;
        }
        allocate((uint32_t)w,(uint32_t)h,4);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
        free(rgba);
    } else {
        allocate((uint32_t)w,(uint32_t)h,n);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,(n==4?GL_RGBA:GL_RGB),GL_UNSIGNED_BYTE,data);
    }
    stbi_image_free(data);
    return true;
}

void GLTexture2D::Bind(uint32_t slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_id);
}
