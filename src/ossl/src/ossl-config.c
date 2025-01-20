// cc -std=c99 -Wall -Werror -Wno-error=deprecated-declarations -pedantic -D_XOPEN_SOURCE=600 -o ossl-config ossl-config.c -lcrypto

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/safestack.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#  define FALLTHROUGH [[fallthrough]]
#elif (defined(__GNUC__) && __GNUC__ >= 7) || (defined(__clang__) && __clang_major__ >= 12)
#  define FALLTHROUGH __attribute__((fallthrough))
#else
#  define FALLTHROUGH ((void) 0)
#endif

#define cleanup(type) \
    __attribute__((cleanup(type##_ptr_free)))

#define cleanupfunc(type, func) \
    static void type##_ptr_free(type **ptr) { \
        func(*ptr); \
        *ptr = NULL; \
    }

typedef STACK_OF(OPENSSL_CSTRING) ossl_sk_cstring_t;

cleanupfunc(char, OPENSSL_free)
cleanupfunc(CONF, NCONF_free)
cleanupfunc(ossl_sk_cstring_t, sk_OPENSSL_CSTRING_free)

/**
 * Print the given value to stdout escaped for the OpenSSL configuration file
 * format.
 */
static void print_escaped_value(const char *value) {
	for (const char *p = value; *p; p++) {
		switch (*p) {
			case '"':
			case '\'':
			case '#':
			case '\\':
			case '$':
				putchar('\\');
				putchar(*p);
				break;
			case '\n':
				fputs("\\n", stdout);
				break;
			case '\r':
				fputs("\\r", stdout);
				break;
			case '\b':
				fputs("\\b", stdout);
				break;
			case '\t':
				fputs("\\t", stdout);
				break;
			case ' ':
				if (p == value || p[1] == '\0') {
					/* Quote spaces if they are the first or last char of the
					 * value. We could quote the entire string (and it would
					 * certainly produce nicer output), but in quoted strings
					 * the escape sequences for \n, \r, \t, and \b do not work.
					 * To make sure we're producing correct results we'd thus
					 * have to selectively not use those in quoted strings and
					 * close and re-open the quotes if they appear, which is
					 * more trouble than adding the quotes just around the
					 * first and last leading and trailing space. */
					fputs("\" \"", stdout);
					break;
				}
				FALLTHROUGH;
			default:
				putchar(*p);
				break;
		}
	}
}

/**
 * Print all values in in the configuration section identified by section_name to stdout.
 */
static void print_section(const CONF *cnf, OPENSSL_CSTRING section_name) {
	STACK_OF(CONF_VALUE) *values = NCONF_get_section(cnf, section_name);
	for (int idx = 0; idx < sk_CONF_VALUE_num(values); idx++) {
		CONF_VALUE *value = sk_CONF_VALUE_value(values, idx);
		printf("%s = ", value->name);
		print_escaped_value(value->value);
		putchar('\n');
	}
}

/**
 * Parse the default OpenSSL configuration file (or the one specified in the
 * OPENSSL_CONF environment variable) and write it back to stdout in
 * a canonical format with all includes and variables expanded.
 */
int main(int argc, char *argv[]) {
	char *configfile cleanup(char) = CONF_get1_default_config_file();
	if (configfile == NULL) {
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}

	CONF *cnf cleanup(CONF) = NCONF_new(NULL);
	if (cnf == NULL) {
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}

	long eline = 0;
	if (NCONF_load(cnf, configfile, &eline) == 0) {
		fprintf(stderr, "Error on line %ld of configuration file\n", eline);
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}

	STACK_OF(OPENSSL_CSTRING) *sections cleanup(ossl_sk_cstring_t) = NCONF_get_section_names(cnf);
	if (sections == NULL) {
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}

	printf("# This configuration file was linarized and expanded from %s\n", configfile);

	int default_section_idx = sk_OPENSSL_CSTRING_find(sections, "default");
	if (default_section_idx != -1) {
		print_section(cnf, "default");
	}
	for (int idx = 0; idx < sk_OPENSSL_CSTRING_num(sections); idx++) {
		if (idx == default_section_idx) {
			continue;
		}
		OPENSSL_CSTRING section_name = sk_OPENSSL_CSTRING_value(sections, idx);
		printf("\n[%s]\n", section_name);
		print_section(cnf, section_name);
	}

	return EXIT_SUCCESS;
}
