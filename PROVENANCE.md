# Source provenance

The repository contains two unchanged snapshots of
[Chris H. Rycroft's Voro++](https://github.com/chr1shr/voro).
Each snapshot is pinned to an upstream commit and checked against
its Git tree. The experimental patches apply to temporary copies.

| Upstream branch | Local source | Commit | Git root tree SHA | Files |
|---|---|---|---|---:|
| `master` | `vendor/voro/` | `b0dac575a47af0f90b5b100e6dc199a493c7cb83` (2026-03-04) | `963387efecc4611d2103370ca8a6b03217e3efab` | 219 |
| `dev` | `vendor/voro-dev/` | `61099f682c6672a275b6be7de5923f2a1a7bf05e` (2026-10-17) | `67d734f48decce33d4844e150c27f5e6932a5979` | 238 |

For each import, the upstream file paths, Git blob SHA-1s and file
modes were compared; the resulting local subtree has the same Git
tree SHA as upstream. Both trees include the upstream source
license and build files.

Verify the trees in a local clone:

```bash
git rev-parse HEAD:vendor/voro
# 963387efecc4611d2103370ca8a6b03217e3efab

git rev-parse HEAD:vendor/voro-dev
# 67d734f48decce33d4844e150c27f5e6932a5979
```

The `master` source has separate 2D and 3D implementations. The
`dev` branch reorganizes their active implementation under
`src/`. Their numerical patches are therefore kept separately
under `patches/` and `patches/dev/`, respectively.

Updating either source snapshot would require a new provenance record
and a separate validation of the corresponding patch.
