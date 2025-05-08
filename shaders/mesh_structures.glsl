
struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;

    // skinning data
    ivec4 jointIndices;
    vec4 weights;
};
