#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform float u_time;
uniform float u_amplitude;
uniform float u_frequency;
uniform float u_speed;

out vec4 finalColor;

void main() {
    vec2 uv = fragTexCoord;
    uv.x += sin(uv.y * u_frequency + u_time * u_speed) * u_amplitude;
    uv.y += cos(uv.x * u_frequency + u_time * u_speed) * u_amplitude * 0.5;
    finalColor = texture(texture0, uv) * fragColor;
}