# Handoff: OCS RankDesc Full List Migration UT Blocker

Date: 2026-05-19
Repo: `/data/ccl_workspace/code_dir/ocs_hcomm`
Branch: `develop`

## Objective

Continue executing the plan:

`docs/superpowers/plans/2026-05-18-ocs-rankdesc-full-list.md`

Original user requirements:

- Execute every task in the plan.
- Commit after each modification for rollback.
- After each completed plan item, change `[ ]` to `[x]` in the plan and commit that progress.
- Do not commit `.gitignore`, `CMakeLists.txt`, or `CLAUDE.md`.
- Do not use `git add .`.
- Complete the active goal only after final audit and evidence.

Recommended next-session skills:

- `superpowers:systematic-debugging` for the UT failure.
- `superpowers:verification-before-completion` before marking any remaining checkbox complete.
- `handoff` only if work must be paused again.

## Current Status

Tasks 1-8 in the plan are complete and checked.

Task 9 status:

- Step 1 incremental build: complete and checked.
- Step 2 full UT: blocked, still unchecked.
- Step 3 final forbidden-symbol deletion check: still unchecked.

No source edits are currently pending. Current known untracked local files are expected and should be ignored:

- `.gitignore`
- `.scratch/`
- `build_incremental/`

## Recent Commits

Relevant recent commits, newest first:

- `288ab6ef docs(api): update rank desc list contract`
- `42be4310 docs(plan): mark incremental build complete`
- `b8b858e3 fix(api): expose rank desc list through RankGraph wrapper`
- `f261a4af docs(plan): mark rank graph tests complete`
- `7d2148ff test: update ut_ocs_mesh_plane for rankDescVec_ access pattern`
- `d8f5775a docs(plan): mark cached descriptor cleanup complete`
- `ff314e96 refactor: remove cachedRankDesc_ and NewRankInfo dead ocsPlane fields`
- `9482008b docs(plan): mark accessor cleanup task complete`
- `63378ff0 refactor: remove OcsPlaneId/Num accessor chain (superseded by rankDescVec_)`
- `4ab278e8 docs(plan): mark subcomm task complete`
- `e01f7b19 feat(subcomm): replace Reparse with BuildRankDescVec in CreateSubComm`
- `4cb7bca3 docs(plan): mark rank desc API task complete`
- `ddc60364 feat(api): simplify HcclGetRankDescList to return full rankDescVec_`
- `d27021c5 docs(plan): mark builder task complete`
- `9aafc699 refactor(builder): switch from Reparse to BuildRankDescVec in BuildRankGraph`
- `7627f2ed docs(plan): mark RankGraph implementation task complete`
- `a5ce0940 fix(rank_graph): harden rankDescVec population`
- `fc846f78 feat(rank_graph): add BuildRankDescVec, converge Reparse to rankDescVec_`
- `08628faf docs(plan): mark RankGraph header task complete`
- `73e67dcf refactor(rank_graph): replace OcsMeshAttr/map with rankDescVec_`

The latest source follow-up was code-review feedback for stale public API comments. It was fixed in `288ab6ef`.

## Implementation Summary

The migration replaced OCS mesh descriptor caches/accessors with `rankDescVec_`.

Key API forwarding path added for independent-op rank graph:

- `src/framework/communicator/impl/independent_op/rank_graph/rank_graph_base.h`
  - Added virtual `GetRankDescList(RankDesc **descList, uint32_t *descNum)`.
- `src/framework/communicator/impl/independent_op/rank_graph/rank_graph_v2.h`
  - Added override.
- `src/framework/communicator/impl/independent_op/rank_graph/rank_graph_v2.cc`
  - Forwards to `pImpl`.
- `src/legacy/interface/rank_graph_interface.h`
  - Added `IRankGraph::GetRankDescList`.
- `src/legacy/interface/rank_graph_interface.cc`
  - Returns `RankGraph::GetRankDescVec().data()` and size after null checks.
- `src/framework/communicator/impl/independent_op/hccl_independent_rank_graph.cc`
  - `HcclGetRankDescList` now calls the wrapper instead of assuming one local descriptor.
- `include/hccl/hccl_rank_graph.h`
  - Public comments now document that all rank descriptors are returned and the list lifetime is owned by the rank graph object.

## Verification Evidence So Far

Incremental build passed earlier:

```bash
cd /data/ccl_workspace/toolsh_dir
bash hcomm_build.sh
```

Evidence recorded from that run:

- Exit code 0.
- Output included `=== 增量编译完成 ===` and `[INFO] All done!`.
- Plan Task 9 Step 1 was checked and committed as `42be4310`.

Targeted related tests passed in existing logs:

- `output/ut_logs/hccl_v2_utest_ut_ocs_mesh_plane.log`
  - `OcsMeshPlaneTest.ReparseGroupedPlaneForOcsMesh_MultiElecGroup_MainComm`
  - `1 test ... PASSED`
- `output/ut_logs/hccl_v2_utest_rank_graph.log`
  - `38 tests from 3 test suites`
  - `PASSED`
- `output/ut_logs/hccl_utest_test_next_resource_rank_graph.log`
  - `18 tests from 1 test suite`
  - `PASSED`

Do not treat these targeted passes as satisfying Task 9 Step 2 unless the user explicitly accepts scoped verification.

## UT Blocker

The plan requires:

```bash
cd /data/ccl_workspace/toolsh_dir
bash hcomm_build.sh --ut
```

That wrapper command rebuilt successfully and ran CTest, but failed:

```text
97% tests passed, 1 tests failed out of 32

The following tests FAILED:
    16 - hccl_utest_framework_communicator (SEGFAULT)
```

Fresh log location:

`/data/ccl_workspace/code_dir/ocs_hcomm/output/ut_logs/ctest_summary.log`

Important detail from that same log: GoogleTest output inside `hccl_utest_framework_communicator` showed all internal tests passing before CTest reported a segfault:

```text
[==========] 131 tests from 30 test suites ran.
[  PASSED  ] 131 tests.
```

Direct isolated reruns of the communicator target did not reproduce the segfault:

```bash
cd /data/ccl_workspace/code_dir/ocs_hcomm/build
ctest -V -R hccl_utest_framework_communicator --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 1
```

Direct gdb run also exited normally:

```bash
cd /data/ccl_workspace/code_dir/ocs_hcomm/build/test/ut/framework/communicator
gdb -q --batch -ex 'set pagination off' -ex run -ex 'thread apply all bt' --args ./hccl_utest_framework_communicator
```

Result:

```text
[  PASSED  ] 131 tests.
[Inferior ... exited normally]
```

A serial full CTest diagnostic was then run:

```bash
cd /data/ccl_workspace/code_dir/ocs_hcomm/build
ctest -j1 --build-nocmake --timeout 200 --output-on-failure
```

Result:

```text
94% tests passed, 2 tests failed out of 32

The following tests FAILED:
    3 - hccl_utest_test_dfx_resource (SEGFAULT)
    32 - hccl_v2_utest (Failed)
```

Log location:

`/data/ccl_workspace/code_dir/ocs_hcomm/build/Testing/Temporary/LastTest.log`

In the serial run, `hccl_utest_framework_communicator` passed, so the full UT failure mode is not stable. The serial `hccl_v2_utest` failure included 42 internal failed tests and logs such as:

```text
LoadBinaryFromFile ... /usr/local/Ascend/cann//opp/built-in/op_impl/aicpu/config/ccl_kernel.json is not a valid real path
```

Current interpretation: the remaining full-UT blocker looks more like UT environment, shared global state, test ordering, or resource isolation instability than a deterministic failure in the rankDescVec_ migration. This is not proven enough to mark Task 9 Step 2 complete.

## Next Steps

1. Do not mark Task 9 Step 2 complete without accepted evidence for the plan requirement.

2. If continuing strict debugging, use systematic debugging:

```bash
cd /data/ccl_workspace/toolsh_dir
bash hcomm_build.sh --ut-run
```

Then narrow the failing order/resource problem. Useful diagnostics already tried:

```bash
cd /data/ccl_workspace/code_dir/ocs_hcomm/build
ctest -V -R hccl_utest_framework_communicator --output-on-failure
ctest -j1 --build-nocmake --timeout 200 --output-on-failure
```

3. If the user accepts scoped verification due to likely environmental instability, document that explicitly before checking Step 2. A defensible scoped package would include:

- Existing targeted rank graph and OCS mesh logs above.
- Direct communicator pass.
- Incremental build pass.
- Clear note that full `hcomm_build.sh --ut` is blocked by non-deterministic full-suite failures.

4. Run final forbidden-symbol deletion check after the UT decision:

```bash
rg -n "OcsMeshAttr|GetOcsPlaneId|GetOcsPlaneNum|SetOcsMeshAttr|ocsMeshAttrMap_|cachedRankDesc_|GetCachedRankDesc" src/ --glob '*.h' --glob '*.cc'
```

Expected: no matches, exit code 1.

5. If the deletion check passes, mark Task 9 Step 3 as checked in the plan and commit only the plan file:

```bash
git add -f docs/superpowers/plans/2026-05-18-ocs-rankdesc-full-list.md
git commit -m "docs(plan): mark deletion check complete"
```

6. Final audit before completing the active goal:

```bash
git status --short
git diff --cached --name-only
git log --oneline -20
sed -n '430,520p' docs/superpowers/plans/2026-05-18-ocs-rankdesc-full-list.md
```

Audit requirements:

- Every plan checkbox is `[x]`, or any unchecked blocker is explicitly accepted by the user.
- Every source/doc modification has a rollback commit.
- Every plan progress check has its own commit.
- No staged files remain.
- `.gitignore`, `CMakeLists.txt`, and `CLAUDE.md` are not staged or committed.
- Final deletion scan has no forbidden symbols.
- UT evidence is accurately described.

Only after that should `update_goal status=complete` be called.

## Hard Constraints

- Never use `git add .`.
- Stage explicit paths only.
- Use `git add -f` for ignored docs files.
- Do not commit `.gitignore`, `CMakeLists.txt`, or `CLAUDE.md`.
- Do not revert user/local files.
- If editing code to fix UT issues, commit the code fix separately before marking any plan checkbox.
