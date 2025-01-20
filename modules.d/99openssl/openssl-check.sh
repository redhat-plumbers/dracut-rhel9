#!/bin/sh

eok() {

    {
        [ "$1" -eq 0 ] && echo OK || echo FAIL

        echo

    } 2> /dev/null
}

echo

set -x

openssl list -providers

eok "$?"

#openssl s_client -connect “$dns_server_ip:$dns_server_port” -servername “$dns_server_name” </dev/null

#openssl s_client -connect “$test_hostname:$test_port” </dev/null

#openssl genpkey -algorithm rsa -pkeyopt rsa_keygen_bits:2048 -out localhost.key

#openssl req -x509 -new -key localhost.key -subj /CN=localhost -days 365 -addext "subjectAltName = DNS:localhost" -out localhost.crt

#openssl s_server -cert localhost.crt -key localhost.key -port “$test_port”
