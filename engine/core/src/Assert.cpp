#include "me/core/Assert.h"

#include <cstdio>
#include <cstdlib>

namespace me::detail {

void AssertFail(const char* expr, const char* file, int line, const char* msg) {
    if (msg != nullptr) {
        std::fprintf(stderr, "[ASSERT] %s:%d: (%s) -- %s\n", file, line, expr, msg);
    } else {
        std::fprintf(stderr, "[ASSERT] %s:%d: (%s)\n", file, line, expr);
    }
    std::abort();
}

} // namespace me::detail
