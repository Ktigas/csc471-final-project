#version 330 core

uniform sampler2D Texture0;
uniform vec3 lightPos;
uniform int flip;

in vec2 vTexCoord;
in vec3 fragNor;
in vec3 fragPos;

out vec4 Outcolor;

void main()
{
    vec3 normal = normalize(fragNor);
    if (flip == 1) {
        normal = -normal;
    }

    vec3 lightDir = normalize(lightPos - fragPos);

    float diffuse = max(dot(normal, lightDir), 0.0);
    float ambient = 0.6;

    // Flip the texture coordinates for sky only
    vec2 texCoord = vTexCoord;
    if (flip == 1) {
        texCoord.t = 1.0 - texCoord.t;  // Flip V coordinate upside down
    }

    vec4 texColor = texture(Texture0, texCoord);

    Outcolor = texColor * (ambient + diffuse), 1.0;
}