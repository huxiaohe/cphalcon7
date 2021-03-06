
/*
  +------------------------------------------------------------------------+
  | Phalcon Framework                                                      |
  +------------------------------------------------------------------------+
  | Copyright (c) 2011-2015 Phalcon Team (http://www.phalconphp.com)       |
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

%token_prefix PHQL_
%token_type {phql_parser_token*}
%token_destructor {
	if ($$) {
		if ($$->free_flag) {
			efree($$->token);
		}
		efree($$);
	}
}
%default_type {zval*}
%default_destructor {
	if (status) {
		// TODO:
	}
	zval_ptr_dtor($$);
	efree($$);
}
%extra_argument {phql_parser_status *status}
%name phql_

%right AGAINST .
%left BETWEEN .
%left EQUALS NOTEQUALS LESS GREATER GREATEREQUAL LESSEQUAL .
%left TS_MATCHES TS_OR TS_AND TS_NEGATE TS_CONTAINS_ANOTHER TS_CONTAINS_IN .
%left AND OR .
%left LIKE ILIKE .
%left BITWISE_AND BITWISE_OR BITWISE_XOR .
%left DIVIDE TIMES MOD .
%left PLUS MINUS .
%left IS .
%right IN .
%right NOT BITWISE_NOT .
%left COMMA .

%include {

#include "php_phalcon.h"

#include "mvc/model/query/parser.h"
#include "mvc/model/query/scanner.h"
#include "mvc/model/query/phql.h"
#include "mvc/model/exception.h"

#include "kernel/main.h"
#include "kernel/memory.h"
#include "kernel/fcall.h"
#include "kernel/exception.h"
#include "kernel/array.h"

#include "kernel/framework/orm.h"

#include "interned-strings.h"

static zval *phql_ret_literal_zval(int type, phql_parser_token *T)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init_size(ret, 2);
	add_assoc_long(ret, ISV(type), type);
	if (T) {
		add_assoc_stringl(ret, ISV(value), T->token, T->token_len);
		efree(T->token);
		efree(T);
	}

	return ret;
}

static zval *phql_ret_placeholder_zval(int type, phql_parser_token *T)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init_size(ret, 2);
	add_assoc_long(ret, ISV(type), type);
	add_assoc_stringl(ret, ISV(value), T->token, T->token_len);
	efree(T->token);
	efree(T);

	return ret;
}

static zval *phql_ret_qualified_name(phql_parser_token *A, phql_parser_token *B, phql_parser_token *C)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init(ret);

	add_assoc_long(ret, ISV(type), PHQL_T_QUALIFIED);

	if (A != NULL) {
		add_assoc_stringl(ret, ISV(ns_alias), A->token, A->token_len);
		efree(A->token);
		efree(A);
	}

	if (B != NULL) {
		add_assoc_stringl(ret, ISV(domain), B->token, B->token_len);
		efree(B->token);
		efree(B);
	}

	add_assoc_stringl(ret, ISV(name), C->token, C->token_len);
	efree(C->token);
	efree(C);

	return ret;
}

static zval *phql_ret_raw_qualified_name(phql_parser_token *A, phql_parser_token *B)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init(ret);

	add_assoc_long(ret, ISV(type), PHQL_T_RAW_QUALIFIED);
	if (B != NULL) {
		add_assoc_stringl(ret, ISV(domain), A->token, A->token_len);
		add_assoc_stringl(ret, ISV(name), B->token, B->token_len);
		efree(B->token);
		efree(B);
	} else {
		add_assoc_stringl(ret, ISV(name), A->token, A->token_len);
	}
	efree(A->token);
	efree(A);

	return ret;
}

static zval *phql_ret_select_statement(zval *S, zval *W, zval *O, zval *G, zval *H, zval *L, zval *F)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init(ret);

	add_assoc_long(ret, ISV(type), PHQL_T_SELECT);
	add_assoc_zval(ret, ISV(select), S);
	efree(S);

	if (W != NULL) {
		add_assoc_zval(ret, ISV(where), W);
		efree(W);
	}
	if (O != NULL) {
		add_assoc_zval(ret, ISV(orderBy), O);
		efree(O);
	}
	if (G != NULL) {
		add_assoc_zval(ret, ISV(groupBy), G);
		efree(G);
	}
	if (H != NULL) {
		add_assoc_zval(ret, ISV(having), H);
		efree(H);
	}
	if (L != NULL) {
		add_assoc_zval(ret, ISV(limit), L);
		efree(L);
	}
	if (F != NULL) {
		add_assoc_zval(ret, ISV(forupdate), F);
		efree(F);
	}

	return ret;
}

static zval *phql_ret_select_clause(zval *distinct, zval *columns, zval *tables, zval *join_list)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init_size(ret, 4);

	if (distinct) {
		add_assoc_zval(ret, ISV(distinct), distinct);
		efree(distinct);
	}

	add_assoc_zval(ret, ISV(columns), columns);
	efree(columns);
	add_assoc_zval(ret, ISV(tables), tables);
	efree(tables);
	if (join_list) {
		add_assoc_zval(ret, ISV(joins), join_list);
		efree(join_list);
	}

	return ret;
}

static zval *phql_ret_distinct_all(int distinct)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	ZVAL_LONG(ret, distinct);

	return ret;
}

static zval *phql_ret_distinct(void)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	ZVAL_TRUE(ret);

	return ret;
}

static zval *phql_ret_order_item(zval *column, int sort){

	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init(ret);
	add_assoc_zval(ret, ISV(column), column);
	efree(column);
	if (sort != 0 ) {
		add_assoc_long(ret, ISV(sort), sort);
	}

	return ret;
}

static zval *phql_ret_limit_clause(zval *L, zval *O)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init_size(ret, 2);

	add_assoc_zval(ret, ISV(number), L);
	efree(L);

	if (O != NULL) {
		add_assoc_zval(ret, ISV(offset), O);
		efree(O);
	}

	return ret;
}

static zval *phql_ret_for_update_clause()
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	ZVAL_TRUE(ret);

	return ret;
}

static zval *phql_ret_insert_statement(zval *Q, zval *F, zval *V)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init_size(ret, 4);

	add_assoc_long(ret, ISV(type), PHQL_T_INSERT);
	add_assoc_zval(ret, ISV(qualifiedName), Q);
	efree(Q);
	if (F != NULL) {
		add_assoc_zval(ret, ISV(fields), F);
		efree(F);
	}
	add_assoc_zval(ret, ISV(values), V);
	efree(V);

	return ret;
}

static zval *phql_ret_insert_statement2(zval *ret, zval *F, zval *V)
{
	zval key1, key2, rows, values;

	ZVAL_STR(&key1, IS(rows));

	if (!phalcon_array_isset_fetch(&rows, ret, &key1)) {
		array_init_size(&rows, 1);		

		ZVAL_STR(&key2, IS(values));

		if (phalcon_array_isset_fetch(&values, ret, &key2)) {
			add_next_index_zval(&rows, &values);	
		}
		PHALCON_PTR_DTOR(&key2);
	}

	PHALCON_PTR_DTOR(&key1);
	add_next_index_zval(&rows, V);
	efree(V);

	add_assoc_zval(ret, ISV(rows), &rows);

	return ret;
}

static zval *phql_ret_update_statement(zval *U, zval *W, zval *L)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init(ret);

	add_assoc_long(ret, ISV(type), PHQL_T_UPDATE);
	add_assoc_zval(ret, ISV(update), U);
	efree(U);
	if (W != NULL) {
		add_assoc_zval(ret, ISV(where), W);
		efree(W);
	}
	if (L != NULL) {
		add_assoc_zval(ret, ISV(limit), L);
		efree(L);
	}

	return ret;
}

static zval *phql_ret_update_clause(zval *tables, zval *values)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init_size(ret, 2);
	add_assoc_zval(ret, ISV(tables), tables);
	efree(tables);
	add_assoc_zval(ret, ISV(values), values);
	efree(values);

	return ret;
}

static zval *phql_ret_update_item(zval *column, zval *expr)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init_size(ret, 2);
	add_assoc_zval(ret, ISV(column), column);
	efree(column);
	add_assoc_zval(ret, ISV(expr), expr);
	efree(expr);

	return ret;
}

static zval *phql_ret_delete_statement(zval *D, zval *W, zval *L)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init(ret);

	add_assoc_long(ret, ISV(type), PHQL_T_DELETE);
	add_assoc_zval(ret, ISV(delete), D);
	efree(D);

	if (W != NULL) {
		add_assoc_zval(ret, ISV(where), W);
		efree(W);
	}
	if (L != NULL) {
		add_assoc_zval(ret, ISV(limit), L);
		efree(L);
	}

	return ret;
}

static zval *phql_ret_delete_clause(zval *tables)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init_size(ret, 1);
	add_assoc_zval(ret, ISV(tables), tables);
	efree(tables);

	return ret;
}

static zval *phql_ret_zval_list(zval *list_left, zval *right_list)
{
	zval *ret;
	HashTable *list;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init(ret);

	list = Z_ARRVAL_P(list_left);
	if (zend_hash_index_exists(list, 0)) {
		zval *item;
		ZEND_HASH_FOREACH_VAL(list, item) {
			add_next_index_zval(ret, item);
		} ZEND_HASH_FOREACH_END();
	} else {
		add_next_index_zval(ret, list_left);
	}
	efree(list_left);

	if (right_list) {
		add_next_index_zval(ret, right_list);
		efree(right_list);
	}

	return ret;
}

static zval *phql_ret_column_item(int type, zval *column, phql_parser_token *identifier_column, phql_parser_token *alias)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init_size(ret, 4);
	add_assoc_long(ret, ISV(type), type);
	if (column) {
		add_assoc_zval(ret, ISV(column), column);
		efree(column);
	}
	if (identifier_column) {
		add_assoc_stringl(ret, ISV(column), identifier_column->token, identifier_column->token_len);
		efree(identifier_column->token);
		efree(identifier_column);
	}
	if (alias) {
		add_assoc_stringl(ret, ISV(alias), alias->token, alias->token_len);
		efree(alias->token);
		efree(alias);
	}

	return ret;
}

static zval *phql_ret_assoc_name(zval *qualified_name, phql_parser_token *alias)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init_size(ret, 2);
	add_assoc_zval(ret, ISV(qualifiedName), qualified_name);
	efree(qualified_name);
	if (alias) {
		add_assoc_stringl(ret, ISV(alias), alias->token, alias->token_len);
		efree(alias->token);
		efree(alias);
	}

	return ret;
}

static zval *phql_ret_join_type(int type)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	ZVAL_LONG(ret, type);

	return ret;
}

static zval *phql_ret_join_item(zval *type, zval *qualified, zval *alias, zval *conditions)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init_size(ret, 4);
	add_assoc_zval(ret, ISV(type), type);
	efree(type);

	if (qualified) {
		add_assoc_zval(ret, ISV(qualified), qualified);
		efree(qualified);
	}

	if (alias) {
		add_assoc_zval(ret, ISV(alias), alias);
		efree(alias);
	}

	if (conditions) {
		add_assoc_zval(ret, ISV(conditions), conditions);
		efree(conditions);
	}

	return ret;
}

static zval *phql_ret_expr(int type, zval *left, zval *right)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init_size(ret, 2);
	add_assoc_long(ret, ISV(type), type);
	if (left) {
		add_assoc_zval(ret, ISV(left), left);
		efree(left);
	}
	if (right) {
		add_assoc_zval(ret, ISV(right), right);
		efree(right);
	}

	return ret;
}

static zval *phql_ret_func_call(phql_parser_token *name, zval *arguments, zval *distinct)
{
	zval *ret;

	PHALCON_ALLOC_INIT_ZVAL(ret);
	array_init_size(ret, 4);
	add_assoc_long(ret, ISV(type), PHQL_T_FCALL);
	add_assoc_stringl(ret, ISV(name), name->token, name->token_len);
	efree(name->token);
	efree(name);

	if (arguments) {
		add_assoc_zval(ret, ISV(arguments), arguments);
		efree(arguments);
	}
	
	if (distinct) {
		add_assoc_zval(ret, ISV(distinct), distinct);
		efree(distinct);
	}

	return ret;
}

}

%syntax_error {
	if (status->scanner_state->start_length) {
		{

			char *token_name = NULL;
			int token_found = 0;
			unsigned int token_length;
			const phql_token_names *tokens = phql_tokens;
			uint active_token = status->scanner_state->active_token;
			uint near_length = status->scanner_state->start_length;

			if (active_token) {

				do {
					if (tokens->code == active_token) {
						token_name = tokens->name;
						token_length = tokens->length;
						token_found = 1;
						break;
					}
					++tokens;
				} while (tokens[0].code != 0);

			}

			if (!token_name) {
				token_length = strlen("UNKNOWN");
				token_name = estrndup("UNKNOWN", token_length);
				token_found = 0;
			}

			status->syntax_error_len = 96 + status->token->len + token_length + near_length + status->phql_length;;
			status->syntax_error = emalloc(sizeof(char) * status->syntax_error_len);

			if (near_length > 0) {
				if (status->token->value) {
					snprintf(status->syntax_error, status->syntax_error_len, "Syntax error, unexpected token %s(%s), near to '%s', when parsing: %s (%d)", token_name, status->token->value, status->scanner_state->start, status->phql, status->phql_length);
				} else {
					snprintf(status->syntax_error, status->syntax_error_len, "Syntax error, unexpected token %s, near to '%s', when parsing: %s (%d)", token_name, status->scanner_state->start, status->phql, status->phql_length);
				}
			} else {
				if (active_token != PHQL_T_IGNORE) {
					if (status->token->value) {
						snprintf(status->syntax_error, status->syntax_error_len, "Syntax error, unexpected token %s(%s), at the end of query, when parsing: %s (%d)", token_name, status->token->value, status->phql, status->phql_length);
					} else {
						snprintf(status->syntax_error, status->syntax_error_len, "Syntax error, unexpected token %s, at the end of query, when parsing: %s (%d)", token_name, status->phql, status->phql_length);
					}
				} else {
					snprintf(status->syntax_error, status->syntax_error_len, "Syntax error, unexpected EOF, at the end of query");
				}
				status->syntax_error[status->syntax_error_len - 1] = '\0';
			}

			if (!token_found) {
				if (token_name) {
					efree(token_name);
				}
			}
		}
	} else {
		status->syntax_error_len = strlen("Syntax error, unexpected EOF");
		status->syntax_error = estrndup("Syntax error, unexpected EOF", status->syntax_error_len);
	}

	status->status = PHQL_PARSING_FAILED;
}

program ::= query_language(Q) . {
	status->ret = Q;
}

query_language(R) ::= select_statement(S) . {
	R = S;
}

query_language(R) ::= insert_statement(I) . {
	R = I;
}

query_language(R) ::= update_statement(U) . {
	R = U;
}

query_language(R) ::= delete_statement(D) . {
	R = D;
}

select_statement(R) ::= select_clause(S) where_clause(W) group_clause(G) having_clause(H) order_clause(O) select_limit_clause(L) for_update_clause(F) . {
	R = phql_ret_select_statement(S, W, O, G, H, L, F);
}

select_clause(R) ::= SELECT distinct_all(D) column_list(C) FROM associated_name_list(A) join_list_or_null(J) . {
	R = phql_ret_select_clause(D, C, A, J);
}

distinct_all(R) ::= DISTINCT . {
	R = phql_ret_distinct_all(1);
}

distinct_all(R) ::= ALL . {
	R = phql_ret_distinct_all(0);
}

distinct_all(R) ::= . {
	R = NULL;
}

column_list(R) ::= column_list(L) COMMA column_item(C) . {
	R = phql_ret_zval_list(L, C);
}

column_list(R) ::= column_item(I) . {
	R = phql_ret_zval_list(I, NULL);
}

column_item(R) ::= TIMES . {
	R = phql_ret_column_item(PHQL_T_STARALL, NULL, NULL, NULL);
}

column_item(R) ::= IDENTIFIER(I) DOT TIMES . {
	R = phql_ret_column_item(PHQL_T_DOMAINALL, NULL, I, NULL);
}

column_item(R) ::= expr(E) AS IDENTIFIER(I) . {
	R = phql_ret_column_item(PHQL_T_EXPR, E, NULL, I);
}

column_item(R) ::= expr(E) IDENTIFIER(I) . {
	R = phql_ret_column_item(PHQL_T_EXPR, E, NULL, I);
}

column_item(R) ::= expr(E) . {
	R = phql_ret_column_item(PHQL_T_EXPR, E, NULL, NULL);
}

associated_name_list(R) ::= associated_name_list(L) COMMA associated_name(A) . {
	R = phql_ret_zval_list(L, A);
}

associated_name_list(R) ::= associated_name(L) . {
	R = L;
}

join_list_or_null(R) ::= join_list(L) . {
	R = L;
}

join_list_or_null(R) ::= . {
	R = NULL;
}

join_list(R) ::= join_list(L) join_item(I) . {
	R = phql_ret_zval_list(L, I);
}

join_list(R) ::= join_item(I) . {
	R = I;
}

join_item(R) ::= join_clause(C) . {
	R = C;
}

/** Join + conditions + alias */
join_clause(R) ::= join_type(T) aliased_or_qualified_name(Q) join_associated_name(A) join_conditions(C) . {
	R = phql_ret_join_item(T, Q, A, C);
}

join_associated_name(R) ::= AS IDENTIFIER(I) . {
	R = phql_ret_qualified_name(NULL, NULL, I);
}

join_associated_name(R) ::= IDENTIFIER(I) . {
	R = phql_ret_qualified_name(NULL, NULL, I);
}

join_associated_name(R) ::= . {
	R = NULL;
}

join_type(R) ::= INNER JOIN . {
	R = phql_ret_join_type(PHQL_T_INNERJOIN);
}

join_type(R) ::= CROSS JOIN . {
	R = phql_ret_join_type(PHQL_T_CROSSJOIN);
}

join_type(R) ::= LEFT OUTER JOIN . {
	R = phql_ret_join_type(PHQL_T_LEFTJOIN);
}

join_type(R) ::= LEFT JOIN . {
	R = phql_ret_join_type(PHQL_T_LEFTJOIN);
}

join_type(R) ::= RIGHT OUTER JOIN . {
	R = phql_ret_join_type(PHQL_T_RIGHTJOIN);
}

join_type(R) ::= RIGHT JOIN . {
	R = phql_ret_join_type(PHQL_T_RIGHTJOIN);
}

join_type(R) ::= FULL OUTER JOIN . {
	R = phql_ret_join_type(PHQL_T_FULLJOIN);
}

join_type(R) ::= FULL JOIN . {
	R = phql_ret_join_type(PHQL_T_FULLJOIN);
}

join_type(R) ::= JOIN . {
	R = phql_ret_join_type(PHQL_T_INNERJOIN);
}

join_conditions(R) ::= ON expr(E) . {
	R = E;
}

join_conditions(R) ::= . {
	R = NULL;
}

/* Insert */
insert_statement(R) ::= insert_statement(Q) COMMA PARENTHESES_OPEN values_list(V) PARENTHESES_CLOSE . {
	R = phql_ret_insert_statement2(Q, NULL, V);
}

insert_statement(R) ::= INSERT INTO aliased_or_qualified_name(Q) VALUES PARENTHESES_OPEN values_list(V) PARENTHESES_CLOSE . {
	R = phql_ret_insert_statement(Q, NULL, V);
}

insert_statement(R) ::= INSERT INTO aliased_or_qualified_name(Q) PARENTHESES_OPEN field_list(F) PARENTHESES_CLOSE VALUES PARENTHESES_OPEN values_list(V) PARENTHESES_CLOSE . {
	R = phql_ret_insert_statement(Q, F, V);
}

values_list(R) ::= values_list(L) COMMA value_item(I) . {
	R = phql_ret_zval_list(L, I);
}

values_list(R) ::= value_item(I) . {
	R = phql_ret_zval_list(I, NULL);
}

value_item(R) ::= expr(E) . {
	R = E;
}

field_list(R) ::= field_list(L) COMMA field_item(I) . {
	R = phql_ret_zval_list(L, I);
}

field_list(R) ::= field_item(I) . {
	R = phql_ret_zval_list(I, NULL);
}

field_item(R) ::= IDENTIFIER(I) . {
	R = phql_ret_qualified_name(NULL, NULL, I);
}

/* Update */

update_statement(R) ::= update_clause(U) where_clause(W) limit_clause(L) . {
	R = phql_ret_update_statement(U, W, L);
}

update_clause(R) ::= UPDATE associated_name(A) SET update_item_list(U) . {
	R = phql_ret_update_clause(A, U);
}

update_item_list(R) ::= update_item_list(L) COMMA update_item(I) . {
	R = phql_ret_zval_list(L, I);
}

update_item_list(R) ::= update_item(I) . {
	R = I;
}

update_item(R) ::= qualified_name(Q) EQUALS new_value(N) . {
	R = phql_ret_update_item(Q, N);
}

new_value(R) ::= expr(E) . {
	R = E;
}

/* Delete */

delete_statement(R) ::= delete_clause(D) where_clause(W) limit_clause(L) . {
	R = phql_ret_delete_statement(D, W, L);
}

delete_clause(R) ::= DELETE FROM associated_name(A) . {
	R = phql_ret_delete_clause(A);
}

associated_name(R) ::= aliased_or_qualified_name(Q) AS IDENTIFIER(I) . {
	R = phql_ret_assoc_name(Q, I);
}

associated_name(R) ::= aliased_or_qualified_name(Q) IDENTIFIER(I) . {
	R = phql_ret_assoc_name(Q, I);
}

associated_name(R) ::= aliased_or_qualified_name(Q) . {
	R = phql_ret_assoc_name(Q, NULL);
}

aliased_or_qualified_name(R) ::= qualified_name(Q) . {
	R = Q;
}

where_clause(R) ::= WHERE expr(E) . {
	R = E;
}

where_clause(R) ::= . {
	R = NULL;
}

order_clause(R) ::= ORDER BY order_list(O) . {
	R = O;
}

order_clause(R) ::= . {
	R = NULL;
}

order_list(R) ::= order_list(L) COMMA order_item(I) . {
	R = phql_ret_zval_list(L, I);
}

order_list(R) ::= order_item(I) . {
	R = I;
}

order_item(R) ::= expr(O) . {
	R = phql_ret_order_item(O, 0);
}

order_item(R) ::= expr(O) ASC . {
	R = phql_ret_order_item(O, PHQL_T_ASC);
}

order_item(R) ::= expr(O) DESC . {
	R = phql_ret_order_item(O, PHQL_T_DESC);
}

group_clause(R) ::= GROUP BY group_list(G) . {
	R = G;
}

group_clause(R) ::= . {
	R = NULL;
}

group_list(R) ::= group_list(L) COMMA group_item(I) . {
	R = phql_ret_zval_list(L, I);
}

group_list(R) ::= group_item(I) . {
	R = I;
}

group_item(R) ::= expr(E) . {
	R = E;
}

having_clause(R) ::= HAVING expr(E) . {
	R = E;
}

having_clause(R) ::= . {
	R = NULL;
}

for_update_clause(R) ::= FOR UPDATE . {
	R = phql_ret_for_update_clause();
}

for_update_clause(R) ::= . {
	R = NULL;
}

select_limit_clause(R) ::= LIMIT integer_or_placeholder(I) . {
	R = phql_ret_limit_clause(I, NULL);
}

select_limit_clause(R) ::= LIMIT integer_or_placeholder(O) COMMA integer_or_placeholder(I). {
	R = phql_ret_limit_clause(I, O);
}

select_limit_clause(R) ::= LIMIT integer_or_placeholder(I) OFFSET integer_or_placeholder(O). {
	R = phql_ret_limit_clause(I, O);
}

select_limit_clause(R) ::= . {
	R = NULL;
}

limit_clause(R) ::= LIMIT integer_or_placeholder(I) . {
	R = phql_ret_limit_clause(I, NULL);
}

limit_clause(R) ::= . {
	R = NULL;
}

integer_or_placeholder(R) ::= INTEGER(I) . {
	R = phql_ret_literal_zval(PHQL_T_INTEGER, I);
}

integer_or_placeholder(R) ::= HINTEGER(I) . {
	R = phql_ret_literal_zval(PHQL_T_HINTEGER, I);
}

integer_or_placeholder(R) ::= NPLACEHOLDER(P) . {
	R = phql_ret_placeholder_zval(PHQL_T_NPLACEHOLDER, P);
}

integer_or_placeholder(R) ::= SPLACEHOLDER(P) . {
	R = phql_ret_placeholder_zval(PHQL_T_SPLACEHOLDER, P);
}

expr(R) ::= MINUS expr(E) . {
	R = phql_ret_expr(PHQL_T_MINUS, NULL, E);
}

expr(R) ::= expr(O1) MINUS expr(O2) . {
	R = phql_ret_expr(PHQL_T_SUB, O1, O2);
}

expr(R) ::= expr(O1) PLUS expr(O2) . {
	R = phql_ret_expr(PHQL_T_ADD, O1, O2);
}

expr(R) ::= expr(O1) TIMES expr(O2) . {
	R = phql_ret_expr(PHQL_T_MUL, O1, O2);
}

expr(R) ::= expr(O1) DIVIDE expr(O2) . {
	R = phql_ret_expr(PHQL_T_DIV, O1, O2);
}

expr(R) ::= expr(O1) MOD expr(O2) . {
	R = phql_ret_expr(PHQL_T_MOD, O1, O2);
}

expr(R) ::= expr(O1) AND expr(O2) . {
	R = phql_ret_expr(PHQL_T_AND, O1, O2);
}

expr(R) ::= expr(O1) OR expr(O2) . {
	R = phql_ret_expr(PHQL_T_OR, O1, O2);
}

expr(R) ::= expr(O1) BITWISE_AND expr(O2) . {
	R = phql_ret_expr(PHQL_T_BITWISE_AND, O1, O2);
}

expr(R) ::= expr(O1) BITWISE_OR expr(O2) . {
	R = phql_ret_expr(PHQL_T_BITWISE_OR, O1, O2);
}

expr(R) ::= expr(O1) BITWISE_XOR expr(O2) . {
	R = phql_ret_expr(PHQL_T_BITWISE_XOR, O1, O2);
}

expr(R) ::= expr(O1) EQUALS expr(O2) . {
	R = phql_ret_expr(PHQL_T_EQUALS, O1, O2);
}

expr(R) ::= expr(O1) NOTEQUALS expr(O2) . {
	R = phql_ret_expr(PHQL_T_NOTEQUALS, O1, O2);
}

expr(R) ::= expr(O1) LESS expr(O2) . {
	R = phql_ret_expr(PHQL_T_LESS, O1, O2);
}

expr(R) ::= expr(O1) GREATER expr(O2) . {
	R = phql_ret_expr(PHQL_T_GREATER, O1, O2);
}

expr(R) ::= expr(O1) GREATEREQUAL expr(O2) . {
	R = phql_ret_expr(PHQL_T_GREATEREQUAL, O1, O2);
}

expr(R) ::= expr(O1) TS_MATCHES expr(O2) . {
	R = phql_ret_expr(PHQL_T_TS_MATCHES, O1, O2);
}

expr(R) ::= expr(O1) TS_OR expr(O2) . {
	R = phql_ret_expr(PHQL_T_TS_OR, O1, O2);
}

expr(R) ::= expr(O1) TS_AND expr(O2) . {
	R = phql_ret_expr(PHQL_T_TS_AND, O1, O2);
}

expr(R) ::= expr(O1) TS_NEGATE expr(O2) . {
	R = phql_ret_expr(PHQL_T_TS_NEGATE, O1, O2);
}

expr(R) ::= expr(O1) TS_CONTAINS_ANOTHER expr(O2) . {
	R = phql_ret_expr(PHQL_T_TS_CONTAINS_ANOTHER, O1, O2);
}

expr(R) ::= expr(O1) TS_CONTAINS_IN expr(O2) . {
	R = phql_ret_expr(PHQL_T_TS_CONTAINS_IN, O1, O2);
}

expr(R) ::= expr(O1) LESSEQUAL expr(O2) . {
	R = phql_ret_expr(PHQL_T_LESSEQUAL, O1, O2);
}

expr(R) ::= expr(O1) LIKE expr(O2) . {
	R = phql_ret_expr(PHQL_T_LIKE, O1, O2);
}

expr(R) ::= expr(O1) NOT LIKE expr(O2) . {
	R = phql_ret_expr(PHQL_T_NLIKE, O1, O2);
}

expr(R) ::= expr(O1) ILIKE expr(O2) . {
	R = phql_ret_expr(PHQL_T_ILIKE, O1, O2);
}

expr(R) ::= expr(O1) NOT ILIKE expr(O2) . {
	R = phql_ret_expr(PHQL_T_NILIKE, O1, O2);
}

expr(R) ::= expr(E) IN PARENTHESES_OPEN argument_list(L) PARENTHESES_CLOSE . {
	R = phql_ret_expr(PHQL_T_IN, E, L);
}

expr(R) ::= expr(E) NOT IN PARENTHESES_OPEN argument_list(L) PARENTHESES_CLOSE . {
	R = phql_ret_expr(PHQL_T_NOTIN, E, L);
}

expr(R) ::= PARENTHESES_OPEN select_statement(S) PARENTHESES_CLOSE . {
	R = phql_ret_expr(PHQL_T_SUBQUERY, S, NULL);
}

expr(R) ::= expr(E) IN PARENTHESES_OPEN select_statement(S) PARENTHESES_CLOSE . {
	R = phql_ret_expr(PHQL_T_IN, E, S);
}

expr(R) ::= expr(E) NOT IN PARENTHESES_OPEN select_statement(S) PARENTHESES_CLOSE . {
	R = phql_ret_expr(PHQL_T_NOTIN, E, S);
}

expr(R) ::= EXISTS PARENTHESES_OPEN select_statement(S) PARENTHESES_CLOSE . {
	R = phql_ret_expr(PHQL_T_EXISTS, NULL, S);
}

expr(R) ::= expr(O1) AGAINST expr(O2) . {
	R = phql_ret_expr(PHQL_T_AGAINST, O1, O2);
}

expr(R) ::= CAST PARENTHESES_OPEN expr(E) AS IDENTIFIER(I) PARENTHESES_CLOSE . {
	R = phql_ret_expr(PHQL_T_CAST, E, phql_ret_raw_qualified_name(I, NULL));
}

expr(R) ::= CONVERT PARENTHESES_OPEN expr(E) USING IDENTIFIER(I) PARENTHESES_CLOSE . {
	R = phql_ret_expr(PHQL_T_CONVERT, E, phql_ret_raw_qualified_name(I, NULL));
}

expr(R) ::= CASE expr(E) when_clauses(W) END . {
	R = phql_ret_expr(PHQL_T_CASE, E, W);
}

when_clauses(R) ::= when_clauses(L) when_clause(W) . {
	R = phql_ret_zval_list(L, W);
}

when_clauses(R) ::= when_clause(W) . {
	R = phql_ret_zval_list(W, NULL);
}

when_clause(R) ::= WHEN expr(E) THEN expr(T) . {
	R = phql_ret_expr(PHQL_T_WHEN, E, T);
}

when_clause(R) ::= ELSE expr(E) . {
	R = phql_ret_expr(PHQL_T_ELSE, E, NULL);
}

expr(R) ::= function_call(F) . {
	R = F;
}

function_call(R) ::= IDENTIFIER(I) PARENTHESES_OPEN distinct_or_null(D) argument_list_or_null(L) PARENTHESES_CLOSE . {
	R = phql_ret_func_call(I, L, D);
}

distinct_or_null(R) ::= DISTINCT . {
	R = phql_ret_distinct();
}

distinct_or_null(R) ::=  . {
	R = NULL;
}

argument_list_or_null(R) ::= argument_list(L) . {
	R = L;
}

argument_list_or_null(R) ::= . {
	R = NULL;
}

argument_list(R) ::= argument_list(L) COMMA argument_item(I) . {
	R = phql_ret_zval_list(L, I);
}

argument_list(R) ::= argument_item(I) . {
	R = phql_ret_zval_list(I, NULL);
}

argument_item(R) ::= TIMES . {
	R = phql_ret_column_item(PHQL_T_STARALL, NULL, NULL, NULL);
}

argument_item(R) ::= expr(E) . {
	R = E;
}

expr(R) ::= expr(E) IS NULL . {
	R = phql_ret_expr(PHQL_T_ISNULL, E, NULL);
}

expr(R) ::= expr(E) IS NOT NULL . {
	R = phql_ret_expr(PHQL_T_ISNOTNULL, E, NULL);
}

expr(R) ::= expr(E) BETWEEN expr(L) . {
	R = phql_ret_expr(PHQL_T_BETWEEN, E, L);
}

expr(R) ::= NOT expr(E) . {
	R = phql_ret_expr(PHQL_T_NOT, NULL, E);
}

expr(R) ::= BITWISE_NOT expr(E) . {
	R = phql_ret_expr(PHQL_T_BITWISE_NOT, NULL, E);
}

expr(R) ::= PARENTHESES_OPEN expr(E) PARENTHESES_CLOSE . {
	R = phql_ret_expr(PHQL_T_ENCLOSED, E, NULL);
}

expr(R) ::= qualified_name(Q) . {
	R = Q;
}

expr(R) ::= INTEGER(I) . {
	R = phql_ret_literal_zval(PHQL_T_INTEGER, I);
}

expr(R) ::= HINTEGER(I) . {
	R = phql_ret_literal_zval(PHQL_T_HINTEGER, I);
}

expr(R) ::= STRING(S) . {
	R = phql_ret_literal_zval(PHQL_T_STRING, S);
}

expr(R) ::= DOUBLE(D) . {
	R = phql_ret_literal_zval(PHQL_T_DOUBLE, D);
}

expr(R) ::= NULL . {
	R = phql_ret_literal_zval(PHQL_T_NULL, NULL);
}

expr(R) ::= TRUE . {
	R = phql_ret_literal_zval(PHQL_T_TRUE, NULL);
}

expr(R) ::= FALSE . {
	R = phql_ret_literal_zval(PHQL_T_FALSE, NULL);
}

/* ?0 */
expr(R) ::= NPLACEHOLDER(P) . {
	R = phql_ret_placeholder_zval(PHQL_T_NPLACEHOLDER, P);
}

/* :placeholder: */
expr(R) ::= SPLACEHOLDER(P) . {
	R = phql_ret_placeholder_zval(PHQL_T_SPLACEHOLDER, P);
}

/* {placeholder} */
expr(R) ::= BPLACEHOLDER(P) . {
	R = phql_ret_placeholder_zval(PHQL_T_BPLACEHOLDER, P);
}

qualified_name(R) ::= IDENTIFIER(A) COLON IDENTIFIER(B) DOT IDENTIFIER(C) . {
	R = phql_ret_qualified_name(A, B, C);
}

qualified_name(R) ::= IDENTIFIER(A) COLON IDENTIFIER(B) . {
	R = phql_ret_qualified_name(A, NULL, B);
}

qualified_name(R) ::= IDENTIFIER(A) DOT IDENTIFIER(B) . {
	R = phql_ret_qualified_name(NULL, A, B);
}

qualified_name(R) ::= IDENTIFIER(A) . {
	R = phql_ret_qualified_name(NULL, NULL, A);
}
