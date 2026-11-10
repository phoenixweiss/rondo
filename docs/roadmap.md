# Rondo roadmap

See the [changelog](../CHANGELOG.md) for delivered changes and the
[compatibility guide](compatibility.md) for the current hardware, BaseOS and core
coverage.

## Planned work

- Browser transfer of folders/ZIP, then SMB and separately researched USB exchange;
  conflict-aware backup/sync, online updates and artwork acquisition.
- Screenshot gallery, fast-forward/turbo/hotkeys, collections/game switching,
  broader core-option overrides and measured rewind.
- PICO-8 after runtime/content-license work; profiles/PIN, localized game metadata,
  achievements/cheats, CPU/display/LED options and restrained blurred backgrounds.
- Battery-gauge/offline-charging research, wake-on-open and additional controllers/
  rumble profiles. New data-safety or power faults override the ordinary queue.
- A separate Vue website in English/Russian, based on real features, screenshots,
  installation, credits and release history, with its own version/CI/deployment.
- Additional H700 devices and Android tablet/TV interaction as separate projects.

## Working rhythm

Use complete, meaningful batches: quiet native checks, one BaseOS preflight and
one combined hardware test. Do not make every cosmetic edit a separate SD update.
Add new ideas to the later backlog rather than silently enlarging the stable gate.
