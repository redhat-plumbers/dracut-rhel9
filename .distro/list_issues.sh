#!/usr/bin/env zsh

set -e

zsh -n "$0"

: "debug"
[[ "$1" == '-d' ]] && {
  set -x
  shift ||:
  :
}

: "No fails on check"
[[ "$1" == '-n' ]] && {
  NOFAIL=y
  shift ||:
  :
} || NOFAIL=

: "Ignore build commits (defaults to failure)"
[[ "$1" == '-i' ]] && {
  IGNORE=y
  shift ||:
  :
} || IGNORE=
#clear

: 'Commit to end on'
e="${1}"
shift

ok='>>> list ok'

all="$(gitl1)"

gr='^\s*(Resolves|Related):\s*RHEL'

er='| tee -a /dev/stderr'

cmd="
  [[ '$e' == '{}' ]] && {
    echo '$ok' $er
    exit 255
  }

  m=\"\$(git log -1 '{}')\"

  grep -E '$gr' <<< \"\$m\" \
    && echo 'Found: {}' \
    || echo 'Missing: {}'

"

zsh -n -c "$cmd"

res="$(
  echo "$all" | cut -d' ' -f1 \
    | xargs -ri zsh -c "$cmd" \
    | tr -s '\t' ' ' | sed -e 's/^\s*//g'
)" ||:

[[ -n "$res" && "$res" =~ "$ok" ]]

[[ -n "$NOFAIL" ]] && {

  echo
  lst="$(
  echo "$res" | cut -d' ' -f2 \
    | while read c; do
      m="$(grep -E "^$c " <<< "$all" | cut -d' ' -f2-)"

      echo "$m" | grep -qE '^(ci|chore|test): ' \
        || echo "- $m"

    done | reverse -l
  )"

  :
} || {
  echo "$res" | grep -E "^Missing" | cut -d' ' -f2 \
    | while read c; do
      m="$(grep -E "^$c " <<< "$all" | cut -d' ' -f2-)"
      echo "$m" | grep -qE '^(build|ci|chore|test): ' || {
        echo "FAIL: $m"
        gith "$c" | cat
        echo
        exit 255
      }
    done

  echo
  lst="$(
  echo "$res" | grep -E "^Found" | cut -d' ' -f2 \
    | while read c; do
      m="$(grep -E "^$c " <<< "$all" | cut -d' ' -f2-)"

      echo "$m" | grep -qE '^(ci): ' && {

        [[ -n "$IGNORE" ]] && continue

        echo ">> False: $m"
        gith "$c" | cat
        echo
        exit 255

      } >&2 || echo "- $m"

    done | reverse -l
  )"
}

# 80
ln='-------------------------------------------------------------------------------'
echo "$ln"
echo "$lst"

isu="$(echo "$res"| grep -E "$gr" | cut -d' ' -f2 | sort -u | xargs -ri echo -n '{},')"

echo "  Resolves: ${isu%,}"
echo "$ln"
echo
#tr -s ',' '\n' <<< "$res" | sed -e 's/^/Resolves: /g'

echo "$res"| grep -E "$gr"
