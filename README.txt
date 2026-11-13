subgroup_probe: does the Vulkan driver put a 64-thread compute workgroup on 32-wide subgroups in order
(threads 0-31 in subgroup 0 and 32-63 in subgroup 1, each lane in order)? KytyPS5's "cooperative halves"
assume it; the Vulkan spec does not promise it.

Run
  subgroup_probe.exe            tests GPU 0
  subgroup_probe.exe 1          tests GPU 1 (if GPU 0 is an integrated GPU)
It prints its result and writes the same text to subgroup_probe.txt next to it. Nothing is installed or
changed. The exe imports only KERNEL32.dll (static C runtime) and loads the Vulkan loader (vulkan-1.dll)
at run time.

Files
  probe.cpp       the program (Vulkan C API, no other libraries)
  probe.comp      the compute shader: each thread writes its subgroup, lane and subgroup size
  shaders.h       probe.comp compiled to SPIR-V, 9 variants (workgroups 64x1, 8x8, 16x4; plain, with an
                  image load, with a texture sample), made by gen_shaders.py
  gen_shaders.py  makes shaders.h from probe.comp with glslc (shaderc)

Build it yourself
  python3 gen_shaders.py
  Windows:  cl /std:c++17 /O1 /EHsc /MT /D_CRT_SECURE_NO_WARNINGS /I<Vulkan-Headers>\include probe.cpp /Fe:subgroup_probe.exe
            (or clang-cl with the same options)
  Linux:    clang++ -std=c++17 -O1 -I<Vulkan-Headers>/include probe.cpp -ldl -o subgroup_probe

The included exe was cross-built on Linux with clang-cl --target=x86_64-pc-windows-msvc -fuse-ld=lld
/std:c++17 /O1 /EHsc /MT /D_CRT_SECURE_NO_WARNINGS (Windows SDK and CRT from xwin).
SHA-256 of subgroup_probe.exe: 56eb0dfb743b4395ac38a48a33b6497c58d0cf80a9b360d504f9937f468ffda8
