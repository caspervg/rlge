#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

// Standard uniforms provided by the engine
uniform float u_time;
uniform vec2 u_resolution;
uniform vec2 u_mouse;

out vec4 finalColor;

#define S(a, b, t) smoothstep(a, b, t)
#define sat(x) clamp(x, 0., 1.)


float remap(float a, float b, float c, float d, float t) {
    return sat(((t - a)/(b - a))) * (d - c) + c;
}

float remap01(float a, float b, float t) {
    return sat(remap(a, b, 0., 1., t));
}

vec2 within(vec2 uv, vec4 rect) {
    return (uv - rect.xy)/(rect.zw - rect.xy);
}

vec4 Eye(vec2 uv) {
    // Renormalize within rectangle to [-0.5, 0.5]
    uv = uv - .5;
    float d = length(uv);

    vec4 irisCol = vec4(.3, .5, 1., 1.);
    vec4 col = mix(vec4(1.), irisCol, S(.1, .7, d) * 0.5);

    // Eye shadow
    col.rgb *= 1. - S(.45, .5, d) * 0.5 * sat(-uv.y - uv.x);

    // Iris outline
    col.rgb = mix(col.rgb, vec3(0.), S(.3, .28, d));

    // Iris
    irisCol.rgb *= 1. + S(.3, .05, d);
    col.rgb = mix(col.rgb, irisCol.rgb, S(.28, .25, d));

    // Pupil
    col.rgb = mix(col.rgb, vec3(0.), S(.16, .14, d));

    // Highlights
    float highlight = S(.1, .09, length(uv - vec2(-.15, .15))); // highlight 1
    highlight += S(.07, .05, length(uv + vec2(-.08, .08))); // highlight 2
    col.rgb = mix(col.rgb, vec3(1.), highlight);

    col.a = S(.5, .48, d);
    return col;
}

vec4 Mouth(vec2 uv) {
    vec4 col = vec4(0.);

    return col;
}

vec4 Head(vec2 uv) {
    vec4 col = vec4(.9, .65, .1, 1.);

    // Circle, with alpha based on distance from center
    float d = length(uv);
    col.a = S(.5, .49, d);

    // Edge shading
    float edgeShade = remap01(.35, .5, d);
    edgeShade *= edgeShade;
    col.rgb *= 1. - edgeShade * .3;

    // Outline
    col.rgb = mix(col.rgb, vec3(.6, .3, .1), S(.47, .48, d));

    // Highlight with gradient
    float highlight = S(.41, .405, d);
    highlight *= remap(.41, -.1, .75, 0., uv.y);
    col.rgb = mix(col.rgb, vec3(1.), highlight);

    // Cheek
    d = length(uv - vec2(.25, -.2));
    float cheek = S(.2, .01, d) * .4;
    cheek *= S(.17, .16, d);
    col.rgb = mix(col.rgb, vec3(1., .1, .1), cheek);

    return col;
}

vec4 smiley(vec2 uv) {
    vec4 col = vec4(0.);

    // Symmetric head
    uv.x = abs(uv.x);

    vec4 head = Head(uv);
    vec4 eye = Eye(within(uv, vec4(.03, -.1, .37, .25)));
    vec4 mouth = Mouth(within(uv, vec4(-.3, -.4, .3, -.1)));

    col = mix(col, head, head.a);
    col = mix(col, eye, eye.a);
    col = mix(col, mouth, mouth.a);

    return col;
}

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution;
    uv -= 0.5;
    uv.x *= u_resolution.x / u_resolution.y;


    // Output to screen
    finalColor = smiley(uv);
}
