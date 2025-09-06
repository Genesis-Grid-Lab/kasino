#include "gfx/glad/GLShader.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>

static GLenum ShaderTypeFromString(const std::string& type)
{
  if (type == "vertex")
    return GL_VERTEX_SHADER;
  if (type == "fragment" || type == "pixel")
    return GL_FRAGMENT_SHADER;

  // UE_CORE_ASSERT(false, "Unknown shader type!");
  return 0;
}

static bool compile(GLuint sh, const char* src, std::string* log){
  glShaderSource(sh, 1, &src, nullptr);
  glCompileShader(sh);
  GLint ok = 0;
  glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

  if (!ok && log) {
    GLint len = 0;
    glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
    std::vector<char> buf(len + 1);
    glGetShaderInfoLog(sh, len, nullptr, buf.data());
    *log += buf.data();
  }

  return ok==GL_TRUE;
}

GLShader::GLShader(const std::string &filepath) {
  std::string source = ReadFile(filepath);
  auto shaderSources = PreProcess(source);
  CompileFromSource(shaderSources);
}

void GLShader::Destroy() {
  if (m_Program) {
    glDeleteProgram(m_Program);
    m_Program = 0;
  }
}

std::unordered_map<GLenum, std::string> GLShader::PreProcess(const std::string& source)
{ 
  std::unordered_map<GLenum, std::string> shaderSources;

  const char* typeToken = "#type";
  size_t typeTokenLength = strlen(typeToken);
  size_t pos = source.find(typeToken, 0); //Start of shader type declaration line
  while (pos != std::string::npos)
    {
      size_t eol = source.find_first_of("\r\n", pos); //End of shader type declaration line
      // UE_CORE_ASSERT(eol != std::string::npos, "Syntax error");
      size_t begin = pos + typeTokenLength + 1; //Start of shader type name (after "#type " keyword)
      std::string type = source.substr(begin, eol - begin);
      // UE_CORE_ASSERT(ShaderTypeFromString(type), "Invalid shader type specified");

      size_t nextLinePos = source.find_first_not_of("\r\n", eol); //Start of shader code after shader type declaration line
      // UE_CORE_ASSERT(nextLinePos != std::string::npos, "Syntax error");
      pos = source.find(typeToken, nextLinePos); //Start of next shader type declaration line

      shaderSources[ShaderTypeFromString(type)] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
    }

  return shaderSources;
}

bool GLShader::CompileFromSource(const char *vs, const char *fs,
                                 std::string *outLog) {
  GLuint v = glCreateShader(GL_VERTEX_SHADER),
         f = glCreateShader(GL_FRAGMENT_SHADER);
  std::string log;

  if (!compile(v, vs, &log) || !compile(f, fs, &log)) {
    if (outLog)
      *outLog = log;
    glDeleteShader(v);
    glDeleteShader(f);
    return false;    
  }

  GLuint p = glCreateProgram();
  glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
  glDeleteShader(v); glDeleteShader(f);
  GLint ok=0; glGetProgramiv(p,GL_LINK_STATUS,&ok);
  if(!ok){ GLint len=0; glGetProgramiv(p,GL_INFO_LOG_LENGTH,&len);
    std::vector<char> buf(len + 1);
    glGetProgramInfoLog(p, len, nullptr, buf.data());    
    if (outLog)
      *outLog = buf.data();
    glDeleteProgram(p);
    return false;    
  }
  m_Program = p;
  m_Uniforms.clear();
  return true;
}

void GLShader::CompileFromSource(const std::unordered_map<GLenum, std::string>& shaderSources)
{

  GLuint program = glCreateProgram();
  // UE_CORE_ASSERT(shaderSources.size() <= 2, "We only support 2 shaders for now");
  std::array<GLenum, 2> glShaderIDs;
  int glShaderIDIndex = 0;
  for (auto& kv : shaderSources)
    {
      GLenum type = kv.first;
      const std::string& source = kv.second;

      GLuint shader = glCreateShader(type);

      const GLchar* sourceCStr = source.c_str();
      glShaderSource(shader, 1, &sourceCStr, 0);

      glCompileShader(shader);

      GLint isCompiled = 0;
      glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
      if (isCompiled == GL_FALSE)
	{
	  GLint maxLength = 0;
	  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

	  std::vector<GLchar> infoLog(maxLength);
	  glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

	  glDeleteShader(shader);

	  // UE_CORE_ERROR("{0}", infoLog.data());
	  // UE_CORE_ASSERT(false, "Shader compilation failure!");
	  break;
	}

      glAttachShader(program, shader);
      glShaderIDs[glShaderIDIndex++] = shader;
    }
		
  m_Program = program;

  // Link our program
  glLinkProgram(program);

  // Note the different functions here: glGetProgram* instead of glGetShader*.
  GLint isLinked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
  if (isLinked == GL_FALSE)
    {
      GLint maxLength = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

      // The maxLength includes the NULL character
      std::vector<GLchar> infoLog(maxLength);
      glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

      // We don't need the program anymore.
      glDeleteProgram(program);
			
      for (auto id : glShaderIDs)
	glDeleteShader(id);

      // UE_CORE_ERROR("{0}", infoLog.data());
      // UE_CORE_ASSERT(false, "Shader link failure!");
      return;
    }

  for (auto id : glShaderIDs)
    {
      glDetachShader(program, id);
      glDeleteShader(id);
    }
}

std::string GLShader::ReadFile(const std::string& filepath)
{

  std::string result;
  std::ifstream in(filepath, std::ios::in | std::ios::binary); // ifstream closes itself due to RAII
  if (in)
    {
      in.seekg(0, std::ios::end);
      size_t size = in.tellg();
      if (size != -1)
	{
	  result.resize(size);
	  in.seekg(0, std::ios::beg);
	  in.read(&result[0], size);
	}
      else
	{
	  // UE_CORE_ERROR("Could not read from file '{0}'", filepath);
	}
    }
  else
    {
      // UE_CORE_ERROR("Could not open file '{0}'", filepath);
    }

  return result;
}

void GLShader::Bind() const { glUseProgram(m_Program); }
void GLShader::Unbind() const { glUseProgram(0); }

int GLShader::uniformLoc(const char* name){
    auto it=m_Uniforms.find(name);
    if(it!=m_Uniforms.end()) return it->second;
    int loc=glGetUniformLocation(m_Program,name);
    m_Uniforms[name]=loc; return loc;
}
void GLShader::SetFloat(const char* name,float v){ glUniform1f(uniformLoc(name),v); }
void GLShader::SetVec2 (const char* name, const glm::vec2& v){ glUniform2f(uniformLoc(name),v.x,v.y); }
void GLShader::SetMat4 (const char* name,const glm::mat4& v){ glUniformMatrix4fv(uniformLoc(name),1,GL_FALSE,glm::value_ptr(v)); }
void GLShader::SetIntArray(const char* name, const int* v, int count) { glUniform1iv(uniformLoc(name), count, v); }
