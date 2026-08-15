const std = @import("std");

// Mirrors compile_flags.txt, minus the GCC-only flags clang rejects
// (-Wduplicated-cond, -Wduplicated-branches, -Wlogical-op, -Warray-bounds=2).
const c_flags = [_][]const u8{
    "-std=c23",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-Wpedantic",
    "-Wshadow",
    "-Wconversion",
    "-Wnull-dereference",
    "-Wdouble-promotion",
    "-Wformat=2",
    "-Wundef",
    "-Wcast-align",
    "-Wcast-qual",
    "-Wwrite-strings",
    "-Wpointer-arith",
    "-Wswitch-enum",
    "-Wuninitialized",
    "-Wstack-protector",
    "-fstack-protector-strong",
    "-Warray-bounds",
    "-Wjump-misses-init",
    "-Wvla",
    "-Wstrict-prototypes",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    if (target.result.os.tag == .emscripten) {
        buildWeb(b, optimize);
        return;
    }

    const exe_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    exe_mod.addCMacro(if (optimize == .Debug) "DEBUG" else "NDEBUG", "1");
    exe_mod.addIncludePath(b.path("src"));
    exe_mod.addCSourceFile(.{
        .file = b.path("src/unity.c"),
        .flags = &c_flags,
    });

    const glfw_dep = b.dependency("zglfw", .{});
    exe_mod.linkLibrary(glfwLibrary(b, target, optimize, glfw_dep));

    const wgpu_dep = wgpuDependency(b, target) orelse return;
    exe_mod.addIncludePath(wgpu_dep.path("include"));
    exe_mod.addObjectFile(wgpuLibrary(wgpu_dep, target));
    linkWgpuSystemDependencies(exe_mod, target);

    const stb_dep = b.dependency("stb", .{});
    exe_mod.addIncludePath(stb_dep.path(""));
    exe_mod.linkLibrary(singleHeaderLibrary(b, target, optimize, .{
        .name = "stb",
        .dep = stb_dep,
        .impl = stbImplementation(b),
    }));

    const cgltf_dep = b.dependency("cgltf", .{});
    exe_mod.addIncludePath(cgltf_dep.path(""));
    exe_mod.linkLibrary(singleHeaderLibrary(b, target, optimize, .{
        .name = "cgltf",
        .dep = cgltf_dep,
        .impl = cgltfImplementation(b),
    }));

    const exe = b.addExecutable(.{
        .name = "aet95",
        .root_module = exe_mod,
    });
    b.installArtifact(exe);

    const run = b.addRunArtifact(exe);
    run.step.dependOn(b.getInstallStep());
    if (b.args) |args| run.addArgs(args);
    b.step("run", "Build and run the executable").dependOn(&run.step);
}

fn buildWeb(b: *std.Build, optimize: std.builtin.OptimizeMode) void {
    const emsdk = b.lazyDependency("emsdk", .{}) orelse return;
    const stb_dep = b.dependency("stb", .{});
    const stb_impl = stbImplementation(b);
    const cgltf_dep = b.dependency("cgltf", .{});
    const cgltf_impl = cgltfImplementation(b);
    const emsdk_root = emsdk.path("").getPath(b);
    const emsdk_script = b.pathJoin(&.{ emsdk_root, "emsdk" });
    const emcc_path = b.pathJoin(&.{ emsdk_root, "upstream", "emscripten", "emcc.py" });

    const make_emsdk_executable = b.addSystemCommand(&.{ "chmod", "+x", emsdk_script });
    const install_emsdk = b.addSystemCommand(&.{ emsdk_script, "install", "4.0.19" });
    install_emsdk.step.dependOn(&make_emsdk_executable.step);
    const activate_emsdk = b.addSystemCommand(&.{ emsdk_script, "activate", "4.0.19" });
    activate_emsdk.step.dependOn(&install_emsdk.step);
    const make_emcc_executable = b.addSystemCommand(&.{ "chmod", "+x", emcc_path });
    make_emcc_executable.step.dependOn(&activate_emsdk.step);

    const emcc = b.addSystemCommand(&.{emcc_path});
    emcc.step.dependOn(&make_emcc_executable.step);
    emcc.addArgs(&.{
        switch (optimize) {
            .Debug => "-O0",
            .ReleaseSafe, .ReleaseFast => "-O3",
            .ReleaseSmall => "-Oz",
        },
        if (optimize == .Debug) "-DDEBUG=1" else "-DNDEBUG=1",
        "--use-port=emdawnwebgpu:cpp_bindings=false",
        "--closure=1",
        "-sALLOW_MEMORY_GROWTH=1",
        "-sUSE_GLFW=3",
    });
    for (c_flags) |flag| emcc.addArg(flag);
    emcc.addArg(b.fmt("-I{s}", .{b.path("src").getPath(b)}));
    emcc.addArg(b.fmt("-I{s}", .{stb_dep.path("").getPath(b)}));
    emcc.addArg(b.fmt("-I{s}", .{cgltf_dep.path("").getPath(b)}));
    emcc.addFileArg(b.path("src/unity.c"));
    emcc.addFileArg(stb_impl);
    emcc.addFileArg(cgltf_impl);
    emcc.addArg("-o");
    const web_output = emcc.addOutputFileArg("aet95.html");

    const install_web = b.addInstallDirectory(.{
        .source_dir = web_output.dirname(),
        .install_dir = .{ .custom = "web" },
        .install_subdir = "",
    });
    b.getInstallStep().dependOn(&install_web.step);
}

fn wgpuDependency(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
) ?*std.Build.Dependency {
    const name: []const u8 = switch (target.result.os.tag) {
        .linux => switch (target.result.cpu.arch) {
            .x86_64 => "wgpu_linux_x86_64",
            .aarch64 => "wgpu_linux_aarch64",
            else => @panic("wgpu-native has no prebuilt library for this Linux architecture"),
        },
        .windows => switch (target.result.cpu.arch) {
            .x86_64 => if (target.result.abi == .msvc)
                "wgpu_windows_x86_64_msvc"
            else
                "wgpu_windows_x86_64_gnu",
            else => @panic("wgpu-native has no configured prebuilt library for this Windows architecture"),
        },
        .macos => switch (target.result.cpu.arch) {
            .x86_64 => "wgpu_macos_x86_64",
            .aarch64 => "wgpu_macos_aarch64",
            else => @panic("wgpu-native has no prebuilt library for this macOS architecture"),
        },
        else => @panic("wgpu-native is only configured for Linux, Windows, and macOS"),
    };
    return b.lazyDependency(name, .{});
}

fn wgpuLibrary(
    dep: *std.Build.Dependency,
    target: std.Build.ResolvedTarget,
) std.Build.LazyPath {
    return if (target.result.os.tag == .windows and target.result.abi == .msvc)
        dep.path("lib/wgpu_native.lib")
    else
        dep.path("lib/libwgpu_native.a");
}

fn linkWgpuSystemDependencies(
    mod: *std.Build.Module,
    target: std.Build.ResolvedTarget,
) void {
    switch (target.result.os.tag) {
        .linux => mod.link_libcpp = true,
        .windows => {
            mod.linkSystemLibrary(if (target.result.abi == .msvc) "d3dcompiler" else "d3dcompiler_47", .{});
            if (target.result.abi == .msvc) {
                mod.linkSystemLibrary("RuntimeObject", .{});
            } else {
                mod.linkSystemLibrary("api-ms-win-core-winrt-error-l1-1-0", .{});
                // The mingw prebuilt is a Rust staticlib built against the GCC
                // unwinder; LLVM's libunwind supplies the _Unwind_* personality
                // routines it expects.
                mod.linkSystemLibrary("unwind", .{});
            }
            inline for (.{
                "opengl32", "gdi32", "oleaut32", "ole32", "ws2_32", "userenv", "propsys",
            }) |library| mod.linkSystemLibrary(library, .{});
        },
        .macos => {
            const system_sdk = mod.owner.dependency("system_sdk", .{});
            mod.addSystemFrameworkPath(system_sdk.path("macos12/System/Library/Frameworks"));
            mod.addSystemIncludePath(system_sdk.path("macos12/usr/include"));
            mod.addLibraryPath(system_sdk.path("macos12/usr/lib"));
            mod.linkFramework("Foundation", .{});
            mod.linkFramework("QuartzCore", .{});
            mod.linkFramework("Metal", .{});
        },
        else => unreachable,
    }
}

fn glfwLibrary(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    dep: *std.Build.Dependency,
) *std.Build.Step.Compile {
    const root = "libs/glfw/";
    const src = root ++ "src/";
    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    mod.addIncludePath(dep.path(root ++ "include"));
    mod.addIncludePath(dep.path(src));
    mod.addCSourceFiles(.{ .root = dep.path(""), .files = &glfw_base_sources });

    switch (target.result.os.tag) {
        .linux => {
            const system_sdk = b.dependency("system_sdk", .{});
            mod.addSystemIncludePath(system_sdk.path("linux/include"));
            mod.addSystemIncludePath(system_sdk.path("linux/include/wayland"));
            mod.addIncludePath(dep.path(src ++ "wayland"));
            mod.addCMacro("_GLFW_X11", "1");
            mod.addCMacro("_GLFW_WAYLAND", "1");
            mod.addCSourceFiles(.{ .root = dep.path(""), .files = &glfw_linux_sources });
        },
        .windows => {
            mod.addCMacro("_GLFW_WIN32", "1");
            mod.addCSourceFiles(.{ .root = dep.path(""), .files = &glfw_windows_sources });
            mod.linkSystemLibrary("gdi32", .{});
            mod.linkSystemLibrary("user32", .{});
            mod.linkSystemLibrary("shell32", .{});
        },
        .macos => {
            const system_sdk = b.dependency("system_sdk", .{});
            mod.addSystemFrameworkPath(system_sdk.path("macos12/System/Library/Frameworks"));
            mod.addSystemIncludePath(system_sdk.path("macos12/usr/include"));
            mod.addLibraryPath(system_sdk.path("macos12/usr/lib"));
            mod.addCMacro("_GLFW_COCOA", "1");
            mod.addCSourceFiles(.{ .root = dep.path(""), .files = &glfw_macos_sources });
            mod.linkSystemLibrary("objc", .{});
            inline for (.{
                "IOKit", "CoreFoundation", "AppKit", "CoreServices", "CoreGraphics", "Foundation",
            }) |framework| mod.linkFramework(framework, .{});
        },
        else => @panic("GLFW is only configured for Linux, Windows, and macOS"),
    }

    const lib = b.addLibrary(.{ .name = "glfw", .root_module = mod });
    lib.installHeadersDirectory(dep.path(root ++ "include"), "", .{});
    return lib;
}

const glfw_base_sources = [_][]const u8{
    "libs/glfw/src/platform.c",
    "libs/glfw/src/monitor.c",
    "libs/glfw/src/init.c",
    "libs/glfw/src/vulkan.c",
    "libs/glfw/src/input.c",
    "libs/glfw/src/context.c",
    "libs/glfw/src/window.c",
    "libs/glfw/src/osmesa_context.c",
    "libs/glfw/src/egl_context.c",
    "libs/glfw/src/null_init.c",
    "libs/glfw/src/null_monitor.c",
    "libs/glfw/src/null_window.c",
    "libs/glfw/src/null_joystick.c",
};

const glfw_linux_sources = [_][]const u8{
    "libs/glfw/src/posix_time.c",
    "libs/glfw/src/posix_thread.c",
    "libs/glfw/src/posix_module.c",
    "libs/glfw/src/posix_poll.c",
    "libs/glfw/src/xkb_unicode.c",
    "libs/glfw/src/linux_joystick.c",
    "libs/glfw/src/x11_init.c",
    "libs/glfw/src/x11_monitor.c",
    "libs/glfw/src/x11_window.c",
    "libs/glfw/src/glx_context.c",
    "libs/glfw/src/wl_init.c",
    "libs/glfw/src/wl_monitor.c",
    "libs/glfw/src/wl_window.c",
};

const glfw_windows_sources = [_][]const u8{
    "libs/glfw/src/wgl_context.c",
    "libs/glfw/src/win32_thread.c",
    "libs/glfw/src/win32_init.c",
    "libs/glfw/src/win32_monitor.c",
    "libs/glfw/src/win32_time.c",
    "libs/glfw/src/win32_joystick.c",
    "libs/glfw/src/win32_window.c",
    "libs/glfw/src/win32_module.c",
};

const glfw_macos_sources = [_][]const u8{
    "libs/glfw/src/posix_thread.c",
    "libs/glfw/src/posix_module.c",
    "libs/glfw/src/posix_poll.c",
    "libs/glfw/src/nsgl_context.m",
    "libs/glfw/src/cocoa_time.c",
    "libs/glfw/src/cocoa_joystick.m",
    "libs/glfw/src/cocoa_init.m",
    "libs/glfw/src/cocoa_window.m",
    "libs/glfw/src/cocoa_monitor.m",
};

// stb and cgltf ship as headers only: one translation unit per dependency
// stamps out the implementations. Neither is clean under our warning set, so
// they get their own flags.
fn singleHeaderLibrary(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    options: struct {
        name: []const u8,
        dep: *std.Build.Dependency,
        impl: std.Build.LazyPath,
    },
) *std.Build.Step.Compile {
    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .sanitize_c = .off,
    });
    mod.addIncludePath(options.dep.path(""));
    mod.addCSourceFile(.{ .file = options.impl, .flags = &.{"-std=c23"} });

    return b.addLibrary(.{ .name = options.name, .root_module = mod });
}

fn stbImplementation(b: *std.Build) std.Build.LazyPath {
    return b.addWriteFiles().add("stb_impl.c",
        \\#ifdef __clang__
        \\#pragma clang diagnostic ignored "-Weverything"
        \\#endif
        \\#define STB_IMAGE_IMPLEMENTATION
        \\#define STB_TRUETYPE_IMPLEMENTATION
        \\#include "stb_image.h"
        \\#include "stb_truetype.h"
        \\
    );
}

fn cgltfImplementation(b: *std.Build) std.Build.LazyPath {
    return b.addWriteFiles().add("cgltf_impl.c",
        \\#ifdef __clang__
        \\#pragma clang diagnostic ignored "-Weverything"
        \\#endif
        \\#define CGLTF_IMPLEMENTATION
        \\#include "cgltf.h"
        \\
    );
}
