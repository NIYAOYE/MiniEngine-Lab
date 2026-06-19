# engine/rhi(me_rhi / me_rhi_cpu)

DX12 薄封装。裸 `ID3D12*` 封死在本模块内,不外泄。

- **me_rhi_cpu**(INTERFACE,跨平台):同步/帧环/描述符/几何/精灵矩阵等纯逻辑,可在 WSL 单测。
- **me_rhi**(STATIC,仅 Windows):Device / Fence / CommandContext / SwapChain / GpuBuffer /
  GpuTexture / Shader / 像素回读。依赖系统 DX12(d3d12/dxgi/d3dcompiler)。

依赖:Core, Platform, me_rhi_cpu(单向)。GPU 正确性由 `me_gpu_tests`(WARP 回读)把关。
