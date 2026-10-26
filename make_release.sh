```bash
#!/usr/bin/env bash
set -euo pipefail

# Release version series
BASE_VERSION="0.9"

# Require execution from the repository root.
if ! git rev-parse --show-toplevel >/dev/null 2>&1; then
    echo "Error: Not inside a Git repository." >&2
    exit 1
fi

cd "$(git rev-parse --show-toplevel)"

# Require a clean working tree, including untracked files.
if [[ -n "$(git status --porcelain)" ]]; then
    echo "Error: Working tree is not clean." >&2
    echo "Commit or stash your changes before releasing." >&2
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
VERSION="${BASE_VERSION}_$(date +%Y%m%d)"

# Refuse to reuse an existing local or remote tag.
if git show-ref --verify --quiet "refs/tags/$VERSION"; then
    echo "Error: Tag $VERSION already exists locally." >&2
    exit 1
fi

if git ls-remote --exit-code --tags origin "refs/tags/$VERSION" \
    >/dev/null 2>&1; then
    echo "Error: Tag $VERSION already exists remotely." >&2
    exit 1
fi

# Confirm before publishing.
echo "Release: $VERSION"
echo "Commit:  $(git rev-parse --short HEAD)"
read -r -p "Create this release? [y/N] " CONFIRM

if [[ "$CONFIRM" != "y" && "$CONFIRM" != "Y" ]]; then
    echo "Cancelled."
    exit 0
fi

# Create and push the annotated tag.
git tag -a "$VERSION" -m "Release $VERSION"
git push origin "$VERSION"

# Publish the GitHub release and generate release notes.
gh release create "$VERSION" \
    --verify-tag \
    --title "$VERSION" \
    --generate-notes

echo "Release $VERSION created successfully."
```