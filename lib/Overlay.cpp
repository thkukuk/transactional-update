/*
  Handling of /etc overlayfs layers

  Copyright (c) 2016 - 2020 SUSE LLC

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Overlay.h"

#include "Configuration.h"
#include "Log.h"
#include "Mount.h"
#include "Snapshot.h"
#include "Util.h"
#include <filesystem>
#include <regex>
#include <sstream>

using std::exception;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
namespace fs = std::filesystem;

/*
 * Create a new overlay instance for the given snapshot number.
 * For existing overlays the lowerdirs are read automatically from the given snapshot overlay;
 * for this to work the snapshot still has to exist.
 * For new overlays `create` has to be called afterwards with a base.
 */
Overlay::Overlay(string snapshot):
        upperdir(fs::path{config.get("OVERLAY_DIR")} / snapshot / "etc"),
        workdir(fs::path{config.get("OVERLAY_DIR")} / snapshot / "work-etc")
{
    fs::create_directories(fs::path{workdir});

    // Read lowerdirs if this is an existing snapshot
    Mount mntEtc{"/etc"};
    mntEtc.setTabSource(upperdir / "fstab");
    try {
        const string fstabLowerdirs = mntEtc.getOption("lowerdir");
        string lowerdir;
        stringstream ss(fstabLowerdirs);
        while (getline(ss, lowerdir, ':')) {
            lowerdir = regex_replace(lowerdir, std::regex("^" + config.get("DRACUT_SYSROOT")), "");
            lowerdirs.push_back(lowerdir);
        }
    } catch (exception e) {}
}

string Overlay::getIdOfOverlayDir(const string dir) {
    std::smatch match;
    std::regex exp("^" + config.get("OVERLAY_DIR") + "/(.+)/etc$");
    if (regex_search(dir.begin(), dir.end(), match, exp)) {
        return match[1];
    }
    return "";
}

void Overlay::sync(string snapshot) {
    Mount currentEtc{"/etc"};

    auto oldestSnapId = getOldestSnapshot();
    unique_ptr<Snapshot> oldestSnap = SnapshotFactory::get();
    oldestSnap->open(oldestSnapId);
    unique_ptr<Mount> oldestEtc{new Mount("/etc")};
    oldestEtc->setTabSource(oldestSnap->getRoot() / "etc" / "fstab");

    if (oldestEtc->getOption("upperdir") == currentEtc.getOption("upperdir"))
        return;

    // Replace generic /etc lowerdir with snapshot version
    Overlay oldestOvl{oldestSnapId};
    auto last = oldestOvl.lowerdirs.rbegin();
    *last = oldestSnap->getRoot() / last->relative_path();

    // Mount read-only, so mount everything as lowerdir
    oldestOvl.lowerdirs.insert(oldestOvl.lowerdirs.begin(), oldestOvl.upperdir);
    oldestOvl.updateMountDirs(oldestEtc);
    oldestEtc->removeOption("upperdir");

    oldestEtc->mount(oldestOvl.upperdir.parent_path() / "sync");
    Util::exec("rsync --quiet --archive --inplace --xattrs --exclude='/fstab' --filter='-x security.selinux' --acls --delete " + string(oldestOvl.upperdir.parent_path() / "sync" / "etc") + "/ " + snapshot + "/etc");
}

void Overlay::updateMountDirs(unique_ptr<Mount>& mount, fs::path prefix) {
    string lower;
    for (auto lowerdir: lowerdirs) {
        if (! lower.empty())
            lower.append(":");
        lower.append(prefix / lowerdir.relative_path());
    }
    mount->setOption("lowerdir", lower);
    mount->setOption("upperdir", prefix / upperdir.relative_path());
    mount->setOption("workdir", prefix / workdir.relative_path());
}

string Overlay::getOldestSnapshot() {
    for (auto it = lowerdirs.rbegin(); it != lowerdirs.rend(); it++) {
        string id = getIdOfOverlayDir(*it);
        if (! id.empty())
            return id;
    }
    return getIdOfOverlayDir(upperdir);
}

void Overlay::create(string base = "") {
    if (base.empty())
        return;

    tulog.debug("Using snapshot " + base + " as base for overlay.");
    Overlay parent = Overlay{base};
    // Remove overlay directory if it already exists (e.g. after the snapshot was deleted)
    fs::remove_all(upperdir);
    fs::create_directories(upperdir);

    for (auto it = parent.lowerdirs.begin(); it != parent.lowerdirs.end(); it++) {
        string lowerdir = *it;
        // Compatibilty handling for old overlay location without separate directories for each
        // snapshot - keep it until all snapshots that could reference it have gone, which is the
        // case as soon as any (numbered) overlay in the list references a removed snapshot.
        if ((lowerdir == "/sysroot/var/lib/overlay/etc") && (lowerdirs == parent.lowerdirs)) {
            lowerdirs.push_back(lowerdir);
        } else {
            string snapId = getIdOfOverlayDir(lowerdir);
            // Add non-snapshot overlays (usually just /etc - but who knows, this would allow
            // interesting setups...)
            if (snapId.empty())
                lowerdirs.push_back(lowerdir);
            else {
                unique_ptr<Snapshot> oldSnap = SnapshotFactory::get();
                oldSnap->open(snapId);
                // Check whether the snapshot of the overlay still exists
                if (fs::is_directory(fs::path{oldSnap->getRoot()})) {
                    tulog.debug("Re-adding overlay stack up to ", oldSnap->getRoot(), " to /etc lowerdirs - snapshot is still active.");
                    // In case some snapshots in the middle of the overlay stack have been
                    // deleted the overlays still have to be added again up to the oldest still
                    // available snapshot, otherwise the overlay contents would be inconsistent
                    lowerdirs = vector<fs::path>(parent.lowerdirs.begin(), it+1);
                } else {
                    tulog.debug("Snapshot for " + lowerdir + " has been deleted - may be discarded from /etc lowerdirs.");
                }
            }
        }
    }
    // For the old overlay compatibility check above to work the old upperdir must only be
    // inserted at the end. Use ranges for the check as soon as C++20 is available instead.
    lowerdirs.insert(lowerdirs.begin(), parent.upperdir);
}
