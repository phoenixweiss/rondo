# Maintainer releases

Rondo is early development software. The first preview version is **0.0.1**. VERSION was initialized at **0.0.0** as an unreleased baseline; Bumpster advances it for a release. The baseline is not a published release. No release is produced by adding this configuration.

## Contract

- Frontend: root VERSION, vX.Y.Z annotated tags, dev → main → dev through Bumpster, no package.json synchronization.
- A Bumpster patch from the initial baseline produces 0.0.1. Test-kit numbers are not product versions.
- Website: no configured package or deployment. Its independent VERSION/package.json and website/vX.Y.Z profile belong to a later task.

Use Bumpster 1.3.0 or a compatible version. Read-only inspection:

```sh
bumpster --version
bumpster --status
```

## Before a bump

Inspect the exact source diff, branch/tracking state, candidate native CI, corresponding sources/licenses and device acceptance notes. Prepare the project-local Python environment as described in [development](development.md). The executable .bumpster/hooks/pre-bump builds the pinned native core, runs tools/check.py and closes the Unreleased section under the new version/date. It stages CHANGELOG.md in Bumpster's version commit. VERSION is updated by Bumpster itself. A failed check aborts before changelog preparation; a partial release is diagnosed from actual Git state rather than retried blindly.

Only after explicit maintainer authorization, run `bumpster --patch` from clean, synchronized dev. This is a mutating release command: it creates commits, merges branches, tags and pushes. Do not use it to test readiness. Refuse an existing version/tag rather than rewriting it. The first release notes describe a development preview, not general device compatibility.

## Candidate workflow

The H700 candidate and release draft workflow can be dispatched on dev to build artifacts without creating a tag or GitHub Release. It runs on a native [Ubuntu ARM64 runner](https://docs.github.com/en/actions/reference/runners/github-hosted-runners). Source/model/UI/runner checks run first. Two independent offline H700 compilations then use the pinned SDK and verified dependency archives. tools/release.py bundle compares both results and readbacks, audits the ZIP and verifies that all source inputs match the checkout.

The retained workflow artifact contains the source-complete frontend ZIP, verification.json, release-notes.md and SHA256SUMS.txt. It expires after seven days; download and preserve a candidate needed for physical testing. The ZIP is a frontend package for an existing BaseOS 1.3.0 RG34XXSP installation, not firmware or a bootable SD image. A non-tag run is clearly named development, even after VERSION is initialized.

The BaseOS rootfs is an external input and is never uploaded. Its isolated preflight is a separate local check; the hosted workflow does not claim to have run it. Physical graphics, audio, input, brightness, save/resume and suspend acceptance of the exact candidate remain separate checks. See [installation/rollback](installation.md) and [compatibility](compatibility.md).

## Tagged draft and publication

A pushed vX.Y.Z tag starts the same build. It must be annotated, point at the checked-out commit reachable from origin/main, match VERSION and have one nonempty dated changelog section. The ZIP's embedded version must match as well. Website tags cannot enter this release path.

Only the draft job has contents:write. After a successful build it creates a draft prerelease, uploads the four verified assets, reads back draft/tag/asset metadata, downloads the assets and verifies checksums. It does not publish the draft or mark it as latest. Existing releases are never overwritten, and a failed/ambiguous upload is not automatically retried.

After checking the exact draft assets and required device acceptance, authorize publication separately. Then verify the public tag/ref, Release metadata and the downloaded ZIP, including its source and license audit. A green build, a draft or a successful upload is not proof of public availability or physical device behavior.
