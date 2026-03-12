#version 330 core

layout(location = 0) in vec4 vertPos;
layout(location = 1) in vec3 vertNor;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

out vec3 fragNor;
out vec3 fragPos;

void main() {
    gl_Position = P * V * M * vertPos;

    // World-space position for lighting (NOT perspective space)
    fragPos = (M * vertPos).xyz;

    // Transform normal by M (w=0 so translation doesn't affect normals)
    fragNor = (M * vec4(vertNor, 0.0)).xyz;
}
