## New in this build

A hotfix for v20261021's precompile. The shaders are the same as in v20261021: your precompiled store stays valid.

- **A precompile that never finished (issue #107):** on PCs where the game had been played before, v20261021's
  precompile split its shards without end, and its "waiting" count kept growing. It loaded the game's own pipeline
  cache (`_PipelineCache\local`), whose data holds the compression dictionary NVIDIA's driver trained in play, and with
  that no pipeline could be kept. The precompile no longer loads that cache, and its shards are smaller (about 100
  pipelines each, at most 160). If you stopped a v20261021 precompile, run it again: what it finished is kept.
- **1080p output in the launcher:** "Game output" picks 4K (as on the PS5) or 1080p, the game's own HD mode, also
  `run.cmd -HdOutput`. In it the souls counter's backdrop is now right (a contribution by @intellq, PR #106), and FSR 3.1
  frame generation keeps the HUD sharp.
- **Cheats one by one:** the launcher's "Cheats" checkbox applies `_Patches\<title id>.json`, and "Select..." picks
  which of its cheats are on (a contribution by @Myoko, PR #105).
- **Linux:** source builds with clang can use profile-guided optimization, and the README explains how to make a
  profile (+7.5% on an i5-14400 with an RX 9060 XT; contributions by @the2ndday, PRs #108 and #109).

## New in v20261021

The shaders are the same as in v20261020b: your precompiled store stays valid. Running the precompile again adds the
new boss-death shaders below (only the new part is compiled).

- **A 1080p output (optional, new):** on a 1080p screen, Demon's Souls can now run its own HD mode: its
  post-processing and HUD at 1920x1080 instead of 3840x2160 scaled down afterwards. Less work for the graphics card:
  at the 1-1 bench spot on an RTX 5090, 69.8 -> 78.3 fps. Turn it on with `run.cmd -Set KYTY_HD_OUTPUT=1` (not
  together with DLSS: `-Upscaler dlss` makes the 4K output). Known issue: in this mode the souls counter's dark strip
  shows as a small black square; the launcher gets a setting for it once that is fixed. (A contribution by @intellq,
  PR #103.)
- **AMD FSR 3.1 frame generation (2x)** for graphics cards without DLSS frame generation: the launcher's "AMD FSR 3
  frame generation" or `run.cmd -Fsr`, with AMD's signed `amd_fidelityfx_vk.dll` (from AMD's FidelityFX SDK) next to
  the emulator. (Contributions by @iSamigor, PRs #96 and #99.)
- **Add-on content (DLC):** add-ons go in `Content\PPSA01341\`, one folder each, checked by their content id.
  Add-ons without files of their own, such as a deluxe edition's extras, can be declared in `dlc_emu.ini`. The
  README's "Add-on content (DLC)" explains both. (A contribution by @Myoko, PR #104.)
- **Your own keys:** `--keymap` lines in `launch.json` now take key names as people write them (`LeftShift`,
  `LeftCtrl`, `Spacebar`, `Esc`). Space and Esc can be mapped; when Space is mapped, the Pause key pauses. A line the
  emulator cannot use is skipped and named in the run log instead of stopping the start. See the README's "Your own
  keys" (issue #91).
- **Ultrawide:** with `-Aspect 21:9` or `32:9`, adding `-AspectFit` keeps the movies (and the title and loading
  screens) at 16:9 with black bars at the sides. Without it they fill the screen as before (issue #97).
- **Game on a hard disk:** the README now says what took a player from freezes to full speed: the game packed into a
  `.zar` on the hard disk and the emulator folder on an SSD (issue #91).
- **Fixes:**
  - AMD Radeon RX 9000/7000/6000: the emulator could end after a few minutes of play with "TextureCache: invalid
    image view description" (issue #102).
  - "TextureCache: color target requires rediscovery before final acquisition" could end the emulator on fast travel
    from the Nexus to Boletaria (issue #94).
  - DLSS super resolution showed a black 3D image on game version 01.005.000 (thanks to @Al3cko for the analysis), and
    with the NVIDIA app's DLSS override, where the game's own upscale is now used instead (issue #87).
  - A button press could be lost during a stutter (a contribution by @Vorshim92, PR #100).
  - A boss death could freeze the game for up to 51 s the first time: its compute shaders are now precompiled in all
    the forms the game uses them (PR #96).
  - An update precompile no longer makes every stored pipeline again just to copy it: updates are much faster
    (issue #85).
- **Linux:** DLSS super resolution, DLAA and DLSS frame generation through NVIDIA NGX (a contribution by @Tutuzaooo,
  PR #101).
- **Internal cleanup:** about 6,000 lines of finished experiments and diagnostics are gone, and the release settings
  are now the emulator's own defaults (`launch.json` no longer lists them; `-Set KEY=` still turns one off). Same
  behaviour and frame rates (checked on all 16 maps), same shaders.
- **Thanks** to @intellq, @Myoko, @iSamigor, @Vorshim92, @Tutuzaooo and @Al3cko for their contributions, and to
  everyone who reported problems and tested.

Earlier changes are on the release pages of v20261020b and before.

## 本版本的变化

这是针对 v20261021 预编译问题的修复版本。着色器与 v20261021 相同：你已经预编译的存储继续有效。

- **预编译一直无法结束（issue #107）：** 在之前已经玩过游戏的电脑上，v20261021 的预编译会不停地拆分分片，"waiting" 的数量越来越多。原因是它读取了游戏自己的管线缓存（`_PipelineCache\local`），其中包含 NVIDIA 驱动在游玩时训练出的压缩字典，结果一个管线都无法保存。现在预编译不再读取这个缓存，分片也更小了（每个约 100 个管线，最多 160 个）。如果你中断过 v20261021 的预编译，重新运行即可：已经完成的部分会保留。
- **启动器中的 1080p 输出：** "Game output" 可以选择 4K（与 PS5 相同）或 1080p（游戏自带的高清模式），也可以用 `run.cmd -HdOutput`。这个模式下魂数旁边的深色条现在显示正常（@intellq 的贡献，PR #106），FSR 3.1 帧生成时 HUD 也保持清晰。
- **逐个选择作弊项：** 启动器的 "Cheats" 复选框会应用 `_Patches\<title id>.json`，"Select..." 可以选择启用其中哪些作弊项（@Myoko 的贡献，PR #105）。
- **Linux：** 用 clang 从源码构建时可以使用配置文件引导优化（PGO），README 说明了如何生成配置文件（i5-14400 + RX 9060 XT 上 +7.5%；@the2ndday 的贡献，PR #108 和 #109）。

## v20261021 的变化

着色器与 v20261020b 相同：你已经预编译的存储继续有效。再运行一次预编译会加入下面提到的 Boss 死亡着色器（只编译新增的部分）。

- **1080p 输出（可选，新增）：** 在 1080p 显示器上，恶魔之魂现在可以使用它自己的高清模式：后期处理和 HUD 直接以 1920x1080 渲染，不再先渲染 3840x2160 再缩小。显卡负担更小：RTX 5090 在 1-1 测试点从 69.8 提升到 78.3 fps。用 `run.cmd -Set KYTY_HD_OUTPUT=1` 开启（不能与 DLSS 同时使用：`-Upscaler dlss` 输出的是 4K）。已知问题：这个模式下魂数旁边的深色条会显示成一个小黑方块；修好之后启动器会加上这个选项。（@intellq 的贡献，PR #103。）
- **AMD FSR 3.1 帧生成（2 倍）**，适用于不支持 DLSS 帧生成的显卡：在启动器里勾选 "AMD FSR 3 frame generation"，或用 `run.cmd -Fsr`，需要把 AMD 签名的 `amd_fidelityfx_vk.dll`（来自 AMD 的 FidelityFX SDK）放在模拟器旁边。（@iSamigor 的贡献，PR #96 和 #99。）
- **附加内容（DLC）：** 附加内容放在 `Content\PPSA01341\`，每个一个文件夹，按内容 ID 校验。没有自己文件的附加内容（例如豪华版的额外内容）可以写在 `dlc_emu.ini` 里声明。README 的 "Add-on content (DLC)" 一节说明了两种方式。（@Myoko 的贡献，PR #104。）
- **自定义按键：** `launch.json` 里的 `--keymap` 现在可以识别常见的按键写法（`LeftShift`、`LeftCtrl`、`Spacebar`、`Esc`）。空格和 Esc 可以映射了；空格被映射后，用 Pause 键暂停。模拟器无法使用的行会被跳过，并在运行日志里写明，不会再导致无法启动。详见 README 的 "Your own keys"（issue #91）。
- **超宽屏：** 使用 `-Aspect 21:9` 或 `32:9` 时，再加上 `-AspectFit`，过场动画（以及标题画面和加载画面）会保持 16:9，两侧显示黑边；不加时仍和以前一样铺满屏幕（issue #97）。
- **游戏放在机械硬盘上：** README 里写了一位玩家从卡顿变成全速运行的方法：在机械硬盘上把游戏打包成 `.zar`，模拟器文件夹放在 SSD 上（issue #91）。
- **修正：**
  - AMD Radeon RX 9000/7000/6000：游玩几分钟后模拟器可能以 "TextureCache: invalid image view description" 退出（issue #102）。
  - 从连接之地（Nexus）传送到波雷塔利亚时，模拟器可能以 "TextureCache: color target requires rediscovery before final acquisition" 退出（issue #94）。
  - DLSS 超分辨率在游戏版本 01.005.000 上 3D 画面是黑的（感谢 @Al3cko 的分析）；使用 NVIDIA App 的 DLSS 覆盖设置时也会黑屏，现在会改用游戏自带的放大（issue #87）。
  - 卡顿时按键可能丢失（@Vorshim92 的贡献，PR #100）。
  - Boss 死亡时第一次可能让游戏卡住最长 51 秒：它的计算着色器现在会按游戏用到的所有形式预编译（PR #96）。
  - 更新时的预编译不再为了复制而把存储里的每个管线重新创建一遍：更新快了很多（issue #85）。
- **Linux：** 通过 NVIDIA NGX 支持 DLSS 超分辨率、DLAA 和 DLSS 帧生成（@Tutuzaooo 的贡献，PR #101）。
- **内部清理：** 删掉了约 6,000 行已经结束的实验和诊断代码；发布用的设置现在就是模拟器自己的默认值（`launch.json` 不再列出它们；`-Set KEY=` 仍然可以关闭某一项）。行为和帧率不变（16 张地图全部测试过），着色器不变。
- **感谢** @intellq、@Myoko、@iSamigor、@Vorshim92、@Tutuzaooo 和 @Al3cko 的贡献，也感谢所有报告问题和帮忙测试的玩家。

更早的变化见 v20261020b 及之前版本的发布页面。
