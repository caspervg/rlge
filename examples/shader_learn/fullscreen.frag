#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

// Standard uniforms provided by the engine
uniform float u_time;
uniform vec2 u_resolution;
uniform vec2 u_mouse;

out vec4 finalColor;

float dot2(in vec2 v) { return dot(v,v); }
float dot2(in vec3 v) { return dot(v,v); }
float ndot(in vec2 a, in vec2 b) { return a.x*b.x - a.y*b.y; }

float sdPlane(vec3 p) {
    return p.y;
}

float sdSphere(vec3 p, float s) {
    return length(p)-s;
}

float sdBox(vec3 p, vec3 b) {
    vec3 d = abs(p) - b;
    return min(max(d.x,max(d.y,d.z)),0.0) + length(max(d,0.0));
}

float smin(float a, float b, float k) {
    float h = max(k-abs(a-b), 0.0) / k;
    return min(a, b) - h*h*h*k*(1./6.);
}

mat2 rot2d(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
}

mat3 rot3d(vec3 axis, float angle) {
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;

    return mat3(
        oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s, oc * axis.z * axis.x + axis.y * s,
        oc * axis.y * axis.x + axis.z * s, oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s,
        oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s, oc * axis.z * axis.z + c
    );
}

vec3 rot3d(vec3 p, vec3 axis, float angle) {
    return mix(dot(axis, p) + axis, p, cos(angle)) + cross(axis, p) * sin(angle);
}

float map(vec3 p) {
    vec3 spherePos = vec3(sin(u_time)*3., 0, 0);

    float sphere = sdSphere(p - spherePos, 1.0);

    vec3 q = p;

    q = mod(p, 10) - .5;
    float box = sdBox(p * 4., vec3(.75)) / 4.;
    //float box = sdBox(q, vec3(.75));

    float ground = p.y + .75;

    return smin(ground, smin(sphere, box, 2.), .5);
}

void main() {
    vec2 uv = (gl_FragCoord.xy * 2. - u_resolution.xy ) / u_resolution.y;
    //vec2 u_mouse_norm = (u_mouse * 2. - u_resolution.xy ) / u_resolution.y;

    vec3 ro = vec3(0, 0, -3);
    vec3 rd = normalize(vec3(uv, 1));
    vec3 col = vec3(0);

    float t = 0.;
    for (int i = 0; i < 100; i++) {
        vec3 p = ro + rd * t;
        float d = map(p);
        t += d; // March the ray!

        if (d < .001 || t > 100.) break;
    }

    col = vec3(t * .2);

    finalColor = vec4(col, 1);
}
