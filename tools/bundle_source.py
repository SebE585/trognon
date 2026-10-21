"""What an evidence bundle records about the source it was built from.

A bundle is only as good as the ability to rebuild its payload. When the working tree was clean, the commit is enough. When it was not,
the uncommitted changes are saved in the bundle (source.diff) so the payload can still be rebuilt: check out the commit, apply the diff,
build. Of the first 54 bundles, 26 were recorded with a dirty tree and, for 25 of them, the recorded commit alone could not rebuild the
payload (it did not exist yet); this module exists so that does not happen again. Files that are not yet tracked cannot be rebuilt from a
diff: they are named in the manifest and the run warns, so they get committed first.
"""
import hashlib
import shutil
import subprocess

# Is all the source of the build in the bundle? Decided when the run is recorded, from the git state alone:
#   complete          clean tree: the recorded commit holds all the source
#   with-source-diff  dirty tree: the recorded commit plus the saved source.diff hold all of it
#   incomplete        files that git does not track were part of the build and are not saved
# This is NOT a verdict of reproducibility. A clean tree does not prove that the binary that was sent was built from that commit
# (an older build, other options), and the nonce proves the binary ran, not where it came from. Reproducibility is a rebuild whose
# SHA-256 matches the manifest: tools/rebuild_bundle.py, recorded on the Verifications page.
SOURCE_COMPLETE = ("complete", "with-source-diff", "incomplete")


def build_environment(payloads_dir, run=subprocess.run, which=shutil.which):
    """The compiler and flags the payloads are built with, as installed when the run is recorded. The payload is not rebuilt at
    run time, so this is the toolchain present, not proof of the one that built the binary; the SHA-256 is the proof."""
    gcc = which("arm-none-eabi-gcc")
    compiler = None
    if gcc:
        out = run([gcc, "--version"], capture_output=True, text=True).stdout.splitlines()
        compiler = out[0].strip() if out else "arm-none-eabi-gcc (version unknown)"
    flags = None
    try:
        r = run(["make", "-s", "-C", payloads_dir, "print-flags"], capture_output=True, text=True, timeout=20)
        flags = r.stdout.strip() or None if r.returncode == 0 else None
    except Exception:
        pass
    return {"compiler": compiler, "cflags": flags}


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
    if untracked or (dirty and not patch):
        block["source_complete"] = "incomplete"
    else:
        block["source_complete"] = "with-source-diff" if patch else "complete"
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
