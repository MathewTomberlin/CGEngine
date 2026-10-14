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
uniform vec3 moonDirection;  // unit vector towards the moon
uniform vec3 moonColour;     // black: no disc
uniform float moonCos;       // cosine of the moon's angular radius
uniform float moonLit;       // the share of the moon's disc that is lit: 0 at a new moon, 1 at full
uniform vec3 moonLitAxis;    // the world direction the lit side faces

out vec4 FragColor;

void main() {
    vec3 ray = normalize(camForward + camRight * (ndc.x * aspect * tanHalfFov) + camUp * (ndc.y * tanHalfFov));
    float up = clamp(ray.y, 0.0, 1.0);
    vec3 sky = mix(horizonColour, zenithColour, pow(up, 0.5));

    float toSun = dot(ray, sunDirection);
    float disc = smoothstep(sunCos - 0.0015, sunCos + 0.0015, toSun);
    float glow = pow(max(toSun, 0.0), 128.0) * 0.6;
    sky += sunColour * (disc * 4.0 + glow);

    // The moon: a disc with its lit part on the side moonLitAxis gives. The edge between the lit and dark parts (the terminator)
    // is placed so that the lit share of the disc is moonLit. A little earthshine shows the dark part. Only above the horizon.
    float toMoon = dot(ray, moonDirection);
    float moonDisc = smoothstep(moonCos - 0.0015, moonCos + 0.0015, toMoon);
    if (moonDisc > 0.0 && moonLit > 0.0) {
        float radius = sqrt(max(1.0 - moonCos * moonCos, 1e-6));   // the disc's radius on the sky, in radians
        vec3 axis = moonLitAxis - moonDirection * dot(moonLitAxis, moonDirection);
        float lit = moonLit;
        if (length(axis) > 1e-4) {
            vec3 offset = (ray - moonDirection * toMoon) / radius;   // in disc radii, from the centre
            float along = dot(offset, normalize(axis));                 // towards the lit side is positive
            float edge = 1.0 - 2.0 * moonLit;                           // -1 full, 0 half, 1 new
            lit = smoothstep(edge - 0.04, edge + 0.04, along);
        }
        float visible = smoothstep(-0.02, 0.02, ray.y);
        sky += moonColour * moonDisc * visible * (lit + 0.03) * 1.5;
    }

    FragColor = vec4(sky, 1.0);
}
