# Release design

Draft maintainer procedure. No release version has been selected and these operations have not been executed for Rondo.

## Development and publication

Use dev for ordinary work and main for released state. Keep changes small enough to build and review independently. Documentation describes the checked-out code. A normal commit/push does not imply a version bump, release or website deployment.

## Read-only preparation

Bumpster 1.3.0 recognizes the checked-in configuration. Inspect the repository without starting a release:

```sh
bumpster --version
bumpster --status
```

The configuration selects main as the release branch, dev as the development/start/return branch and disables package.json synchronization. VERSION is intentionally absent until the first version is selected. There is no binary publication workflow yet. Neither this file nor .bumpsterrc establishes a ready-to-run release: version initialization, changelog preparation, candidate checks and an artifact publication workflow must be completed first. Do not run a bump command as a readiness check.

## Rondo releases

Use Bumpster from a clean, synchronized dev branch. For a versioned release, the root VERSION is authoritative; root package.json synchronization is disabled because the frontend is a C++ product. Tags follow vX.Y.Z. Bumpster handles the dev → main → dev flow. The first number and first release scope need an explicit decision; local test-kit numbers are not product versions.

Before a release, inspect the exact candidate diff/ref, relevant native and BaseOS checks, device acceptance notes, VERSION/changelog consistency, dependency provenance, licenses and corresponding sources. Build a frontend ZIP with a manifest and checksums from that candidate. Confirm installation and rollback on the supported target. Keep commercial ROMs, private state and vendor OS images out of the artifact.

Commit, bump, tag, push, GitHub Release creation and SD installation remain explicitly authorized maintainer operations. After publication, independently read back the tag/ref and public assets, download the actual ZIP and verify its checksums and contents. A green build alone does not establish device behavior or successful publication.

## Future website releases

The independent website profile will be installed as .bumpster/packages/website.json when real website/VERSION and website/package.json exist and are tracked. No website profile is configured yet.

With a Bumpster version supporting package releases, website status/release commands run from the repository root using --package website. The profile selects website/VERSION, website/package.json and website/vX.Y.Z tags; it preserves the dev → main → dev flow. Frontend releases use the root VERSION and vX.Y.Z tags.

Website CI validates content/locales, source, generated output and relevant browser behavior. Its release/deployment workflow responds only to the website release contract. It must not publish frontend ZIPs or bump Rondo's version. Verify the deployed version and served content separately from build and deploy-job status.

The hosting target, domain, first website version and deployment authorization are intentionally undecided. Site generation starts in a later task.
