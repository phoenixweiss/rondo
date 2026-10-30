# Verification stages

Rondo uses separate checks for source, native behavior, packaged userspace and physical hardware. Passing one stage does not establish the others.

1. **Source inputs:** validate JSON, documentation links, locale placeholders, vendor hashes and source boundaries with tools/check_source.py. Keep commercial ROMs, BIOS, user state, firmware images and local maintainer materials out of the repository.
2. **Native behavior:** build the pinned FCEUmm core and run tools/check.py. This checks the model, volume feedback, separate runner, SRAM, save slots, resume and synthetic device policies without opening a window or playing sound. GitHub Actions runs the native checks on Ubuntu and macOS.
3. **H700 package:** build twice with `tools/build_h700_runtime.py --fresh` in independent directories. Compare source IDs, all shipped binary hashes and ZIP hashes. Compilation uses the pinned ARM64 SDK without networking; dependency archives are checked by SHA-256.
4. **Distribution audit:** run tools/audit_h700_package.py against the exact ZIP. Verify complete manifests, corresponding source, target ELF files, dependency archives and required license notices.
5. **BaseOS userspace:** optionally run tools/check_h700_runtime.py with an owner-supplied BaseOS rootfs mounted read-only. Loader/dependency checks and synthetic devices establish userspace and policy behavior, not physical panel, audio, input or suspend operation.
6. **Device and release acceptance:** test the exact candidate on RG34XXSP after separately authorizing installation and backing up games, saves and preferences. Before release, verify the selected version/ref, hosted CI, artifact checksums and the actual downloaded public package.

See [development](development.md), [runtime builds](../runtime/README.md), [compatibility](compatibility.md), [installation and rollback](installation.md) and [release procedure](release.md) for commands and current limits. A source commit or successful build does not create a release or install anything on an SD card.
