#!/usr/bin/env bash
# PreToolUse hook (Bash): block git commands that create or publish commits unless the
# identity is the fork maintainer's personal one. Exit 2 = block (stderr is shown to Claude).
set -u

EXPECTED_NAME="Marcelo Frau"
EXPECTED_EMAIL="marcelofrau@gmail.com"

input=$(cat)
cmd=$(printf '%s' "$input" | jq -r '.tool_input.command // ""')
cwd=$(printf '%s' "$input" | jq -r '.cwd // ""')

# Only git commands that create or publish commits.
printf '%s' "$cmd" | grep -Eq '(^|[^[:alnum:]_-])git([^[:alnum:]_-]|$)' || exit 0
printf '%s' "$cmd" | grep -Eqw 'commit|push|merge|cherry-pick|revert|rebase|am|tag' || exit 0

[ -n "$cwd" ] && cd "$cwd" 2>/dev/null
# Follow a leading `cd <dir>` or `git -C <dir>` so the target repo's identity is checked.
target=$(printf '%s' "$cmd" | sed -nE 's/.*git[[:space:]]+-C[[:space:]]+("([^"]+)"|([^[:space:]]+)).*/\2\3/p' | head -1)
[ -n "$target" ] || target=$(printf '%s' "$cmd" | sed -nE 's/^[[:space:]]*cd[[:space:]]+("([^"]+)"|([^[:space:];&|]+)).*/\2\3/p' | head -1)
[ -n "$target" ] && cd "$target" 2>/dev/null
git rev-parse --git-dir >/dev/null 2>&1 || exit 0

fail() {
  echo "BLOCKED by check-git-identity: $1" >&2
  echo "Expected identity: $EXPECTED_NAME <$EXPECTED_EMAIL> (never the corporate account)." >&2
  echo "Fix with: git config --local user.name \"$EXPECTED_NAME\" && git config --local user.email \"$EXPECTED_EMAIL\"" >&2
  exit 2
}

name=$(git config user.name 2>/dev/null || true)
email=$(git config user.email 2>/dev/null || true)
[ "$name" = "$EXPECTED_NAME" ] || fail "git user.name is '$name'"
[ "$email" = "$EXPECTED_EMAIL" ] || fail "git user.email is '$email'"

for var in GIT_AUTHOR_NAME GIT_COMMITTER_NAME; do
  val=${!var-}
  [ -z "$val" ] || [ "$val" = "$EXPECTED_NAME" ] || fail "$var is set to '$val'"
done
for var in GIT_AUTHOR_EMAIL GIT_COMMITTER_EMAIL; do
  val=${!var-}
  [ -z "$val" ] || [ "$val" = "$EXPECTED_EMAIL" ] || fail "$var is set to '$val'"
done

# Inline overrides in the command itself.
if printf '%s' "$cmd" | grep -Eq -- '--author[= ]'; then
  printf '%s' "$cmd" | grep -Fq "$EXPECTED_EMAIL" || fail "command overrides --author with another identity"
fi
if printf '%s' "$cmd" | grep -Eq -- '-c[[:space:]]+user\.(name|email)=|(GIT_(AUTHOR|COMMITTER)_(NAME|EMAIL))='; then
  fail "command overrides the git identity inline"
fi

# Before publishing: every local commit that is not upstream's must be ours (author and committer).
if printf '%s' "$cmd" | grep -Eqw 'push'; then
  if git remote | grep -qx upstream; then
    bad=$(git log --branches --not --remotes=upstream --format='%h %an <%ae> / %cn <%ce>' 2>/dev/null |
      grep -v -F "$EXPECTED_NAME <$EXPECTED_EMAIL> / $EXPECTED_NAME <$EXPECTED_EMAIL>" || true)
    [ -z "$bad" ] || fail "local commits with a foreign identity would be pushed:
$bad"
  fi
fi

exit 0
