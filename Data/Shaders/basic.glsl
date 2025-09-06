#type vertex
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec4 aColor;
layout(location=2) in vec2 aUV;
layout(location=3) in float aTexIndex;
layout(location=4) in float aTiling;

uniform mat4 uViewProj;

out vec4  vColor;
out vec2  vUV;
out float vTexIndex;
out float vTiling;

void main(){
    vColor     = aColor;
    vUV        = aUV;
    vTexIndex  = aTexIndex;
    vTiling    = aTiling;
    gl_Position = uViewProj * vec4(aPos, 0.0, 1.0);
}

#type fragment
#version 330 core
in vec4  vColor;
in vec2  vUV;
in float vTexIndex;
in float vTiling;

out vec4 FragColor;

uniform sampler2D uTextures[16];

vec4 sampleTex(int idx, vec2 uv) {
    if (idx == 0)  return texture(uTextures[0],  uv);
    if (idx == 1)  return texture(uTextures[1],  uv);
    if (idx == 2)  return texture(uTextures[2],  uv);
    if (idx == 3)  return texture(uTextures[3],  uv);
    if (idx == 4)  return texture(uTextures[4],  uv);
    if (idx == 5)  return texture(uTextures[5],  uv);
    if (idx == 6)  return texture(uTextures[6],  uv);
    if (idx == 7)  return texture(uTextures[7],  uv);
    if (idx == 8)  return texture(uTextures[8],  uv);
    if (idx == 9)  return texture(uTextures[9],  uv);
    if (idx == 10) return texture(uTextures[10], uv);
    if (idx == 11) return texture(uTextures[11], uv);
    if (idx == 12) return texture(uTextures[12], uv);
    if (idx == 13) return texture(uTextures[13], uv);
    if (idx == 14) return texture(uTextures[14], uv);
    return              texture(uTextures[15], uv);
}

void main(){
    int idx = int(vTexIndex + 0.5);
    vec4 tex = sampleTex(idx, vUV * vTiling);
    FragColor = vColor * tex;
}
