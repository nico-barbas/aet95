struct VertexInput {
    @location(0) position: vec3f,
    @location(1) color: u32,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(1) color: vec4f,
}

struct FragmentOutput {
    @location(0) color: vec4f,
}

struct GlobalData {
    matProj: mat4x4f,
    matView: mat4x4f,
    matProjView: mat4x4f,
    totalTime: f32,
}

@group(0) @binding(0)
var<uniform> globals: GlobalData;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = globals.matProjView * vec4f(in.position, 1.0);

    let r = f32((in.color >> 24) & 0xFFu) / 255.0;
    let g = f32((in.color >> 16) & 0xFFu) / 255.0;
    let b = f32((in.color >> 8) & 0xFFu) / 255.0;
    let a = f32(in.color & 0xFFu) / 255.0;

    out.color = vec4f(r, g, b, a);

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput {
    var out: FragmentOutput;
    out.color = in.color;

    return out;
}