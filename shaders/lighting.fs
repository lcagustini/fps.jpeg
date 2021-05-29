#version 330 core
out vec4 finalColor;

in vec3 fragNormal;
in vec3 fragPosition;

uniform vec3 viewPos;

void main()
{
    vec3 lightPos = vec3(2.0, 5.0, 2.0);
    vec3 lightColor = vec3(0.9, 0.5, 0.2);
    vec3 objectColor = vec3(0.8, 0.8, 0.8);

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

    vec3 result = (ambient + diffuse);

    lightPos = vec3(-2.0, 5.0, -2.0);
    lightColor = 0.25 * vec3(0.9, 0.5, 0.2);

    // ambient
    ambient = ambientStrength * lightColor;

    // diffuse
    norm = normalize(fragNormal);
    lightDir = normalize(lightPos - fragPosition);
    diff = max(dot(norm, lightDir), 0.0);
    diffuse = diff * lightColor;

    // specular
    viewDir = normalize(viewPos - fragPosition);
    reflectDir = reflect(-lightDir, norm);
    spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    specular = specularStrength * spec * lightColor;

    result = (result + diffuse) * objectColor;

    finalColor = vec4(result + result, 1.0);
}
