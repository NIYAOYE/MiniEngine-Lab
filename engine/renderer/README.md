# engine/renderer(me_renderer,仅 Windows)

2D 渲染。M1 仅含 SpriteRenderer(单精灵:根签名+PSO+单位四边形,经 MVP 绘制)。
依赖:Core, RHI, Assets(单向)。M2 扩展为 SpriteBatch + 正交相机。
