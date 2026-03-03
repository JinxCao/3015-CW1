#version 460

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec2 VertexTexCoord;
layout (location = 3) in vec4 VertexTangent;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out mat3 TBN;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(VertexPosition, 1.0));
    
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    Normal = normalize(normalMatrix * VertexNormal);
    
    TexCoord = VertexTexCoord;
    
    // TBN 矩阵用于法线贴图
    vec3 T = normalize(normalMatrix * VertexTangent.xyz);
    vec3 N = Normal;
    // 重新正交化
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * VertexTangent.w;
    TBN = mat3(T, B, N);
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}