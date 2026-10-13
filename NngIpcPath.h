#ifndef LLT_NNGIPC_PATH_H
#define LLT_NNGIPC_PATH_H

#include <string>

namespace llt {
namespace nngipc {

// Normalizes the configured IPC directory at every handler use site.  CMake
// normally supplies a trailing '/', but this also protects external defines.
static inline bool normalizeIpcDirectoryPath(const char *configuredPath, std::string *path)
{
    if (!configuredPath || !path || configuredPath[0] == '\0') {
        return false;
    }

    *path = configuredPath;
    if (path->back() != '/') {
        path->push_back('/');
    }
    return true;
}

} // namespace nngipc
} // namespace llt

#endif /* LLT_NNGIPC_PATH_H */
