#version 330 core
out vec4 finalColor;

in vec3 fragNormal;
in vec3 fragPosition;
uniform vec4 colDiffuse;

uniform vec3 viewPos;

uniform vec3 lightsPosition[10];
uniform vec3 lightsColor[10];
uniform int lightsLen;

void main()
{
    for (int i = 0; i < lightsLen; i++) {
        vec3 lightPos = lightsPosition[i];
        vec3 lightColor = lightsColor[i];
        vec3 objectColor = vec3(colDiffuse.xyz);

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

        vec3 result = (ambient + diffuse) * objectColor;
        finalColor = vec4(result, 1.0);
    }
    //finalColor = vec4(fragNormal, 1.0);
}
