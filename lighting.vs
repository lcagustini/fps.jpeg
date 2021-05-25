#version 330 core
in vec3 vertexPosition;
in vec3 vertexNormal;

out vec3 fragNormal;
out vec3 fragPosition;

uniform mat4 mvp;
uniform mat4 matNormal;
uniform mat4 matModel;

void main()
{
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragNormal = mat3(transpose(inverse(matModel))) * vertexNormal;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
