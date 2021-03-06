
/*
 +------------------------------------------------------------------------+
 | Phalcon Framework                                                      |
 +------------------------------------------------------------------------+
 | Copyright (c) 2011-2014 Phalcon Team (http://www.phalconphp.com)       |
 +------------------------------------------------------------------------+
 | This source file is subject to the New BSD License that is bundled     |
 | with this package in the file docs/LICENSE.txt.                        |
 |                                                                        |
 | If you did not receive a copy of the license and are unable to         |
 | obtain it through the world-wide-web, please send an email             |
 | to license@phalconphp.com so we can send you a copy immediately.       |
 +------------------------------------------------------------------------+
 | Authors: Andres Gutierrez <andres@phalconphp.com>                      |
 |          Eduar Carvajal <eduar@phalconphp.com>                         |
 +------------------------------------------------------------------------+
*/

#ifndef PHALCON_MVC_MODEL_QUERY_PHQL_H
#define PHALCON_MVC_MODEL_QUERY_PHQL_H

#include "php_phalcon.h"
#include "mvc/model/query/scanner.h"

typedef struct _phql_parser_token {
	char *token;
	int opcode;
	unsigned int token_len;
	int free_flag;
} phql_parser_token;

typedef struct _phql_parser_status {
	zval *ret;
	char* phql;
	unsigned int phql_length;
	int status;
	phql_scanner_state *scanner_state;
	phql_scanner_token *token;
	char *syntax_error;
	uint32_t syntax_error_len;
	zend_bool enable_literals;
} phql_parser_status;

#define PHQL_PARSING_OK 1
#define PHQL_PARSING_FAILED 0

int phql_parse_phql(zval *result, zval *phql) PHALCON_ATTR_NONNULL;
int phql_internal_parse_phql(zval *result, char *phql, unsigned int phql_length, zval *error_msg) PHALCON_ATTR_NONNULL2(1, 4);

#endif /* PHALCON_MVC_MODEL_QUERY_PHQL_H */
