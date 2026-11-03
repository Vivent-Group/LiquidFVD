#version 130

in vec3 aPosition;

uniform mat4 projectionMatrix;
uniform mat4 modelMatrix;
uniform vec3 center;
uniform float radius;

out vec3 vNormal;

void main(void)
{
    vNormal = aPosition;
    gl_Position = projectionMatrix * modelMatrix * vec4(center + radius*aPosition, 1.0);
}
