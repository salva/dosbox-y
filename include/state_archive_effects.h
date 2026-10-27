/* GPL-2.0-or-later. Copied facts from one original-engine archive save. */
#ifndef DOSBOX_STATE_ARCHIVE_EFFECTS_H
#define DOSBOX_STATE_ARCHIVE_EFFECTS_H
namespace StateArchive {
struct Effects {
    // Publication covers the main archive only. Flagged-file sidecars are
    // separate, and no directory-sync/transaction durability is promised.
    bool published = false;
    bool cleanup_attempted = false;
    bool cleanup_failed = false;
};
}
#endif
