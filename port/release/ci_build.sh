#!/usr/bin/env bash
# Builds, packages and checks both release archives inside the container of port/release/Dockerfile.ci
# (.github/workflows/release.yml, docs/port/release.md "Releases from CI"). Run from the checkout's root:
#
#   TTT_RELEASE_TAG=v0.1.0 bash port/release/ci_build.sh     a tag build: the archives are named after the tag
#   bash port/release/ci_build.sh                            any other build: named after `git describe`
#
# Writes dist/ttt-<version>-linux-x86_64.tar.gz, dist/ttt-<version>-windows-x86_64.zip, dist/SHA256SUMS and
# dist/ttt-<version>-portability.txt. Needs no game file and publishes nothing.
set -euo pipefail

cd "$(dirname "$0")/../.."
git config --global --add safe.directory "$PWD"

tag="${TTT_RELEASE_TAG:-}"
if [[ -n "$tag" ]]; then
    if ! git rev-parse -q --verify "refs/tags/$tag" >/dev/null; then
        echo "ci_build: tag $tag is not in the checkout (fetch the tags: fetch-depth 0)" >&2
        exit 1
    fi
    # The version is `git describe --always --dirty` (release.py, the build's TTT_GIT), which names annotated tags
    # only. A lightweight tag, or one the checkout fetched without its tag object, is made annotated here, locally
    # and at the same commit, so the archives and `ttt --version` carry the tag's name. Nothing is pushed.
    if [[ "$(git cat-file -t "refs/tags/$tag")" != "tag" ]]; then
        git -c user.name=ci -c user.email=ci@localhost tag -f -a "$tag" -m "$tag" "$tag^{commit}" >/dev/null
    fi
    described="$(git describe --always --dirty)"
    if [[ "$described" != "$tag" ]]; then
        echo "ci_build: git describe gives $described, not the tag $tag (another annotated tag on this commit?)" >&2
        exit 1
    fi
fi

echo "ci_build: version $(git describe --always --dirty)"
echo "ci_build: $(clang --version | head -n 1); $(x86_64-w64-mingw32-gcc --version | head -n 1)"
if [[ -n "${TTT_SDL3_DLL:-}" ]]; then
    echo "ci_build: SDL3.dll ${TTT_SDL3_VERSION:-} from $TTT_SDL3_DLL"
else
    echo "ci_build: no SDL3.dll given: the Windows archive will not include it" >&2
fi

# release.py builds both targets (port/tools/build.py, TTT_CC=win64, build_server.py [--win64]), checks the
# portability limits and the content of each archive, and writes the checksums. A tree with local changes is refused.
python3 port/tools/release.py --targets linux,windows

echo "ci_build: dist/SHA256SUMS"
cat dist/SHA256SUMS
