#pragma message("Compiling GLBuffer.cpp")
#include "gfx/glad/GLBuffer.h"
#include "glad/glad.h"

GLBuffer::GLBuffer(BufferType t):m_Type(t){ glGenBuffers(1,&m_Id); }
GLBuffer::~GLBuffer(){ if(m_Id) glDeleteBuffers(1,&m_Id); }

void GLBuffer::Bind() const {
    glBindBuffer(m_Type==BufferType::Vertex ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER, m_Id);
}
void GLBuffer::Unbind() const {
    glBindBuffer(m_Type==BufferType::Vertex ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER, 0);
}
void GLBuffer::SetData(const void* data,std::size_t bytes,bool dynamic){
    Bind();
    glBufferData(m_Type==BufferType::Vertex ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)bytes, data, dynamic?GL_DYNAMIC_DRAW:GL_STATIC_DRAW);
}