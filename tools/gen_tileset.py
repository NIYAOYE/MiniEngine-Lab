"""一次性生成 assets/textures/tileset.png:64x48,4 列 x 3 行,16px 瓦片。
运行:python tools/gen_tileset.py(需 pip install pillow)。"""
from PIL import Image

TILE = 16
COLS, ROWS = 4, 3
COLORS = [
    (110, 170, 90), (90, 150, 70), (160, 130, 80), (120, 90, 60),   # 草/土
    (70, 110, 200), (200, 200, 210), (210, 180, 70), (180, 70, 70), # 水/石/花
    (140, 110, 70), (90, 160, 90), (200, 200, 200), (60, 60, 70),
]
img = Image.new("RGBA", (COLS * TILE, ROWS * TILE), (0, 0, 0, 0))
px = img.load()
for r in range(ROWS):
    for c in range(COLS):
        color = COLORS[r * COLS + c] + (255,)
        for y in range(TILE):
            for x in range(TILE):
                px[c * TILE + x, r * TILE + y] = color
img.save("assets/textures/tileset.png")
print("wrote assets/textures/tileset.png")
