"""What an evidence bundle records about the source it was built from.

A bundle is only as good as the ability to rebuild its payload. When the working tree was clean, the commit is enough. When it was not,
the uncommitted changes are saved in the bundle (source.diff) so the payload can still be rebuilt: check out the commit, apply the diff,
build. Of the first 54 bundles, 26 were recorded with a dirty tree and, for 25 of them, the recorded commit alone could not rebuild the
payload (it did not exist yet); this module exists so that does not happen again. Files that are not yet tracked cannot be rebuilt from a
diff: they are named in the manifest and the run warns, so they get committed first.
"""
import hashlib


def source_fields(commit, dirty, diff, untracked):
    """(the 'git' block of the manifest, the patch text to save or None)."""
    block = {"commit": commit, "dirty": bool(dirty)}
    patch = None
    if dirty and diff:
        patch = diff if diff.endswith("\n") else diff + "\n"
        block["source_diff"] = "source.diff"
        block["source_diff_sha256"] = hashlib.sha256(patch.encode()).hexdigest()
    if untracked:
        block["untracked"] = sorted(untracked)
    return block, patch


def warnings(block):
    """Lines to print so a dirty run is not silently unreproducible."""
    out = []
    if block.get("dirty"):
        out.append("source tree is dirty: the uncommitted changes are saved in source.diff; commit them before the next run so that a "
                   "bundle can be rebuilt from its commit alone")
    if block.get("untracked"):
        out.append("untracked files under tools/, include/ or src/ are named in the manifest but NOT saved (a diff cannot carry them): "
                   "commit them first. Untracked: " + ", ".join(block["untracked"]))
    return out
