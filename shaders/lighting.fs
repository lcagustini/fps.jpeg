#version 330 core

out vec4 finalColor;

in vec3 fragNormal;
in vec3 fragPosition;
in vec2 fragTexCoord;

uniform int isMap;

uniform vec4 colDiffuse;

uniform vec3 viewPos;

uniform vec3 lightsPosition[10];
uniform vec3 lightsColor[10];
uniform int lightsLen;
uniform float time;

// 2D Random
float random (in vec2 st) {
    return fract(sin(dot(st.xy,
                         vec2(12.9898,78.233)))
                 * 43758.5453123);
}

// 2D Noise based on Morgan McGuire @morgan3d
// https://www.shadertoy.com/view/4dS3Wd
float noise (in vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);

    // Four corners in 2D of a tile
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));

    // Smooth Interpolation

    // Cubic Hermine Curve.  Same as SmoothStep()
    vec2 u = f*f*(3.0-2.0*f);
    // u = smoothstep(0.,1.,f);

    // Mix 4 coorners percentages
    return mix(a, b, u.x) +
            (c - a)* u.y * (1.0 - u.x) +
            (d - b) * u.x * u.y;
}

vec3 wallTexture(vec2 uv)
{
    if (fract(uv.x) < 0.1 || fract(uv.y) < 0.1) {
        return vec3(0.7, 0.7, 0.7);
    }

    return vec3(0.45, 0.2, 0.2);
}

vec3 groundTexture(vec2 uv)
{
    return vec3(0.35, 0.12, 0.2) + vec3(noise(uv * 75 + time) * 0.15);
}

void main()
{
    vec3 result = vec3(0);
    vec3 objectColor;

    if (isMap == 1) {
        if (fragTexCoord.x < 0.5 && fragTexCoord.y < 0.5) {
            //objectColor = vec3(0.0, 1.0, 1.0);
            objectColor = wallTexture(fragTexCoord * 300);
        } else if (fragTexCoord.x > 0.5 && fragTexCoord.y < 0.5) {
            //objectColor = vec3(1.0, 0.3, 0.4);
            objectColor = groundTexture(fragTexCoord);
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
