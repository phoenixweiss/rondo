# Continuous checks

The Native checks workflow always validates source files, local documentation
links, CI scope selection and release/package/installer tooling on Linux.
README, changelog, contributing text and ordinary Markdown documentation can
finish at this stage without installing SDL or compiling emulator cores.

Runtime changes run the existing Linux and macOS native matrix. Code, assets,
vendored files, dependency pins, build/release settings, workflow changes and
unrecognized paths require the full matrix. Deleted paths and both sides of a
rename are included. Missing diff history, first pushes and manual workflow
runs also select the full matrix. A failed source job cannot silently turn into
a successful documentation-only run; cancellation still cancels downstream work.

Version-tag H700 builds and Bumpster pre-release checks retain their full gates,
including independent builds, source/license audits and artifact verification.
The scope selector does not publish, push, install or alter branch protection.

Run the lightweight commands locally with Python:

```sh
python tools/check_source.py
python tests/ci_scope_tests.py
python tests/release_tests.py
python tests/runtime_build_tests.py
python tests/installer_tests.py
```

Full native checks still use `tools/build_core.py` followed by `tools/check.py`
with the project's Python dependencies installed. The build/check directories
are temporary; retain the package and compact results after verification.

## Reusing checks for the same commit

When a push to `main` points to the exact `dev` commit, the workflow may reuse
that commit's trusted `dev` push run. Both Linux and macOS native jobs must
already exist and be running or successful. A documentation-only run is never
eligible. The main workflow waits for that run and independently requires both
native jobs to finish successfully. Unknown history, API errors, absent jobs,
failed runs, pull requests and manual runs use the full matrix instead.
Source validation runs on both branches.

The native jobs cache only the six compiled cores. The cache key includes pinned
sources/build scripts, OS/architecture, compiler/archive-tool identities, SDK and
build environment. File hashes are verified before use; changed inputs rebuild
in a fresh temporary source tree. Source and object trees are removed afterwards.
Application/test compilation and the complete native test suite still run.
Use `python tools/build_core.py --fresh --directory build/fresh-cores` to bypass
compiled caching; the directory must be empty. Two compiled entries are retained
locally. GitHub stores this same small cache using the computed toolchain key.

Run `python tests/build_cache_tests.py` and `python tests/ci_reuse_tests.py` to
exercise cache invalidation/integrity and reuse/fallback decisions without network
access or publication.
