# engine/assets(me_assets)

资源解码层。M1 仅含图片解码(stb_image)→ 紧凑 RGBA8 `ImageData`。
跨平台,可在 WSL 单测。依赖:Core, Platform(单向)。
后续里程碑扩展 AssetManager/句柄/图集/瓦片集/内容 JSON。
