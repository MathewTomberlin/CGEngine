#version 420

// One triangle that covers the screen, from the vertex index alone: (-1,-1), (3,-1) and (-1,3).
out vec2 ndc;

void main() {
    vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    ndc = corner * 2.0 - 1.0;
    gl_Position = vec4(ndc, 1.0, 1.0);
}
