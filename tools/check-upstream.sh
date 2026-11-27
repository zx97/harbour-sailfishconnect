#!/usr/bin/env bash
#
# Report the state of the upstream project and of our pull request, and
# notify when something changed since the previous run.
#
# The first run records the current state and sends a summary. Afterwards a
# notification is only sent when the pull request or the upstream branch
# moved.
#
# Usage:
#   tools/check-upstream.sh [--dry-run] [--force] [--no-notify]
#
# Options:
#   --dry-run     print the report, change no state, send nothing
#   --force       notify even when nothing changed
#   --no-notify   print the report and record the state, send nothing
#
# Environment:
#   UPSTREAM_REPO   default R1tschY/harbour-sailfishconnect
#   FORK            default zx97/harbour-sailfishconnect
#   PR              default 72
#   NTFY_URL        default https://ntfy.sh/Openc0d3_M4nu
#   MAIL_TO         default manuel.flury@gmail.com
#   GITHUB_TOKEN    optional, raises the API rate limit
#
# Exit status: 0 on success (changed or not), 1 on a network or API error.

set -u

UPSTREAM_REPO=${UPSTREAM_REPO:-R1tschY/harbour-sailfishconnect}
FORK=${FORK:-zx97/harbour-sailfishconnect}
PR=${PR:-72}
NTFY_URL=${NTFY_URL:-https://ntfy.sh/Openc0d3_M4nu}
MAIL_TO=${MAIL_TO:-manuel.flury@gmail.com}
STATE_DIR=${XDG_STATE_HOME:-$HOME/.local/state}/sailfish-connect
STATE_FILE=$STATE_DIR/upstream-state

DRY_RUN=0
FORCE=0
NOTIFY=1
for argument in "$@"; do
    case "$argument" in
        --dry-run) DRY_RUN=1; NOTIFY=0 ;;
        --force) FORCE=1 ;;
        --no-notify) NOTIFY=0 ;;
        -h|--help) sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) printf 'Unknown option: %s\n' "$argument" >&2; exit 1 ;;
    esac
done

command -v curl >/dev/null || { echo "curl is required" >&2; exit 1; }
command -v jq >/dev/null || { echo "jq is required" >&2; exit 1; }

api() {
    local path=$1
    local headers=(-H "Accept: application/vnd.github+json"
                   -H "User-Agent: sailfishconnect-upstream-check")
    if [ -n "${GITHUB_TOKEN:-}" ]; then
        headers+=(-H "Authorization: Bearer $GITHUB_TOKEN")
    fi
    curl -fsS --max-time 20 "${headers[@]}" "https://api.github.com$path"
}

die() {
    printf '%s\n' "$1" >&2
    exit 1
}

# pull request
if ! pr=$(api "/repos/$UPSTREAM_REPO/pulls/$PR"); then
    die "Cannot read pull request #$PR of $UPSTREAM_REPO (network or API error)"
fi
case $(printf '%s' "$pr" | jq -r '.message // empty') in
    "API rate limit"*|"Not Found"*) die "GitHub API: $(printf '%s' "$pr" | jq -r .message)" ;;
esac

if ! reviews=$(api "/repos/$UPSTREAM_REPO/pulls/$PR/reviews"); then
    die "Cannot read the reviews of pull request #$PR"
fi

if ! upstream=$(api "/repos/$UPSTREAM_REPO/commits/master"); then
    die "Cannot read the master branch of $UPSTREAM_REPO"
fi

pr_state=$(printf '%s' "$pr" | jq -r '.state')
pr_merged=$(printf '%s' "$pr" | jq -r '.merged')
pr_updated=$(printf '%s' "$pr" | jq -r '.updated_at')
pr_comments=$(printf '%s' "$pr" | jq -r '.comments')
pr_review_comments=$(printf '%s' "$pr" | jq -r '.review_comments')
pr_commits=$(printf '%s' "$pr" | jq -r '.commits')
pr_mergeable=$(printf '%s' "$pr" | jq -r '.mergeable_state')
pr_title=$(printf '%s' "$pr" | jq -r '.title')
review_count=$(printf '%s' "$reviews" | jq -r 'length')
upstream_sha=$(printf '%s' "$upstream" | jq -r '.sha[0:8]')
upstream_date=$(printf '%s' "$upstream" | jq -r '.commit.committer.date')
upstream_subject=$(printf '%s' "$upstream" | jq -r '.commit.message | split("\n")[0]')

pr_url="https://github.com/$UPSTREAM_REPO/pull/$PR"

merged_info=""
[ "$pr_merged" = "true" ] && merged_info=", merged"
report="Pull request #$PR ($pr_state$merged_info, $pr_mergeable)
  $pr_title
  updated $pr_updated, $pr_commits commit(s)
  $pr_comments comment(s), $pr_review_comments review comment(s), $review_count review(s)
  $pr_url
Upstream master: $upstream_sha ($upstream_date)
  $upstream_subject
Our fork master: https://github.com/$FORK/commits/master
"

# previous state
previous_state=""
previous_merged=""
previous_updated=""
previous_comments=0
previous_review_comments=0
previous_reviews=0
previous_upstream=""
first_run=0
if [ -r "$STATE_FILE" ]; then
    # shellcheck disable=SC1090
    . "$STATE_FILE"
else
    first_run=1
fi

changes=()
if [ "$first_run" = 1 ]; then
    changes+=("first run")
else
    [ "$pr_state" != "${previous_state:-}" ] &&
        changes+=("pull request is now $pr_state")
    [ "$pr_merged" = "true" ] && [ "$previous_merged" != "true" ] &&
        changes+=("pull request was merged")
    [ "$pr_updated" != "$previous_updated" ] &&
        changes+=("pull request activity")
    [ "$pr_comments" -gt "${previous_comments:-0}" ] &&
        changes+=("$((pr_comments - previous_comments)) new comment(s)")
    [ "$pr_review_comments" -gt "${previous_review_comments:-0}" ] &&
        changes+=("$((pr_review_comments - previous_review_comments)) new review comment(s)")
    [ "$review_count" -gt "${previous_reviews:-0}" ] &&
        changes+=("$((review_count - previous_reviews)) new review(s)")
    [ "$upstream_sha" != "${previous_upstream:-}" ] &&
        changes+=("upstream master moved to $upstream_sha")
fi

printf '%s' "$report"
if [ ${#changes[@]} -gt 0 ]; then
    printf '\nChanges:\n'
    printf '  - %s\n' "${changes[@]}"
else
    printf '\nNo change.\n'
fi

notify_ntfy() {
    local title=$1
    local body=$2
    curl -fsS --max-time 20 -H "Title: $title" -H "Tags: arrow_up" \
        -d "$body" "$NTFY_URL" >/dev/null
}

notify_mail() {
    local subject=$1
    local body=$2
    if ! command -v mail >/dev/null; then
        printf 'mail(1) not available, email not sent\n' >&2
        return 1
    fi
    printf '%s\n' "$body" | mail -s "$subject" "$MAIL_TO"
}

if [ "$DRY_RUN" = 0 ]; then
    mkdir -p "$STATE_DIR"
    cat >"$STATE_FILE" <<EOF
previous_state=$pr_state
previous_merged=$pr_merged
previous_updated=$pr_updated
previous_comments=$pr_comments
previous_review_comments=$pr_review_comments
previous_reviews=$review_count
previous_upstream=$upstream_sha
EOF
fi

if [ "$NOTIFY" = 1 ] && { [ ${#changes[@]} -gt 0 ] || [ "$FORCE" = 1 ]; }; then
    if [ ${#changes[@]} -gt 0 ]; then
        summary="PR #$PR: ${changes[*]}"
    else
        summary="PR #$PR: forced report"
    fi

    notify_ntfy "KDE Connect upstream" "$summary
$pr_url" || printf 'ntfy notification failed\n' >&2
    notify_mail "[sailfishconnect] upstream: $summary" "$report
Changes:
  $(printf '%s\n  ' "${changes[@]:-none}")" ||
        printf 'email notification failed\n' >&2
fi

exit 0
