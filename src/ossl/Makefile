.PHONY: all clean tests

CFLAGS ?= -std=c99 -Wall -Werror -pedantic -D_XOPEN_SOURCE=600
CRYPTO_FLAGS = -lcrypto -Wl,-pie -Wl,-z,now -fPIE
TARGETS = src/ossl-config src/ossl-files

TESTS_CONFIG = $(wildcard tests/config/*.cnf)
TESTS_FILES = $(wildcard tests/files/*.cnf)

all: $(TARGETS)

clean:
	$(RM) $(TARGETS)
	$(RM) tests/config/*.1 tests/config/*.2
	$(RM) tests/files/*.1

%: %.c
	$(CC) $(CFLAGS) $(CRYPTO_FLAGS) -o $@ $<

test: $(TARGETS)
	@for TEST in $(TESTS_CONFIG); do \
		echo "Test $$TEST..."; \
		OPENSSL_CONF="$$TEST" src/ossl-config >"$$TEST.1" && \
		OPENSSL_CONF="$$TEST.1" src/ossl-config >"$$TEST.2" && \
		diff -u "$$TEST.expected" "$$TEST.1" && \
		diff -u <(sed 1d "$$TEST.1") <(sed 1d "$$TEST.2") && \
		echo "PASS" || (echo "FAIL"; exit 1); \
	done
	
	@for TEST in $(TESTS_FILES); do \
		echo "Test $$TEST..."; \
		OPENSSL_CONF="$$TEST" src/ossl-files --engines --providers >"$$TEST.1" && \
		diff -u "$$TEST.expected" "$$TEST.1" && \
		echo "PASS" || (echo "FAIL"; exit 1); \
	done
