# DOSBox-Y

An experimental fork of [DOSBox-X](https://github.com/joncampbell123/dosbox-x),
focused on debugger integration, Lua automation, MCP support and state persistence.

**Project website:** <https://salva.github.io/dosbox-y/>

## Development status

Active development; no stable release or general compatibility claim is made.
Work includes integration with current DOSBox-X, a shared native debugger design
for interactive and programmatic use, interactive Lua scripting, MCP integration,
and improved save/restore support. These are development directions, not a list
of completed or release-qualified features.

This repository was bootstrapped with the public project site. The integrated
emulator source will be published separately after review; this initial site
branch does not contain an emulator build or downloads.

## Website

The `gh-pages` branch contains a small static site (`index.html`), with no build
dependencies. GitHub Pages publishes the branch root. `.nojekyll` disables Jekyll
processing. The branch is independent so an upstream-based source branch can
later be published as `main` without mixing the website's bootstrap history into
the emulator integration.

Preview locally with `python3 -m http.server 8765` from this directory, then open
<http://localhost:8765/>. Publish reviewed site changes with ordinary commits and
pushes to `gh-pages`; do not force-push.

## Upstream and licensing

DOSBox-Y builds on the work of the DOSBox and DOSBox-X contributors and is not an
official DOSBox-X release. The project retains the GNU GPL v2 license text in
[COPYING](COPYING). Upstream source carries its own copyright and component
notices; those must remain with any published source or distribution.
