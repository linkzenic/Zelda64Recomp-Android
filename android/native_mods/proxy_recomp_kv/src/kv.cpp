#include <filesystem>
#include <string>

#include "sqlite3.h"
#include "helpers.hpp"
#include "mod_recomp.h"

#define DB_FILE_EXT ".ProxyRecomp_KV.db"

sqlite3 *db;
int kvState = -1;

namespace fs = std::filesystem;

fs::path DB_FILE;

extern "C" {

DLLEXPORT uint32_t recomp_api_version = 1;

int KV_InitImpl() {
    if (sqlite3_open(DB_FILE.string().c_str(), &db) != SQLITE_OK) {
        printf("[ProxyRecomp_KV] Failed init, can't open database: %s\n", sqlite3_errmsg(db));
        kvState = 0;
        return kvState;
    }

    const char *sql = "CREATE TABLE IF NOT EXISTS storage005 (key TEXT, slot INTEGER, value BLOB NOT NULL, PRIMARY KEY(key, slot));";
    kvState = sqlite3_exec(db, sql, 0, 0, 0) == SQLITE_OK;
    if (!kvState) {
        printf("[ProxyRecomp_KV] Failed init, failed table creation: %s\n", sqlite3_errmsg(db));
    } else {
        printf("[ProxyRecomp_KV] Initialized\n");
    }

    return kvState;
}

DLLEXPORT void KV_Init(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, KV_InitImpl());
}

DLLEXPORT void KV_PathUpdateInternal(uint8_t* rdram, recomp_context* ctx) {
    fs::path new_path = fs::path(_arg_string<0>(rdram, ctx)).replace_extension(DB_FILE_EXT);

    if (kvState == -1) {
        DB_FILE = new_path;
        printf("[ProxyRecomp_KV] Initializing KV Store at %s\n", new_path.string().c_str());
        _return(ctx, KV_InitImpl());
        return;
    } else if (new_path != DB_FILE) {
        // Restarting DB
        printf("[ProxyRecomp_KV] Reloading KV Store at %s\n", new_path.string().c_str());
        DB_FILE = new_path;
        sqlite3_close(db);
        _return(ctx, KV_InitImpl());
        return;
    } else {
        // printf("[ProxyRecomp_KV] No state change needed. %s\n", new_path.string().c_str());
    }
    _return(ctx, kvState);
    return;
}

DLLEXPORT void KV_Teardown(uint8_t* rdram, recomp_context* ctx) {
    sqlite3_close(db);
}

DLLEXPORT void KV_Set(uint8_t* rdram, recomp_context* ctx) {
    std::string key = _arg_string<0>(rdram, ctx);
    void* data = _arg<1, void*>(rdram, ctx);
    uint32_t size = _arg<2, uint32_t>(rdram, ctx);
    uint8_t slot = _arg<3, uint8_t>(rdram, ctx);

    if (!kvState) {
        printf("[ProxyRecomp_KV] Failed SET %s (slot %d): %s\n", key.c_str(), slot, sqlite3_errmsg(db));
        _return(ctx, 0);
        return;
    }

    const char *sql = "INSERT INTO storage005 (key, slot, value) VALUES (?, ?, ?) ON CONFLICT(key, slot) DO UPDATE SET value = excluded.value;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        printf("[ProxyRecomp_KV] Failed SET %s (slot %d): %s\n", key.c_str(), slot, sqlite3_errmsg(db));
        _return(ctx, 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, slot);
    sqlite3_bind_blob(stmt, 3, data, size, SQLITE_STATIC);
    int res = sqlite3_step(stmt) == SQLITE_DONE;
    if (!res) {
        printf("[ProxyRecomp_KV] Failed SET %s (slot %d): %s\n", key.c_str(), slot, sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    _return(ctx, res);
}

DLLEXPORT void KV_Get(uint8_t* rdram, recomp_context* ctx) {
    std::string key = _arg_string<0>(rdram, ctx);
    void* dest = _arg<1, void*>(rdram, ctx);
    uint32_t expected_size = _arg<2, uint32_t>(rdram, ctx);
    uint8_t slot = _arg<3, uint8_t>(rdram, ctx);

    if (!kvState) {
        printf("[ProxyRecomp_KV] Failed GET %s (slot %d): %s\n", key.c_str(), slot, sqlite3_errmsg(db));
        _return(ctx, 0);
        return;
    }

    const char *sql = "SELECT value FROM storage005 WHERE key = ? AND slot = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        printf("[ProxyRecomp_KV] Failed GET %s (slot %d): %s\n", key.c_str(), slot, sqlite3_errmsg(db));
        _return(ctx, 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, slot);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        size_t stored_size = sqlite3_column_bytes(stmt, 0);
        if (stored_size != expected_size) {  // Fail if sizes don't match
            printf("[ProxyRecomp_KV] Failed GET %s (slot %d): Size doesn't match\n", key.c_str(), slot);
            sqlite3_finalize(stmt);
            _return(ctx, 0);
            return;
        }
        memcpy(dest, sqlite3_column_blob(stmt, 0), stored_size);
        sqlite3_finalize(stmt);
        _return(ctx, 1);
        return;
    }

    // No need to log this I don't think?
    // printf("[ProxyRecomp_KV] Failed getting %s (slot %d): Nothing stored\n", key.c_str(), slot);
    sqlite3_finalize(stmt);
    _return(ctx, 0);
}

DLLEXPORT void KV_Remove(uint8_t* rdram, recomp_context* ctx) {
    std::string key = _arg_string<0>(rdram, ctx);
    uint8_t slot = _arg<1, uint8_t>(rdram, ctx);

    if (!kvState) {
        printf("[ProxyRecomp_KV] Failed REMOVE %s (slot %d): %s\n", key.c_str(), slot, sqlite3_errmsg(db));
        _return(ctx, 0);
        return;
    }

    const char *sql = "DELETE FROM storage005 WHERE key = ? AND slot = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        printf("[ProxyRecomp_KV] Failed REMOVE %s (slot %d): %s\n", key.c_str(), slot, sqlite3_errmsg(db));
        _return(ctx, 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, slot);
    int res = sqlite3_step(stmt) == SQLITE_DONE;
    if (!res) {
        printf("[ProxyRecomp_KV] Failed REMOVE %s (slot %d): %s\n", key.c_str(), slot, sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    _return(ctx, res);
}

DLLEXPORT void KV_Has(uint8_t* rdram, recomp_context* ctx) {
    std::string key = _arg_string<0>(rdram, ctx);
    uint8_t slot = _arg<1, uint8_t>(rdram, ctx);

    if (!kvState) {
        printf("[ProxyRecomp_KV] Failed HAS %s (slot %d): %s\n", key.c_str(), slot, sqlite3_errmsg(db));
        _return(ctx, 0);
        return;
    }

    const char *sql = "SELECT 1 FROM storage005 WHERE key = ? AND slot = ? LIMIT 1;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        printf("[ProxyRecomp_KV] Failed HAS %s (slot %d): %s\n", key.c_str(), slot, sqlite3_errmsg(db));
        _return(ctx, 0);
        return;
    }
    
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, slot);

    // Return 1 if the key exists, 0 otherwise
    int exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    _return(ctx, exists);
}

DLLEXPORT void KV_DeleteSlot(uint8_t* rdram, recomp_context* ctx) {
    uint8_t slot = _arg<0, uint8_t>(rdram, ctx);

    if (!kvState) {
        printf("[ProxyRecomp_KV] 1 Failed REMOVE slot %d: %s\n", slot, sqlite3_errmsg(db));
        _return(ctx, 0);
        return;
    }

    const char *sql = "DELETE FROM storage005 WHERE slot = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        printf("[ProxyRecomp_KV] 2 Failed REMOVE slot %d: %s\n", slot, sqlite3_errmsg(db));
        _return(ctx, 0);
        return;
    }
    sqlite3_bind_int(stmt, 1, slot);
    int res = sqlite3_step(stmt) == SQLITE_DONE;
    if (!res) {
        printf("[ProxyRecomp_KV] Failed REMOVE slot %d: %s\n", slot, sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);

    printf("[ProxyRecomp_KV] REMOVE slot %d succeeded\n", slot);
    _return(ctx, res);

}

DLLEXPORT void KV_CopySlot(uint8_t* rdram, recomp_context* ctx) {
    uint8_t old_slot = _arg<0, uint8_t>(rdram, ctx);
    uint8_t new_slot = _arg<1, uint8_t>(rdram, ctx);

    if (!kvState) {
        printf("[ProxyRecomp_KV] Failed COPY slot %d -> slot %d: %s\n", old_slot, new_slot, sqlite3_errmsg(db));
        _return(ctx, 0);
        return;
    }

    const char *sql = "Insert INTO storage005 (key, slot, value) SELECT key, ?, value FROM storage005 WHERE slot = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        printf("[ProxyRecomp_KV] Failed COPY slot %d -> slot %d: %s\n", old_slot, new_slot, sqlite3_errmsg(db));
        _return(ctx, 0);
        return;
    }
    sqlite3_bind_int(stmt, 1, new_slot);
    sqlite3_bind_int(stmt, 2, old_slot);
    int res = sqlite3_step(stmt) == SQLITE_DONE;
    if (!res) {
        printf("[ProxyRecomp_KV] Failed COPY slot %d -> slot %d: %s\n", old_slot, new_slot, sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    _return(ctx, res);

}

}
