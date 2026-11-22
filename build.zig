const std = @import ("std");

pub fn build (b: *std.Build) void {

    const name = b.option ([]const u8, "name", "lib name") orelse "echoes";
    const version = b.option ([]const u8, "version", "lib version") orelse "0.0.0";

    std.debug.print ("Name: {any}, Version: {any}\n", .{name, version});

    const config_h = b.addConfigHeader (.{ .style = .blank, .include_path = "config.h"}, .{
        .NAME = name,
        .VERSION = version,
    });

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .safe });

    const cflags: []const []const u8 = &.{ "-std=gnu11", "-Wall", "-Wextra", "-Werror" };

    const platform_source: []const u8 = switch (target.result.os.tag) {
        .linux => "driver-linux.c",
        .macos => "driver-mac.c",
        .windows => "driver-windows.c",
        else => @panic ("OS not supported!!")
    };

    const ead_mod = b.createModule (.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    ead_mod.addIncludePath (b.path ("lib/ead/include"));
    ead_mod.addCSourceFiles (.{
        .root = b.path ("lib/ead/src"),
        .files = &.{ "IO.c", platform_source },
        .flags = cflags,
    });

    if (target.result.os.tag == .linux) {
        ead_mod.linkSystemLibrary ("asound", .{});
    }

    if (target.result.os.tag == .macos) {
        ead_mod.linkFramework ("AudioToolbox", .{});
    }

    ead_mod.addConfigHeader (config_h);

    const ead = b.addLibrary (.{
        .name = "ead",
        .linkage = .static,
        .root_module = ead_mod,
    });
    b.installArtifact (ead);

}
