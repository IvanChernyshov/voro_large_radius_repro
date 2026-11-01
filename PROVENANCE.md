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
