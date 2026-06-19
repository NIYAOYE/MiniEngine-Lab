#include "me/rhi/Shader.h"

#include <d3dcompiler.h>
#include "me/core/Log.h"

namespace me::rhi {

ComPtr<ID3DBlob> CompileHlsl(const std::wstring& path, const char* entry,
                             const char* target) {
    UINT flags = 0;
#if !defined(NDEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> code;
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3DCompileFromFile(
        path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry, target, flags, 0, &code, &errors);
    if (FAILED(hr)) {
        if (errors) {
            ME_LOG_ERROR(std::string("着色器编译失败: ") +
                         static_cast<const char*>(errors->GetBufferPointer()));
        } else {
            ME_LOG_ERROR("着色器编译失败(文件无法打开?)");
        }
        return nullptr;
    }
    return code;
}

} // namespace me::rhi
