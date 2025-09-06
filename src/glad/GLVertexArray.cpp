#include "gfx/glad/GLVertexArray.h"
#include "glad/glad.h"

GLVertexArray::GLVertexArray(){ glGenVertexArrays(1,&m_Id); }
GLVertexArray::~GLVertexArray(){ if(m_Id) glDeleteVertexArrays(1,&m_Id); }
void GLVertexArray::Bind() const { glBindVertexArray(m_Id); }
void GLVertexArray::Unbind() const { glBindVertexArray(0); }

void GLVertexArray::EnableAttrib(unsigned int idx,int comps,unsigned int type,
                                 bool normalized,int stride,std::size_t offset){
    glEnableVertexAttribArray(idx);
    glVertexAttribPointer(idx, comps, type, normalized?GL_TRUE:GL_FALSE, stride, (const void*)offset);
}