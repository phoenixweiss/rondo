# Maintainer releases

Rondo uses Bumpster for versioning and GitHub Releases for RG34XXSP packages. VERSION is updated in a separate commit by Bumpster.

## Contract

- Frontend: root VERSION, vX.Y.Z annotated tags, dev → main → dev through Bumpster, no package.json synchronization.

Use Bumpster 1.4.0 or a compatible version. Read-only inspection:

```sh
bumpster --version
bumpster --status
```

## Before a bump

Inspect the exact source diff, branch/tracking state, candidate native CI, corresponding sources/licenses and device acceptance notes. Prepare the project-local Python environment as described in [development](development.md).

After selecting and authorizing the next version, prepare the changelog before the ordinary change commit:

```sh
BUMPSTER_PREV_VERSION=0.2.0 BUMPSTER_NEW_VERSION=0.3.0 .venv/bin/python tools/release.py prepare-changelog
```

Substitute the actual versions. Commit and push the reviewed changes/changelog, then wait for their native CI. The executable `.bumpster/hooks/pre-bump` builds the pinned native core, runs `tools/check.py` and validates the prepared section without changing the index or working tree. Bumpster 1.4 requires that hook to be read-only. Bumpster updates VERSION in its separate version commit. A failed check aborts; inspect actual refs before retrying a partial release.

Only after explicit maintainer authorization, run the selected Bumpster action from clean, synchronized dev (`bumpster --minor` for 0.0.1 → 0.1.0; `--patch` for a patch). This is a mutating release command: it creates commits, merges branches, tags and pushes. Do not use it to test readiness. Refuse an existing version/tag rather than rewriting it.

## Candidate workflow

The H700 candidate and release draft workflow can be dispatched on dev to build artifacts without creating a tag or GitHub Release. It runs on a native [Ubuntu ARM64 runner](https://docs.github.com/en/actions/reference/runners/github-hosted-runners). Source/model/UI/runner checks run first. Two independent offline H700 compilations then use the pinned SDK and verified dependency archives. tools/release.py bundle compares both results and readbacks, audits the ZIP and verifies that all source inputs match the checkout.

Pinned source archives are cached separately from compiled dependencies. Every
restored archive is checked against its pinned SHA-256 before use. Downloads use
bounded retries and the official mirrors recorded with each dependency. A checksum
mismatch stops the build instead of silently selecting another source. Source
caching does not change the two independent `--fresh` compilations.

The retained workflow artifact contains the source-complete frontend ZIP, verification.json, release-notes.md and SHA256SUMS.txt. It expires after seven days; download and preserve a candidate needed for physical testing. Install the frontend ZIP on a card that already boots BaseOS on an RG34XXSP. Manual candidate builds retain a development filename.

The BaseOS rootfs is an external input to isolated userspace checks and stays out of the ZIP. Physical graphics, audio, input, brightness and suspend are separate on-device checks. See [installation/rollback](installation.md) and [compatibility](compatibility.md).

## Tagged draft and publication

A pushed vX.Y.Z tag starts the same build. It must be annotated, point at the checked-out commit reachable from origin/main, match VERSION and have one nonempty dated changelog section. The ZIP's embedded version must match as well. Website tags cannot enter this release path.

The tagged workflow sends the already verified ZIP through the BaseOS matrix. Each job checks the package SHA-256 against the build job output. Once all versions pass, the draft job creates a regular release draft, uploads the four verified assets, reads back draft/tag/asset metadata, downloads the assets and verifies checksums. Only the draft job has contents:write. It does not publish the draft or mark it as latest. Existing releases are never overwritten, and a failed/ambiguous upload is not automatically retried.

Check the exact draft assets and recorded on-device results before the authorized publication. Then verify the public tag/ref, Release metadata and the downloaded ZIP, including its source and license audit. A green build, a draft or a successful upload is not proof of public availability or physical device behavior.

## Ongoing changelog and website inputs

- Add an English Unreleased entry with each delivered logical change, including documentation changes. Describe actual behavior and meaningful limits; planned features stay in the roadmap/feature review. Do not claim a new device or core is supported on the strength of a build alone.
- Keep VERSION at the last released number while developing. Do not bump for planning edits or assign future capability IDs to an invented release. Choose the next version from the actual reviewed scope, then advance it through the authorized Bumpster flow.
- Preserve dated published sections. The preparation command moves completed Unreleased notes to the selected version, and the pre-bump hook validates the resulting section; never rewrite old notes to make a planned feature appear delivered. Release dates currently use UTC in tools/release.py.
- Keep stable feature-review IDs in task/evidence references when useful. A capability is released only when its implementation, relevant checks and public artifact are established; track device/core limits alongside the notes.
- The future website uses dated changelog sections, canonical Release links and checked compatibility as its release history. Label Unreleased changes as unreleased and roadmap entries as planned; do not maintain an independent manually versioned product history. The website itself has an independent version.
