# Source provenance

This project contains a full and unmodified Voro++ source snapshot under `vendor/voro/`.

- Project: [Chris H. Rycroft's Voro++](https://github.com/chr1shr/voro).
- Branch: `master` (the original project's default branch).
- Commit: `b0dac575a47af0f90b5b100e6dc199a493c7cb83`.
- Original Git **root tree SHA**: `963387efecc4611d2103370ca8a6b03217e3efab`.
- Original commit date: 2026-03-04.
- Source includes both `src/` (3D) and `2d/`, examples, build files and original `LICENSE`.

## Integrity check

During import, every one of the 219 source file entries was compared with the original Git tree by path, blob SHA and file mode. All entries matched; the resulting Git subtree at `vendor/voro` is **exactly** `963387efecc4611d2103370ca8a6b03217e3efab`.

You can check the subtree identity in a clone:

```bash
git rev-parse HEAD:vendor/voro
# 963387efecc4611d2103370ca8a6b03217e3efab
```

Source files are not edited in this repository. The candidate patch is applied to a temporary copy, then discarded at the end of a run. The original source license is preserved in `vendor/voro/LICENSE`.

The Voro++ snapshot should not be silently updated: a future newer baseline would require an explicit provenance change and fresh test results.

## Additional development-branch snapshot (investigation)

The separately preserved, **unmodified** development-branch snapshot lives in
`vendor/voro-dev/` and is independent of the original `vendor/voro/` master
snapshot described above.

- Upstream branch: `chr1shr/voro` [`dev`](https://github.com/chr1shr/voro/tree/dev).
- Pinned commit: `61099f682c6672a275b6be7de5923f2a1a7bf05e` (2026-10-17).
- Original Git root tree SHA: `67d734f48decce33d4844e150c27f5e6932a5979`.
- Files: 238 source-tree blobs, including original `LICENSE` and `README.md`.
- The complete subtree was reconstructed using upstream Git blob IDs and file
  modes, then checked for **exact Git tree SHA equality** with upstream `dev`.
- This branch has reorganized 2D/3D sources and is not interchangeable with
  the pinned `master` branch or its experimental patches.

Verify it in a clone with:

```bash
git rev-parse HEAD:vendor/voro-dev
# 67d734f48decce33d4844e150c27f5e6932a5979
```

The minimal dev-specific C++ reproducers are in `repro/dev/` and use only
the upstream snapshot in `vendor/voro-dev/`. No source file in either
upstream subtree is modified by any test harness.
