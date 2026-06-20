#pragma once

#include "me/core/Rect.h"
#include "me/core/Assert.h"
#include "me/assets/TileMapData.h"
#include "me/assets/TileLayout.h"

namespace me::rhi { class GpuTexture; }

namespace me::render {

/**
 * @brief 运行时 tileset:把 TilesetDesc 绑定到一个已上传的 GpuTexture。
 *
 * 非拥有纹理指针,生命周期归调用方。SrcRectForGid 将全局 gid 转局部 id 后委托 TileLayout。
 */
class Tileset {
public:
    Tileset(const me::rhi::GpuTexture* texture, me::assets::TilesetDesc desc)
        : m_texture(texture), m_desc(std::move(desc)) {
        ME_ASSERT_MSG(m_texture != nullptr, "Tileset: 纹理不可为空");
    }

    const me::rhi::GpuTexture* Texture() const { return m_texture; }

    /// @brief 全局 gid → 归一化采样 UV(调用方须保证 gid != kEmptyTileGid)。
    me::Rect SrcRectForGid(int gid) const {
        ME_ASSERT_MSG(gid >= m_desc.firstGid, "gid 小于 firstGid");
        return me::assets::SrcRectForLocalId(m_desc, gid - m_desc.firstGid);
    }

private:
    const me::rhi::GpuTexture* m_texture = nullptr;
    me::assets::TilesetDesc m_desc;
};

} // namespace me::render
