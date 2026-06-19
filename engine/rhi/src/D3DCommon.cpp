#include "me/rhi/D3DCommon.h"

#include <cstdio>
#include <cstdlib>

namespace me::rhi::detail {

void HrFail(long hr, const char* expr, const char* file, int line) {
    std::fprintf(stderr, "[ME_HR_CHECK] FAILED hr=0x%08lX  %s\n  at %s:%d\n",
                 static_cast<unsigned long>(hr), expr, file, line);
    std::fflush(stderr);
    std::abort();
}

} // namespace me::rhi::detail
