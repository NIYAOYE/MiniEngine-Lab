// stb_image 的实现仅在此唯一 TU 展开,避免重复符号。
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO // 只用内存解码接口,文件 IO 走引擎 FileSystem
#include "stb_image.h"
