"""Stand-in for glslangValidator, built on the glslc that ships with the Android NDK.

DXVK's meson.build calls glslangValidator like this:
    glslangValidator [--quiet] [--version] -V --vn NAME INPUT -o OUTPUT
and expects a C header with the SPIR-V words in `const uint32_t NAME[]`.
That is the only form this script understands.

Set ANDROID_NDK_HOME (or ANDROID_NDK_ROOT) to the NDK folder.
"""
import os
import platform
import subprocess
import sys

STAGES = {".vert": "vertex", ".frag": "fragment", ".comp": "compute",
          ".geom": "geometry", ".tesc": "tesscontrol", ".tese": "tesseval"}


def find_glslc():
    ndk = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT")
    if not ndk:
        sys.exit("glslang shim: set ANDROID_NDK_HOME to your NDK folder")
    host = {"Windows": "windows-x86_64", "Linux": "linux-x86_64", "Darwin": "darwin-x86_64"}[platform.system()]
    exe = "glslc.exe" if platform.system() == "Windows" else "glslc"
    path = os.path.join(ndk, "shader-tools", host, exe)
    if not os.path.exists(path):
        sys.exit("glslang shim: glslc not found at " + path)
    return path


def main(argv):
    if "--version" in argv:
        print("glslang shim over NDK glslc")
        return 0

    var_name = src = out = None
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--vn":
            var_name = argv[i + 1]; i += 2
        elif a == "-o":
            out = argv[i + 1]; i += 2
        elif a in ("-V", "--quiet"):
            i += 1
        elif not a.startswith("-"):
            src = a; i += 1
        else:
            print("glslang shim: unsupported argument " + a, file=sys.stderr)
            return 2

    if not (var_name and src and out):
        print("glslang shim: missing --vn, input or -o", file=sys.stderr)
        return 2

    stage = STAGES.get(os.path.splitext(src)[1])
    if stage is None:
        print("glslang shim: unknown shader stage for " + src, file=sys.stderr)
        return 2

    cmd = [find_glslc(), "--target-env=vulkan1.1", "-fshader-stage=" + stage,
           "-mfmt=c", "-I", os.path.dirname(os.path.abspath(src)), "-o", "-", src]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        sys.stderr.write(res.stderr)
        return res.returncode

    with open(out, "w", newline="\n") as f:
        f.write("#pragma once\n")
        f.write("  const uint32_t %s[] = %s;\n" % (var_name, res.stdout.strip()))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
