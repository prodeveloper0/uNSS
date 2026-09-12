#pragma once

#include "sync.hpp"

#include <string>
#include <vector>

namespace nativerestore
{
struct Backup
{
    u64 titleId;
    std::string revision;
    std::string titleName;
};

// Lists only installed native titles that also have a server backup. This
// excludes forwarders without guessing from their arbitrary program IDs.
int listAvailable(const SyncOptions& options, std::vector<Backup>& backups,
                  int* skipped = nullptr);

// Non-destructive: download and fully verify all relevant current backups.
int verifyLatest(const SyncOptions& options, SyncLogFunc log);

// Preflight and restore only the explicitly selected games. If any selected
// archive fails validation, no live save is modified.
int restoreSelected(const SyncOptions& options,
                    const std::vector<Backup>& selected, SyncLogFunc log);
}
