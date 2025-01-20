#!/bin/bash

check() {
    return 255
}

install() {

    local ossl_files openssl_cnf initrd_openssl_cnf

    ossl_files="${dracutbasedir}/ossl-files"

    openssl_cnf="$($ossl_files --config)"

    initrd_openssl_cnf="${initdir}/${openssl_cnf}"

    if [[ ! -r $openssl_cnf ]]; then
        dfatal "'$ossl_files --config' does not return a path!!"
        exit 1
    fi

    # ossl-files gives us one line per file
    # shellcheck disable=SC2046
    inst_multiple -o \
        /etc/crypto-policies/back-ends/opensslcnf.config \
        $($ossl_files --engines --providers)

    mkdir -p "${initrd_openssl_cnf%/*}"

    "${dracutbasedir}/ossl-config" > "${initrd_openssl_cnf}"
}
