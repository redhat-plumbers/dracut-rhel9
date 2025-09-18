#!/usr/bin/env zsh
#
# .distro/backport_fix.sh [options] DISTRO_VERSION JIRA_ISSUE DRACUT_PR [COMMIT_COUNT [COMMITS_ORIGIN_REPO]]
#
#  Note: fedora support is experimental
#

set -xe
zsh -n "$0"

[[ -z $EDITOR ]] && EDITOR=nano

: "OPT: continue after solving cherry-pick conflict"
[[ $1 == "-c" ]] && {
    {
        shift || :
    } 2> /dev/null
    CON=y
    :
} || CON=

: "OPT: delete conflicting branch"
[[ $1 == "-d" ]] && {
    {
        shift || :
    } 2> /dev/null
    DEL=y
    :
} || DEL=

: "OPT: Fedora distro"
[[ $1 == "-f" ]] && {
    {
        shift || :
    } 2> /dev/null
    FED=y
    :
} || FED=

: "OPT: local changes only"
[[ $1 == "-l" ]] && {
    {
        shift || :
    } 2> /dev/null
    LOC=y

} || LOC=

: "OPT: expect ref (commit-ish to get commits from) instead of PR #"
[[ $1 == "-r" ]] && {
    {
        shift || :
    } 2> /dev/null
    REF=y

} || REF=

: "OPT: skip already applied commits"
[[ $1 == "-s" ]] && {
    SKI="$2"
    {
        shift 2 || :
    } 2> /dev/null
    :
} || SKI=0

: 'No more opts (check order)'
{
    [[ -n $1 ]] && [[ ${1:0:1} == "-" ]] && exit 4

} 2> /dev/null

{ echo; } 2> /dev/null

[[ -z $FED ]] && {
    : 'DISTRO version #'
    rv="${1}"
    {
        [[ -n $rv ]]
        shift
    } 2> /dev/null

    : 'Jira issue #'
    bn="${1}"
    {
        [[ -n $bn ]]
        shift || :
    } 2> /dev/null
}

: 'Dracut pull request or REF'
pr="${1}"
{
    [[ -n ${pr} ]]
    shift || :
} 2> /dev/null

: 'Commit count'
cc="${1:-1}"
{
    [[ -n $cc ]]
    shift || :
} 2> /dev/null

: 'Commits origin repo'
or="${1:-upstream-ng}"
{
    [[ -n $or ]]
    shift || :
} 2> /dev/null

: 'No extra arg'
{
    [[ -z $1 ]]

} 2> /dev/null

{ echo; } 2> /dev/null

[[ -z $FED ]] && {
    dist=rhel
    remote="${dist}-${rv}"
    :
} || {
    dist=fedora
    remote=main
}

[[ -z $REF ]] && rf="pr${pr}" || rf="${or}/${pr}"
[[ -z $bn ]] && bn="${pr}"

{ echo; } 2> /dev/null

[[ -z $CON ]] && {
    : "Create ${remote}-fix-${bn}?"
    read '?-->continue?'

    gitt
    gitc "${remote}"

    [[ -n $DEL ]] && gitbd "${remote}-fix-${bn}" || :

    [[ -z $FED ]] && {
        gitp "${remote}"
        gitcb "${remote}-fix-${bn}"
        gitrh "${remote}/main"
        :
    } || {

        gitfo
        gitcb "backport-fix-${bn}"
        gitrho
    }

    [[ -z $REF ]] && gitf "${or}" "refs/pull/${pr}/head:${rf}"
}

: "List Commits"
cis="$(gitl1 "${rf}" "-${cc}" --reverse | cut -d' ' -f1)"
[[ -n ${cis} ]]

com="\nCherry-picked commits:\n${cis}\n"

[[ -z $FED ]] && {
    com="${com}\nResolves: RHEL-${bn}\n"
}

echo -e "${com}"

read '?-->continue?'

i=0
echo "${cis}" \
    | while read ci; do
        [[ -n ${ci} ]] || continue

        i=$((i + 1))

        [[ $i -le $SKI ]] && continue

        gityx "${ci}" || {

            mod="$(gits | grep '^\s*both modified: ')" || :

            [[ -z $mod ]] || {

                mod=$(echo -e "$mod" | tr -s ' ' | cut -d' ' -f3)

                ls -d $mod

                $EDITOR $mod

                gita $mod

                gitdh

                gits

                exit 2
            }

            gits | grep -q '^nothing to commit' \
                && {
                    gits | grep 'git cherry-pick --skip'

                    gity --skip
                    :
                } || {

                gits

                exit 3
            }
        }
    done

read '?-->continue?'

[[ -z $CON ]] && {
    [[ ${cc} -gt 1 ]] && {

        gitei HEAD~${cc}
        :
    } || {

        gitia --amend
    }
    :
} || {
    gits | grep -q '^\s*both modified: ' \
        && gita $(gits | grep '^\s*both modified: ' | tr -s ' ' | cut -d' ' -f3)

    gityc || :
}

gitl || :
gitlp || :

[[ -z $LOC ]] || exit 0

[[ -z $FED ]] && {
    gituu "${remote}"
    :

} || {
    gituu

}

gh pr create -f -a '@me' -R "redhat-plumbers/dracut-rhel${rv}"
