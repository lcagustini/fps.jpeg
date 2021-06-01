#version 330 core

out vec4 finalColor;

in vec3 fragNormal;
in vec3 fragPosition;
in vec2 fragTexCoord;

uniform int isWall;

uniform vec4 colDiffuse;

uniform vec3 viewPos;

uniform vec3 lightsPosition[10];
uniform vec3 lightsColor[10];
uniform int lightsLen;

vec3 wallTexture(vec2 uv)
{
    if (fract(uv.x) < 0.1 || fract(uv.y) < 0.1) {
        return vec3(0.7, 0.7, 0.7);
    }

    return vec3(0.45, 0.2, 0.2);
}

void main()
{
    vec3 result = vec3(0);
    vec3 objectColor;

    if (isWall == 1) {
        if (fragTexCoord.x < 0.5 && fragTexCoord.y < 0.5) {
            //objectColor = vec3(0.0, 1.0, 1.0);
            objectColor = wallTexture(fragTexCoord * 300);
        } else if (fragTexCoord.x > 0.5 && fragTexCoord.y < 0.5) {
            objectColor = vec3(1.0, 0.3, 0.4);
        }
    } else {
        objectColor = vec3(colDiffuse.xyz);
    }

    for (int i = 0; i < lightsLen; i++) {
        vec3 lightPos = lightsPosition[i];
        vec3 lightColor = lightsColor[i];

        // ambient
        float ambientStrength = 0.2;
        vec3 ambient = ambientStrength * lightColor;

        // diffuse
        vec3 norm = normalize(fragNormal);
        vec3 lightDir = normalize(lightPos - fragPosition);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;

        // specular
        float specularStrength = 0.5;
        vec3 viewDir = normalize(viewPos - fragPosition);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec3 specular = specularStrength * spec * lightColor;

        result += ambient + diffuse;
    }

    finalColor = vec4(clamp(result, 0, 1) * objectColor, 1.0);
}
