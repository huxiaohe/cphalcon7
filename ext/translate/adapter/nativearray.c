
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

#include "translate/adapter/nativearray.h"
#include "translate/adapter.h"
#include "translate/adapterinterface.h"
#include "translate/exception.h"

#include "kernel/main.h"
#include "kernel/memory.h"
#include "kernel/exception.h"
#include "kernel/array.h"
#include "kernel/object.h"
#include "kernel/hash.h"
#include "kernel/concat.h"
#include "kernel/fcall.h"
#include "kernel/string.h"

/**
 * Phalcon\Translate\Adapter\NativeArray
 *
 * Allows to define translation lists using PHP arrays
 *
 */
zend_class_entry *phalcon_translate_adapter_nativearray_ce;

static zend_object_handlers phalcon_translate_adapter_nativearray_object_handlers;

static zval* phalcon_translate_adapter_nativearray_read_dimension(zval *object, zval *offset, int type, zval *rv)
{
	zval *translate, *translation;

	if (!is_phalcon_class(Z_OBJCE_P(object))) {
		return zend_get_std_object_handlers()->read_dimension(object, offset, type, rv);
	}

	translate = phalcon_read_property(object, SL("_translate"), PH_NOISY);
	if (phalcon_array_isset_fetch(&translation, translate, offset)) {
		return translation;
	}

	return offset;
}

static int phalcon_translate_adapter_nativearray_has_dimension(zval *object, zval *offset, int check_empty)
{
	zval *translate, *translation;

	if (!is_phalcon_class(Z_OBJCE_P(object))) {
		return zend_get_std_object_handlers()->has_dimension(object, offset, check_empty);
	}

	translate = phalcon_read_property(object, SL("_translate"), PH_NOISY);
	if (!phalcon_array_isset_fetch(&translation, translate, offset)) {
		return 0;
	}

	return (1 == check_empty) ? zend_is_true(translation) : 1;
}

PHP_METHOD(Phalcon_Translate_Adapter_NativeArray, __construct);
PHP_METHOD(Phalcon_Translate_Adapter_NativeArray, query);
PHP_METHOD(Phalcon_Translate_Adapter_NativeArray, exists);

ZEND_BEGIN_ARG_INFO_EX(arginfo_phalcon_translate_adapter_nativearray___construct, 0, 0, 1)
	ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

static const zend_function_entry phalcon_translate_adapter_nativearray_method_entry[] = {
	PHP_ME(Phalcon_Translate_Adapter_NativeArray, __construct, arginfo_phalcon_translate_adapter_nativearray___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(Phalcon_Translate_Adapter_NativeArray, query, arginfo_phalcon_translate_adapterinterface_query, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Translate_Adapter_NativeArray, exists, arginfo_phalcon_translate_adapterinterface_exists, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

/**
 * Phalcon\Translate\Adapter\NativeArray initializer
 */
PHALCON_INIT_CLASS(Phalcon_Translate_Adapter_NativeArray){

	PHALCON_REGISTER_CLASS_EX(Phalcon\\Translate\\Adapter, NativeArray, translate_adapter_nativearray, phalcon_translate_adapter_ce, phalcon_translate_adapter_nativearray_method_entry, 0);

	zend_declare_property_null(phalcon_translate_adapter_nativearray_ce, SL("_translate"), ZEND_ACC_PROTECTED);

	zend_class_implements(phalcon_translate_adapter_nativearray_ce, 1, phalcon_translate_adapterinterface_ce);

	phalcon_translate_adapter_nativearray_object_handlers                = phalcon_translate_adapter_object_handlers;
	phalcon_translate_adapter_nativearray_object_handlers.read_dimension = phalcon_translate_adapter_nativearray_read_dimension;
	phalcon_translate_adapter_nativearray_object_handlers.has_dimension  = phalcon_translate_adapter_nativearray_has_dimension;

	return SUCCESS;
}

/**
 * Phalcon\Translate\Adapter\NativeArray constructor
 *
 * @param array $options
 */
PHP_METHOD(Phalcon_Translate_Adapter_NativeArray, __construct){

	zval *options, *data;

	phalcon_fetch_params(0, 1, 0, &options);
	
	if (Z_TYPE_P(options) != IS_ARRAY) { 
		PHALCON_THROW_EXCEPTION_STRW(phalcon_translate_exception_ce, "Invalid options");
		return;
	}

	if (!phalcon_array_isset_string_fetch(&data, options, SS("content"))) {
		PHALCON_THROW_EXCEPTION_STRW(phalcon_translate_exception_ce, "Translation content was not provided");
		return;
	}

	if (Z_TYPE_P(data) != IS_ARRAY) { 
		PHALCON_THROW_EXCEPTION_STRW(phalcon_translate_exception_ce, "Translation data must be an array");
		return;
	}
	
	phalcon_update_property_this(getThis(), SL("_translate"), data);

	Z_OBJ_HT_P(getThis()) = &phalcon_translate_adapter_nativearray_object_handlers;
}

/**
 * Returns the translation related to the given key
 *
 * @param string $index
 * @param array $placeholders
 * @return string
 */
PHP_METHOD(Phalcon_Translate_Adapter_NativeArray, query){

	zval *index, *placeholders = NULL, *translate, *translation = NULL;
	zval *replaced = NULL, *value;
	zend_string *str_key;
	ulong idx;

	phalcon_fetch_params(0, 1, 1, &index, &placeholders);
	
	if (!placeholders) {
		placeholders = &PHALCON_GLOBAL(z_null);
	}
	
	translate = phalcon_read_property(getThis(), SL("_translate"), PH_NOISY);
	if (!phalcon_array_isset_fetch(&translation, translate, index)) {
		translation = index;
	}

	if (Z_TYPE_P(placeholders) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(placeholders))) {

		ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(placeholders), idx, str_key, value) {
			zval key, key_placeholder, *replaced = NULL;
			if (str_key) {
				ZVAL_STR(&key, str_key);
			} else {
				ZVAL_LONG(&key, idx);
			}

			PHALCON_CONCAT_SVS(&key_placeholder, "%", &key, "%");

			PHALCON_STR_REPLACE(&replaced, &key_placeholder, value, translation);

			translation = replaced;
		} ZEND_HASH_FOREACH_END();
	}

	RETURN_ZVAL(translation, 1, 0);
}

/**
 * Check whether is defined a translation key in the internal array
 *
 * @param string $index
 * @return bool
 */
PHP_METHOD(Phalcon_Translate_Adapter_NativeArray, exists){

	zval **index;

	phalcon_fetch_params(0, 1, 0, &index);
	RETURN_BOOL(phalcon_translate_adapter_nativearray_has_dimension(getThis(), *index, 0));
}
