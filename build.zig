const std = @import ("std");

pub fn build (b: *std.Build) void {
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

    const ead = b.addLibrary (.{
        .name = "ead",
        .linkage = .static,
        .root_module = ead_mod,
    });
    b.installArtifact (ead);
}
