// cc -std=c99 -Wall -Werror -Wno-error=deprecated-declarations -pedantic -D_XOPEN_SOURCE=600 -o ossl-files ossl-files.c -lcrypto

#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/safestack.h>

#include <getopt.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

typedef enum flag {
	CONFIG_FILE = 1,
	ENGINES,
	PROVIDERS,
	PKCS11_MODULES,
} flag_t;

static const OPENSSL_CSTRING get_option(STACK_OF(CONF_VALUE) *section, const OPENSSL_CSTRING name) {
	for (size_t idx = 0; idx < sk_CONF_VALUE_num(section); ++idx) {
		const CONF_VALUE *value = sk_CONF_VALUE_value(section, idx);
		if (strcmp(name, value->name) == 0) {
			return value->value;
		}
	}

	return NULL;
}

/**
 * Locate a section in the OpenSSL configuration file given its path
 * components, separated by dots.
 *
 * Returns the STACK_OF(CONF_VALUE) that represents the section, if it exists
 * and NULL otherwise.
 */
static STACK_OF(CONF_VALUE) *locate_section(const CONF* cnf, const OPENSSL_CSTRING path) {
	STACK_OF(CONF_VALUE) *sect = NCONF_get_section(cnf, "default");
	if (sect == NULL)
		return NULL;

	char *pathbuf cleanup(char) = OPENSSL_strdup(path);
	char *curpath = pathbuf;
	while (curpath) {
		char *split = strchr(curpath, '.');
		char *nextpath = NULL;

		if (split != NULL) {
			*split = '\0';
			nextpath = split + 1;
		}

		const OPENSSL_CSTRING next_section_name = get_option(sect, curpath);
		if (next_section_name == NULL)
			return NULL;

		sect = NCONF_get_section(cnf, next_section_name);
		if (sect == NULL)
			return NULL;

		curpath = nextpath;
	}

	return sect;
}

static void list_providers(const CONF *cnf) {
	const char *modulesdir = OPENSSL_info(OPENSSL_INFO_MODULES_DIR);

	{
		struct stat st;
		size_t pathlen = strlen(modulesdir) + 1 /* "/" */ + strlen("fips.so") + 1;
		char pathbuf[pathlen];

		snprintf(pathbuf, pathlen, "%s/fips.so", modulesdir);
		pathbuf[pathlen - 1] = '\0';

		if (stat(pathbuf, &st) == 0) {
			/* Print the path to the FIPS provider if it exists on disk,
			 * regardless of whether it is enabled or not. This is because some
			 * distributions (like Fedora and RHEL) auto-enable the FIPS
			 * provider if the kernel command line contains fips=1. */
			puts(pathbuf);
		}
	}

	STACK_OF(CONF_VALUE) *providers_sect = locate_section(cnf, "openssl_conf.providers");
	if (providers_sect == NULL)
		return;

	for (size_t idx = 0; idx < sk_CONF_VALUE_num(providers_sect); ++idx) {
		const CONF_VALUE *value = sk_CONF_VALUE_value(providers_sect, idx);
		/* The section name in the providers section is typically the basename
		 * of the loadable module, unless the section for this provider
		 * contains a 'module' option. */
		const OPENSSL_CSTRING provider_name = value->name;
		const OPENSSL_CSTRING section_name = value->value;

		if (strcmp(provider_name, "default") == 0
				|| strcmp(provider_name, "base") == 0
				|| strcmp(provider_name, "fips") == 0) {
			/* This is either a builtin provider, which does not exist on disk,
			 * or it was handled earlier. */
			continue;
		}

		STACK_OF(CONF_VALUE) *section = NCONF_get_section(cnf, section_name);
		if (section == NULL) {
			printf("%s/%s.so\n", modulesdir, provider_name);
		} else {
			OPENSSL_CSTRING module_path = get_option(section, "module");
			if (module_path) {
				if (*module_path == '/') {
					puts(module_path);
				} else {
					printf("%s/%s\n", modulesdir, module_path);
				}
			} else {
				printf("%s/%s.so\n", modulesdir, provider_name);
			}
		}
	}
}

static void list_engines(const CONF *cnf) {
	const char *enginesdir = OPENSSL_info(OPENSSL_INFO_ENGINES_DIR);

	STACK_OF(CONF_VALUE) *engines_sect = locate_section(cnf, "openssl_conf.engines");
	if (engines_sect == NULL)
		return;

	for (size_t idx = 0; idx < sk_CONF_VALUE_num(engines_sect); ++idx) {
		const CONF_VALUE *value = sk_CONF_VALUE_value(engines_sect, idx);
		const OPENSSL_CSTRING section_name = value->value;

		STACK_OF(CONF_VALUE) *section = NCONF_get_section(cnf, section_name);
		if (section == NULL)
			continue;
		OPENSSL_CSTRING dynamic_path = get_option(section, "dynamic_path");
		if (dynamic_path == NULL)
			continue;

		if (*dynamic_path == '/') {
			puts(dynamic_path);
		} else {
			printf("%s/%s\n", enginesdir, dynamic_path);
		}
	}
}


/**
 * Parse the default OpenSSL configuration file (or the one specified in the
 * OPENSSL_CONF environment variable) and write it back to stdout in
 * a canonical format with all includes and variables expanded.
 */
int main(int argc, char *argv[]) {
	struct option long_options[] = {
		{"config",         no_argument, NULL, CONFIG_FILE},
		{"engines",        no_argument, NULL, ENGINES},
		{"providers",      no_argument, NULL, PROVIDERS},
		{"help",           no_argument, NULL, 'h'},
		{NULL,             0,           NULL, 0},
	};
	int chosen_options[sizeof(long_options) / sizeof(*long_options) - 2] = {0};

	for (size_t idx = 0; idx < sizeof(chosen_options) / sizeof(*chosen_options); idx++) {
		long_options[idx].flag = &chosen_options[idx];
	}

	int c;
	char *configfile cleanup(char) = NULL;
	while (1) {
		c = getopt_long(argc, argv, "", long_options, NULL);
		switch (c) {
			case -1:
				// end of options
				goto options_parsed;
				break;
			case 0:
				/* option detected, we use flags to react, so no need for
				 * custom code here. */
				break;
			case 'h':
				// --help output requested
				fprintf(stderr, "Usage: %s OPTIONS\n\n", argv[0]);
				fputs(
					"OPTIONS are:\n"
					"  --config\n"
					"    Print the path of the OpenSSL configuration file on\n"
					"    this system\n"
					"  --engines\n"
					"    Print the path of any OpenSSL ENGINEs configured in\n"
					"    the configuration file\n"
					"  --providers\n"
					"    Print the path of any OpenSSL providers configured in\n"
					"    the configuration file\n"
					"  --help\n"
					"    Print this help output\n",
					stderr
				);
				return EXIT_FAILURE;
				break;
			case '?':
			case ':':
				// error, getopt(3) already printed a message
				return EXIT_FAILURE;
				break;
			default:
				fprintf(stderr, "getopt(3) returned unexpected character code 0%o\n", c);
				return EXIT_FAILURE;
				break;
		}
	}
options_parsed:

	configfile = CONF_get1_default_config_file();
	if (configfile == NULL) {
		ERR_print_errors_fp(stderr);
		return EXIT_FAILURE;
	}

	CONF *cnf cleanup(CONF) = NCONF_new(NULL);
	if (cnf == NULL) {
		ERR_print_errors_fp(stderr);
		return EXIT_FAILURE;
	}

	long eline = 0;
	if (NCONF_load(cnf, configfile, &eline) == 0) {
		fprintf(stderr, "Error on line %ld of configuration file\n", eline);
		ERR_print_errors_fp(stderr);
		return EXIT_FAILURE;
	}

	bool any_chosen = false;
	for (size_t idx = 0; idx < sizeof(chosen_options) / sizeof(*chosen_options); idx++) {
		if (chosen_options[idx] != 0) {
			any_chosen = true;
		}
		switch (chosen_options[idx]) {
			case CONFIG_FILE:
				puts(configfile);
				break;
			case ENGINES:
				list_engines(cnf);
				break;
			case PROVIDERS:
				list_providers(cnf);
				break;
			case PKCS11_MODULES:
				break;
		}
	}

	if (!any_chosen) {
		fprintf(stderr, "No options were provided, so no output was produced. See --help for instructions.\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
