#ifndef EMULATOR_SRC_LOCAL_FSR_NATIVE_H_
#define EMULATOR_SRC_LOCAL_FSR_NATIVE_H_
// AMD FidelityFX FSR 3 built from source (3rdparty/fidelityfx, SDK 1.1.4) on Linux, where AMD ships no runtime for the
// ffx-api the Windows FSR backend loads (amd_fidelityfx_vk.dll, frame-gen.h). Any Vulkan GPU.
// KYTY_UPSCALER=fsr: FSR 3 upscaling in place of Sony's PS5 Pro upscaler the game dispatches, from the inputs DLSS takes
// there (renderSuperResolution.cpp: the game's jittered HDR color before its post-processing, its depth and motion
// vectors, the jitter and the reset flag), so the HUD and the post-processing are drawn on the upscaled image.
// KYTY_FSR_SHARPNESS=<0..1> (default 0.6): FSR's RCAS sharpening of the upscaled image (0: none). At Boletaria's start
// (1080 -> 2160) 0.8 had a quarter more edge detail than 0.3; the game's jitter as DLSS takes it served FSR as well as
// any sign change (live switches fsr_jitter_mode, fsr_sharpness_pct, fsr_mv_mode, fsr_debug_view: live-switch.h).
// Elsewhere (Windows) the functions answer false.
#include "local/frame-gen.h"

namespace Libs::Graphics {
struct GraphicContext;
namespace FsrNative {

// KYTY_UPSCALER=fsr in a build with the FidelityFX sources.
[[nodiscard]] bool UpscalerRequested();
// GPU thread or the recording worker: records the upscale of `inputs` (color, motion and depth in shader-read layouts,
// the output general) into `command`; false when it could not (the caller stretches instead). The images are left in
// their layouts.
[[nodiscard]] bool Upscale(GraphicContext& graphics, vk::CommandBuffer command, const FrameGen::UpscaleInputs& inputs);

} // namespace FsrNative
} // namespace Libs::Graphics

#endif // EMULATOR_SRC_LOCAL_FSR_NATIVE_H_
