#pragma once

namespace sdf3d {

/// Tracks which editor edits require shader rebuilds versus material uniform uploads.
struct EditorDirtyState {
    bool scene = false;
    bool material = false;
};

} // namespace sdf3d
