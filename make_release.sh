#!/usr/bin/env bash
set -euo pipefail

# Release version series
BASE_VERSION="0.9.0"

# Require execution from the repository root.
if ! git rev-parse --show-toplevel >/dev/null 2>&1; then
    echo "Error: Not inside a Git repository." >&2
    exit 1
fi

cd "$(git rev-parse --show-toplevel)"

# Require GitHub CLI authentication before making any release changes.
if ! gh auth status; then
    echo "Error: GitHub CLI is not authenticated. Run 'gh auth login' first." >&2
    exit 1
fi

# Require the intended release branch.
BRANCH="$(git branch --show-current)"
if [[ "$BRANCH" != "master" ]]; then
    echo "Error: Releases must be made from master (currently on $BRANCH)." >&2
    exit 1
fi

# Ensure the local branch is synchronized with origin.
git fetch origin

if [[ "$(git rev-parse HEAD)" != "$(git rev-parse origin/master)" ]]; then
    echo "Error: Local master does not match origin/master." >&2
    echo "Synchronize your branch before releasing." >&2
    exit 1
fi

# Generate today's release tag.
VERSION="v${BASE_VERSION}_$(date +%Y%m%d)"

# Reuse a local tag on this commit so an interrupted release can be retried.
TAG_EXISTS_LOCALLY=0
if git show-ref --verify --quiet "refs/tags/$VERSION"; then
    TAG_COMMIT="$(git rev-parse "refs/tags/$VERSION^{commit}")"
    if [[ "$TAG_COMMIT" != "$(git rev-parse HEAD)" ]]; then
        echo "Error: Local tag $VERSION does not point to HEAD." >&2
        exit 1
    fi
    TAG_EXISTS_LOCALLY=1
fi

if [[ "$TAG_EXISTS_LOCALLY" -eq 0 ]]; then
    if git ls-remote --exit-code --tags origin "refs/tags/$VERSION" \
        >/dev/null 2>&1; then
        echo "Error: Tag $VERSION already exists remotely." >&2
        exit 1
    fi
fi

# Confirm before publishing.
echo "Release: $VERSION"
echo "Commit:  $(git rev-parse --short HEAD)"
read -r -p "Create this release? [y/N] " CONFIRM

if [[ "$CONFIRM" != "y" && "$CONFIRM" != "Y" ]]; then
    echo "Cancelled."
    exit 0
fi

# Create the annotated tag if this is a new release.
if [[ "$TAG_EXISTS_LOCALLY" -eq 0 ]]; then
    git tag -a "$VERSION" -m "Release $VERSION"
fi

# Push the tag; this is also safe when retrying an interrupted release.
git push origin "$VERSION"

# Publish the GitHub release and generate release notes.
gh release create "$VERSION" \
    --verify-tag \
    --title "$VERSION" \
    --generate-notes

echo "Release $VERSION created successfully."