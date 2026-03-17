#version 330 core

in vec3 fragNor;
in vec3 fragPos;

uniform vec3  lightPos;
uniform vec3  MatAmb;
uniform vec3  MatDif;
uniform vec3  MatSpec;
uniform vec3 viewPos;
uniform float MatShine;

out vec4 color;

void main() {
    vec3 N = normalize(fragNor);
    vec3 L = normalize(lightPos - fragPos);

    // view dir 
    vec3 V = normalize(viewPos - fragPos);

    // halfway vector for specular
    vec3 H = normalize(L + V);

    // ambient
    vec3 ambient = MatAmb;

    // diffuse 
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * MatDif;

    // specular 
    float spec = pow(max(dot(N, H), 0.0), MatShine);
    vec3 specular = spec * MatSpec;

    color = vec4(ambient + diffuse + specular, 1.0);
}
