const std = @import("std");

const PathList = std.ArrayList([]const u8);
const BuildError = error{
    UnsupportedOs,
};

fn list(b: *std.Build, p: []const u8, options: struct {platform: std.Target.Os.Tag = .linux, kind: []const u8 = ".cpp"}) !PathList {
    var sources = try PathList.initCapacity(b.allocator, 256);

    var root = std.fs.cwd();
    var main = try root.openDir(p, .{.access_sub_paths=true, .iterate=true, .no_follow=true});
    defer main.close();
    var walker = try main.walk(b.allocator);
    defer walker.deinit();

    while(try walker.next()) |entry| {
        if( entry.kind != .file ) continue;

        const ext = std.fs.path.extension(entry.basename);
        if( std.mem.eql(u8, ext, options.kind) ) {
            try sources.append(b.allocator, b.dupe(entry.path));
        }
    }

    const path = try switch( options.platform ) {
        .windows => std.fs.path.join(b.allocator, &.{p, "win32lib"}),
        .linux => std.fs.path.join(b.allocator, &.{p, "linuxlib"}),
        else => error.UnsupportedOs,
    };

    var specific = try root.openDir(path, .{.iterate = true});
    var spe_walker = try specific.walk(b.allocator);
    defer spe_walker.deinit();

    while(try spe_walker.next()) |entry| {
        const ext = std.fs.path.extension(entry.basename);
        if( std.mem.eql(u8, ext, options.kind) ) {
            try sources.append(b.allocator, b.dupe(entry.path));
        }
    }

    return sources;
}

fn add_module(b: *std.Build, optimise: std.builtin.OptimizeMode, target: std.Build.ResolvedTarget, common: []const []const u8, module: []const u8) !void {
    var red_alert_cpp = try list(b, module, .{ .platform=target.result.os.tag, .kind=".cpp"});
    defer red_alert_cpp.deinit(b.allocator);
    var red_alert_asm = try list(b, module, .{ .platform=target.result.os.tag, .kind=".asm"});
    defer red_alert_asm.deinit(b.allocator);

    var red_alert_module = b.createModule(.{
        .optimize = optimise,
        .target = target,
        // .link_libcpp = true,
        .strip = optimise != .Debug,
        .pic = true,
    });
    red_alert_module.addCSourceFiles(.{
        .root = b.path(module),
        .files = red_alert_cpp.items,
        .flags = common,
    });
    red_alert_module.addIncludePath(b.path(module));

    for (red_alert_asm.items) |f| {
        red_alert_module.addAssemblyFile(b.path(f));
    }

    switch( target.result.os.tag ) {
        .windows => red_alert_module.addIncludePath(try b.path(module).join(b.allocator, "win32lib")),
        .linux => red_alert_module.addIncludePath(try b.path(module).join(b.allocator, "linuxlib")),
        else => return error.UnsupportedOs,
    }

    const red_alert = b.addExecutable(.{
        .name = module,
        .version = .{ .major = 0, .minor = 1, .patch = 0},
        .root_module = red_alert_module,
    });
    red_alert.linkLibCpp();
    b.installArtifact(red_alert);

    const run_step = b.step("run", "Run the app");
    const run_cmd = b.addRunArtifact(red_alert);
    run_step.dependOn(&run_cmd.step);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
}

pub fn build(b: *std.Build) !void {
    const optimise = b.standardOptimizeOption(.{});
    const target = b.standardTargetOptions(.{});
    const common = [_][]const u8{"-std=c++20", "-W", "-Wall", "-Wextra"};

    try add_module(b, optimise, target, &common, "RedAlert");
}
