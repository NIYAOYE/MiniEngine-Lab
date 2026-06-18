# me_platform

操作系统隔离层。namespace `me::platform`。依赖 `me_core`。

## 内容(M0,跨平台)
- **Time**:`Stopwatch`(单调秒表)、`FrameTimer`(逐帧 deltaTime)。
- **FileSystem**:`Exists` / `ReadTextFile` / `ReadBinaryFile` / `WriteTextFile`,失败返回 `optional`/`bool`,不抛异常。

## 推迟到 M1
- `Window` / `Input`(Win32):需真实 Windows 窗口验证,与 DX12 上屏一起做。

## 依赖
`me_core`(单向)。
