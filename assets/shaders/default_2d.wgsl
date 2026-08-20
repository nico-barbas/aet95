struct VertexInput {
    @location(0) position: vec2f,
    @location(1) texCoord: vec3f,
    @location(2) color: vec4f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(1) texCoord: vec3f,
    @location(2) color: vec4f,
}

struct FragmentOutput {
    @location(0) color: vec4f,
}

struct GlobalData {
    matProjView: mat4x4f,
}

@group(0) @binding(0)
var<uniform> globals: GlobalData;

@group(0) @binding(1)
var sampler0: sampler;

@group(0) @binding(2)
var texture0: texture_2d<f32>;

@group(0) @binding(3)
var texture1: texture_2d<f32>;

@group(0) @binding(4)
var texture2: texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = globals.matProjView * vec4f(in.position, 0, 1);
    out.texCoord = in.texCoord;
    out.color = in.color;

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput {
    var out: FragmentOutput;
    out.color = texIndexToColor(u32(in.texCoord.z), in.texCoord.xy) * in.color;

    return out;
}

fn texIndexToColor(index: u32, texCoord: vec2f) -> vec4f {
    switch index {
        case 0: {
            return textureSample(texture0, sampler0, texCoord);
        }
        case 1: {
            return textureSample(texture1, sampler0, texCoord);
        }
        case 2: {
            return textureSample(texture2, sampler0, texCoord);
        }
        default: {
            return vec4f(1, 0, 1, 1);
        }
    }
}