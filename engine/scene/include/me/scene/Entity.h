#pragma once

#include "me/core/Handle.h"

namespace me::scene {

/// 仅作类型标签,使 Entity 与其它 Handle<T> 不可互相赋值(编译期类型安全)。
struct EntityTag;

/// 场景实体句柄:index + generation,销毁后旧句柄因 generation 不匹配而失效。
using Entity = me::Handle<EntityTag>;

} // namespace me::scene
