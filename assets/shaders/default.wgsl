struct VertexInput {
    @location(0) position: vec4f,
    @location(1) normal: vec4f,
    @location(2) texCoord: vec2f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(1) normal: vec4f,
    @location(2) texCoord: vec2f,
    @location(3) color: vec4f,
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

struct InstanceData {
    matTransform: mat4x4f,
    matNormal: mat4x4f,
    color: vec4f,
}

@group(0) @binding(0)
var<uniform> globals: GlobalData;

@group(0) @binding(1)
var<storage, read> instances: array<InstanceData>;

@group(1) @binding(0)
var mapAlbedo: texture_2d<f32>;

@group(1) @binding(1)
var sampler0: sampler;

@vertex
fn vs_main(in: VertexInput, @builtin(instance_index) instanceIndex: u32) -> VertexOutput {
    var out: VertexOutput;
    var instanceData = instances[instanceIndex];

    out.position = globals.matProjView * instanceData.matTransform * in.position;
    out.normal = in.normal;
    out.texCoord = in.texCoord;
    out.color = instanceData.color;

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput {
    var out: FragmentOutput;
    out.color = textureSample(mapAlbedo, sampler0, in.texCoord);
    out.color *= in.color;

    return out;
}