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
