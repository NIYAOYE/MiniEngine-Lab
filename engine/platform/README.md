# me_platform

操作系统隔离层。namespace `me::platform`。依赖 `me_core`。

## 内容(M0,跨平台)
- **Time**:`Stopwatch`(单调秒表)、`FrameTimer`(逐帧 deltaTime)。
- **FileSystem**:`Exists` / `ReadTextFile` / `ReadBinaryFile` / `WriteTextFile`,失败返回 `optional`/`bool`,不抛异常。

## M1(仅 Windows)
- **Window**(Win32):创建/消息泵/`ShouldClose`/`HWND`,pImpl 封死 `windows.h`。
- **Input**:跨平台键盘状态机(`InputState`,持续态+边沿);Win32 负责 VK→`KeyCode` 馈入。

## 线程归属
`InputState` 非线程安全,约定只在主线程(消息泵所在线程)读写。

## 依赖
`me_core`(单向)。
