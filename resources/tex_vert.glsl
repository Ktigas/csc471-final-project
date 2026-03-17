#version 330 core

layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNor;
layout(location = 2) in vec2 vertTex;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

out vec2 vTexCoord;
out vec3 fragNor;
out vec3 fragPos;  // in world space

void main() {
    // Calculate world position (needed for lighting)
    vec4 worldPos = M * vec4(vertPos, 1.0);
    fragPos = worldPos.xyz;
    
    mat3 normalMatrix = transpose(inverse(mat3(M)));
    fragNor = normalMatrix * vertNor;

    gl_Position = P * V * worldPos;
    
    vTexCoord = vertTex;
}