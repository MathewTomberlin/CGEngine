#version 420

// The sky behind the scene: a gradient from the horizon colour up to the zenith colour, and the sun's disc with a soft glow.
// Each pixel's view ray comes from the camera's basis and lens, so the sky turns with the camera.
in vec2 ndc;

uniform vec3 camForward;
uniform vec3 camRight;
uniform vec3 camUp;
uniform float tanHalfFov;
uniform float aspect;
uniform vec3 horizonColour;
uniform vec3 zenithColour;
uniform vec3 sunDirection;   // unit vector towards the sun
uniform vec3 sunColour;      // black: no disc
uniform float sunCos;        // cosine of the sun's angular radius

out vec4 FragColor;

void main() {
    vec3 ray = normalize(camForward + camRight * (ndc.x * aspect * tanHalfFov) + camUp * (ndc.y * tanHalfFov));
    float up = clamp(ray.y, 0.0, 1.0);
    vec3 sky = mix(horizonColour, zenithColour, pow(up, 0.5));

    float toSun = dot(ray, sunDirection);
    float disc = smoothstep(sunCos - 0.0015, sunCos + 0.0015, toSun);
    float glow = pow(max(toSun, 0.0), 128.0) * 0.6;
    sky += sunColour * (disc * 4.0 + glow);

    FragColor = vec4(sky, 1.0);
}
