#pragma once

#include <string>
#include <d3d12.h>
#include <d3dcommon.h> // ID3DBlob

#include "me/rhi/D3DCommon.h"

namespace me::rhi {

/**
 * @brief 运行时用 FXC(D3DCompileFromFile)编译一个 HLSL 入口为字节码。
 *
 * @param path   .hlsl 绝对路径(由 ME_ASSET_DIR 拼出)。
 * @param entry  入口函数名(如 "VSMain")。
 * @param target 着色器模型(如 "vs_5_1" / "ps_5_1")。
 * @return 字节码 Blob;失败返回 nullptr 并把编译错误写入 Log。
 */
ComPtr<ID3DBlob> CompileHlsl(const std::wstring& path, const char* entry,
                             const char* target);

} // namespace me::rhi
