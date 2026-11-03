#version 130

in vec3 vNormal;
uniform vec3 color;
out vec4 oFragColor;

void main(void)
{
    // bright, slightly shaded ball so bookmarks stand out against sky and track
    float light = 0.55 + 0.45*max(dot(normalize(vNormal), normalize(vec3(0.3, 1.0, 0.2))), 0.0);
    oFragColor = vec4(color*light, 1.0);
}
