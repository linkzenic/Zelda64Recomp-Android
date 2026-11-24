#include "nfd.h"

#include <cstdlib>

extern "C" {

NFD_API nfdresult_t NFD_Init(void) {
    return NFD_OKAY;
}

NFD_API void NFD_Quit(void) {
}

NFD_API nfdresult_t NFD_OpenDialogN(
    nfdnchar_t** out_path,
    const nfdnfilteritem_t*,
    nfdfiltersize_t,
    const nfdnchar_t*) {
    if (out_path != nullptr) {
        *out_path = nullptr;
    }
    return NFD_CANCEL;
}

NFD_API nfdresult_t NFD_OpenDialogMultipleN(
    const nfdpathset_t** out_paths,
    const nfdnfilteritem_t*,
    nfdfiltersize_t,
    const nfdnchar_t*) {
    if (out_paths != nullptr) {
        *out_paths = nullptr;
    }
    return NFD_CANCEL;
}

NFD_API nfdresult_t NFD_SaveDialogN(
    nfdnchar_t** out_path,
    const nfdnfilteritem_t*,
    nfdfiltersize_t,
    const nfdnchar_t*,
    const nfdnchar_t*) {
    if (out_path != nullptr) {
        *out_path = nullptr;
    }
    return NFD_CANCEL;
}

NFD_API nfdresult_t NFD_PickFolderN(nfdnchar_t** out_path, const nfdnchar_t*) {
    if (out_path != nullptr) {
        *out_path = nullptr;
    }
    return NFD_CANCEL;
}

NFD_API void NFD_FreePathN(nfdnchar_t* path) {
    std::free(path);
}

NFD_API nfdresult_t NFD_PathSet_GetCount(const nfdpathset_t*, nfdpathsetsize_t* count) {
    if (count != nullptr) {
        *count = 0;
    }
    return NFD_OKAY;
}

NFD_API nfdresult_t NFD_PathSet_GetPathN(
    const nfdpathset_t*,
    nfdpathsetsize_t,
    nfdnchar_t** out_path) {
    if (out_path != nullptr) {
        *out_path = nullptr;
    }
    return NFD_ERROR;
}

NFD_API void NFD_PathSet_Free(const nfdpathset_t*) {
}

NFD_API const char* NFD_GetError(void) {
    return "Native file dialogs are unavailable in the clean Android boot target.";
}

NFD_API void NFD_ClearError(void) {
}

}
