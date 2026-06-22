"""把 largeImage/ 下的 AI 生成大图(1254x1254 RGB)处理成引擎可用的美术资产。

产出(写入 assets/textures/):
  1. ground_tileset.png —— 地面瓦片图集(GROUND_TILES 顺序,TILE_PX 方格,COLS 列)。
  2. prop_<name>.png / player.png —— 每个物件/角色一张:抠白底→裁内容→等比缩放装入 SPRITE_BOX 透明方框(RGBA)。

地面瓦片是满帧无缝纹理,直接高质量降采样(无需 alpha)。物件是不透明白底,用「从四边洪水填充近白色」抠背景,
保留物体内部的白色细节;再按内容包围盒裁剪、等比缩放居中放入透明方框。

运行:python tools/pack_atlas.py   (需 pip install pillow)
源图是平滑插画(非真像素画),故降采样用 LANCZOS 取干净结果,而非 NEAREST。
"""
import os
from collections import deque

from PIL import Image

# —— 可调参数(具名常量,零魔法数字)——
SRC_DIR = "largeImage"
OUT_DIR = "assets/textures"
TILE_PX = 32                 # 地面瓦片边长(像素)
COLS = 4                     # 地面图集列数
SPRITE_BOX = 64             # 物件精灵装入的透明方框边长
BG_WHITE_MIN = 205         # RGB 三通道均 ≥ 此值视为近白底候选
EDGE_FEATHER_ALPHA = 0     # 抠出的背景 alpha 值(0=全透明)

# 地面瓦片:按此顺序排进图集,localId 即此下标(行主序),gid = firstGid + localId。
GROUND_TILES = [
    "lush_farm_grass",
    "dirt_path",
    "tilled_dark_farm_soil_with_furrows",
    "shallow_water",
    "stone_floor",
    "sand",
    "wooden_plank_floor",
]

# 物件/角色:抠底后各存一张独立精灵纹理。
PROPS = [
    "small_leafy_farm_tree",
    "gray_boulder_rock",
    "wooden_fence_segment",
    "watering_can",
    "wooden_crate",
    "sign_post",
    "bush_with_berries",
]
PLAYER = "player"


def _src(name):
    return os.path.join(SRC_DIR, name + ".png")


def remove_white_background(im):
    """从四边洪水填充连通的近白像素 → 透明;保留物体内部白色。返回 RGBA。"""
    im = im.convert("RGBA")
    w, h = im.size
    px = im.load()

    def is_white(x, y):
        r, g, b, _ = px[x, y]
        return r >= BG_WHITE_MIN and g >= BG_WHITE_MIN and b >= BG_WHITE_MIN

    visited = bytearray(w * h)
    q = deque()
    # 从所有边界白像素入队
    for x in range(w):
        for y in (0, h - 1):
            if not visited[y * w + x] and is_white(x, y):
                visited[y * w + x] = 1
                q.append((x, y))
    for y in range(h):
        for x in (0, w - 1):
            if not visited[y * w + x] and is_white(x, y):
                visited[y * w + x] = 1
                q.append((x, y))
    # 4 邻接洪水填充
    while q:
        x, y = q.popleft()
        r, g, b, _ = px[x, y]
        px[x, y] = (r, g, b, EDGE_FEATHER_ALPHA)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < w and 0 <= ny < h and not visited[ny * w + nx] and is_white(nx, ny):
                visited[ny * w + nx] = 1
                q.append((nx, ny))
    return im


def fit_into_box(im, box):
    """按内容(alpha>0)包围盒裁剪,等比缩放装入 box×box 透明方框并居中。"""
    bbox = im.getbbox()  # 非零(含 alpha)区域
    if bbox:
        im = im.crop(bbox)
    w, h = im.size
    scale = box / max(w, h)
    nw, nh = max(1, round(w * scale)), max(1, round(h * scale))
    im = im.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("RGBA", (box, box), (0, 0, 0, 0))
    canvas.paste(im, ((box - nw) // 2, (box - nh) // 2), im)
    return canvas


def build_ground_tileset():
    rows = (len(GROUND_TILES) + COLS - 1) // COLS
    sheet = Image.new("RGBA", (COLS * TILE_PX, rows * TILE_PX), (0, 0, 0, 0))
    for i, name in enumerate(GROUND_TILES):
        tile = Image.open(_src(name)).convert("RGBA").resize((TILE_PX, TILE_PX), Image.LANCZOS)
        c, r = i % COLS, i // COLS
        sheet.paste(tile, (c * TILE_PX, r * TILE_PX))
    out = os.path.join(OUT_DIR, "ground_tileset.png")
    sheet.save(out)
    print(f"[ground] {out}  {sheet.size[0]}x{sheet.size[1]}  "
          f"tile={TILE_PX}px cols={COLS} rows={rows} count={len(GROUND_TILES)}")
    for i, name in enumerate(GROUND_TILES):
        print(f"         localId {i} (col {i % COLS}, row {i // COLS}) = {name}")


def build_sprite(name, out_name):
    im = remove_white_background(Image.open(_src(name)))
    im = fit_into_box(im, SPRITE_BOX)
    out = os.path.join(OUT_DIR, out_name)
    im.save(out)
    amin, amax = im.getextrema()[3]
    print(f"[sprite] {out}  {im.size[0]}x{im.size[1]}  alpha[{amin}-{amax}]")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    build_ground_tileset()
    for name in PROPS:
        build_sprite(name, f"prop_{name}.png")
    build_sprite(PLAYER, "player_sprite.png")
    print("done.")


if __name__ == "__main__":
    main()
