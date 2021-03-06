
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

#include "cache/backend/redis.h"
#include "cache/backend.h"
#include "cache/backendinterface.h"
#include "cache/exception.h"

#include "kernel/main.h"
#include "kernel/memory.h"
#include "kernel/array.h"
#include "kernel/fcall.h"
#include "kernel/object.h"
#include "kernel/exception.h"
#include "kernel/concat.h"
#include "kernel/operators.h"
#include "kernel/hash.h"
#include "kernel/string.h"

#include "internal/arginfo.h"

/**
 * Phalcon\Cache\Backend\Redis
 *
 * Allows to cache output fragments, PHP data or raw data to a redis backend
 *
 * This adapter uses the special redisd key "_PHCR" to store all the keys internally used by the adapter
 *
 *<code>
 *
 * // Cache data for 2 days
 * $frontCache = new Phalcon\Cache\Frontend\Data(array(
 *    "lifetime" => 172800
 * ));
 *
 * //Create the Cache setting redisd connection options
 * $cache = new Phalcon\Cache\Backend\Redis($frontCache, array(
 *		'host' => 'localhost',
 *		'port' => 6379,
 *		'auth' => 'foobared',
 *  	'persistent' => false
 * ));
 *
 * //Cache arbitrary data
 * $cache->save('my-data', array(1, 2, 3, 4, 5));
 *
 * //Get data
 * $data = $cache->get('my-data');
 *
 *</code>
 */
zend_class_entry *phalcon_cache_backend_redis_ce;

PHP_METHOD(Phalcon_Cache_Backend_Redis, __construct);
PHP_METHOD(Phalcon_Cache_Backend_Redis, _connect);
PHP_METHOD(Phalcon_Cache_Backend_Redis, get);
PHP_METHOD(Phalcon_Cache_Backend_Redis, save);
PHP_METHOD(Phalcon_Cache_Backend_Redis, delete);
PHP_METHOD(Phalcon_Cache_Backend_Redis, queryKeys);
PHP_METHOD(Phalcon_Cache_Backend_Redis, exists);
PHP_METHOD(Phalcon_Cache_Backend_Redis, increment);
PHP_METHOD(Phalcon_Cache_Backend_Redis, decrement);
PHP_METHOD(Phalcon_Cache_Backend_Redis, flush);
PHP_METHOD(Phalcon_Cache_Backend_Redis, getTrackingKey);
PHP_METHOD(Phalcon_Cache_Backend_Redis, setTrackingKey);

ZEND_BEGIN_ARG_INFO_EX(arginfo_phalcon_cache_backend_redis___construct, 0, 0, 1)
	ZEND_ARG_INFO(0, frontend)
	ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_phalcon_cache_backend_redis_settrackingkey, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

static const zend_function_entry phalcon_cache_backend_redis_method_entry[] = {
	PHP_ME(Phalcon_Cache_Backend_Redis, __construct, arginfo_phalcon_cache_backend_redis___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(Phalcon_Cache_Backend_Redis, _connect, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(Phalcon_Cache_Backend_Redis, get, arginfo_phalcon_cache_backendinterface_get, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Cache_Backend_Redis, save, arginfo_phalcon_cache_backendinterface_save, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Cache_Backend_Redis, delete, arginfo_phalcon_cache_backendinterface_delete, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Cache_Backend_Redis, queryKeys, arginfo_phalcon_cache_backendinterface_querykeys, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Cache_Backend_Redis, exists, arginfo_phalcon_cache_backendinterface_exists, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Cache_Backend_Redis, increment, arginfo_phalcon_cache_backendinterface_increment, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Cache_Backend_Redis, decrement, arginfo_phalcon_cache_backendinterface_decrement, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Cache_Backend_Redis, flush, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Cache_Backend_Redis, getTrackingKey, arginfo_empty, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Cache_Backend_Redis, setTrackingKey, arginfo_phalcon_cache_backend_redis_settrackingkey, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

/**
 * Phalcon\Cache\Backend\Redis initializer
 */
PHALCON_INIT_CLASS(Phalcon_Cache_Backend_Redis)
{
	PHALCON_REGISTER_CLASS_EX(Phalcon\\Cache\\Backend, Redis, cache_backend_redis, phalcon_cache_backend_ce, phalcon_cache_backend_redis_method_entry, 0);

	zend_declare_property_null(phalcon_cache_backend_redis_ce, SL("_redis"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(phalcon_cache_backend_redis_ce, SL("_auth"), ZEND_ACC_PROTECTED);

	zend_class_implements(phalcon_cache_backend_redis_ce, 1, phalcon_cache_backendinterface_ce);

	return SUCCESS;
}

/**
 * Phalcon\Cache\Backend\Redis constructor
 *
 * @param Phalcon\Cache\FrontendInterface $frontend
 * @param array $options
 */
PHP_METHOD(Phalcon_Cache_Backend_Redis, __construct){

	zval *frontend, *_options = NULL, options = {};

	phalcon_fetch_params(0, 1, 1, &frontend, &_options);

	if (!_options) {
		array_init_size(&options, 4);
	} else {
		if (Z_TYPE_P(_options) != IS_ARRAY) {
			array_init_size(&options, 4);
		} else {
			PHALCON_CPY_WRT(&options, _options);
		}
	}

	if (!phalcon_array_isset_str(&options, SL("host"))) {
		phalcon_array_update_str_str(&options, SL("host"), SL("127.0.0.1"), PH_COPY);
	}

	if (!phalcon_array_isset_str(&options, SL("port"))) {
		phalcon_array_update_str_long(&options, SL("port"), 6379, 0);
	}

	if (!phalcon_array_isset_str(&options, SL("persistent"))) {
		phalcon_array_update_str_bool(&options, SL("persistent"), 0, 0);
	}

	if (!phalcon_array_isset_str(&options, SL("statsKey"))) {
		phalcon_array_update_str_str(&options, SL("statsKey"), SL("_PHCR"), PH_COPY);
	}

	PHALCON_CALL_PARENTW(NULL, phalcon_cache_backend_redis_ce, getThis(), "__construct", frontend, &options);
}

/**
 * Create internal connection to redisd
 */
PHP_METHOD(Phalcon_Cache_Backend_Redis, _connect)
{
	zval options = {}, redis = {}, host = {}, port = {}, persistent = {}, success = {}, auth = {};
	zend_class_entry *ce0;

	phalcon_return_property(&options, getThis(), SL("_options"));
	ce0 = phalcon_fetch_str_class(SL("Redis"), ZEND_FETCH_CLASS_AUTO);

	object_init_ex(&redis, ce0);
	if (phalcon_has_constructor(&redis)) {
		PHALCON_CALL_METHODW(NULL, &redis, "__construct");
	}
	
	if (
		   !phalcon_array_isset_fetch_str(&host, &options, SL("host"))
		|| !phalcon_array_isset_fetch_str(&port, &options, SL("port"))
		|| !phalcon_array_isset_fetch_str(&persistent, &options, SL("persistent"))
	) {
		PHALCON_THROW_EXCEPTION_STRW(phalcon_cache_exception_ce, "Unexpected inconsistency in options");
		return;
	}

	if (zend_is_true(&persistent)) {
		PHALCON_CALL_METHODW(&success, &redis, "pconnect", &host, &port);
	} else {
		PHALCON_CALL_METHODW(&success, &redis, "connect", &host, &port);
	}
	
	if (!zend_is_true(&success)) {
		PHALCON_THROW_EXCEPTION_STRW(phalcon_cache_exception_ce, "Cannot connect to Redisd server");
		return;
	}
	
	if (phalcon_array_isset_fetch_str(&auth, &options, SL("auth"))) {
		PHALCON_CALL_METHODW(&success, &redis, "auth", &auth);
		if (!zend_is_true(&success)) {
			PHALCON_THROW_EXCEPTION_STRW(phalcon_cache_exception_ce, "Redisd server is authentication failed");
			return;
		}
	}

	phalcon_update_property_this(getThis(), SL("_redis"), &redis);
	RETURN_CTORW(&redis);
}

/**
 * Returns a cached content
 *
 * @param int|string $keyName
 * @param   long $lifetime
 * @return  mixed
 */
PHP_METHOD(Phalcon_Cache_Backend_Redis, get){

	zval *key_name, *lifetime = NULL, redis = {}, frontend = {}, prefix = {}, prefixed_key = {}, cached_content = {};

	phalcon_fetch_params(0, 1, 1, &key_name, &lifetime);

	phalcon_return_property(&redis, getThis(), SL("_redis"));
	if (Z_TYPE(redis) != IS_OBJECT) {
		PHALCON_CALL_METHODW(&redis, getThis(), "_connect");
	}

	phalcon_return_property(&frontend, getThis(), SL("_frontend"));
	phalcon_return_property(&prefix, getThis(), SL("_prefix"));

	PHALCON_CONCAT_SVV(&prefixed_key, "_PHCR", &prefix, key_name);
	phalcon_update_property_this(getThis(), SL("_lastKey"), &prefixed_key);
	
	PHALCON_CALL_METHODW(&cached_content, &redis, "get", &prefixed_key);
	if (PHALCON_IS_FALSE(&cached_content)) {
		RETURN_NULL();
	}

	if (phalcon_is_numeric(&cached_content)) {
		RETURN_CTORW(&cached_content);
	}
	
	PHALCON_RETURN_CALL_METHODW(&frontend, "afterretrieve", &cached_content);
}

/**
 * Stores cached content into the Redisd backend and stops the frontend
 *
 * @param int|string $keyName
 * @param string $content
 * @param long $lifetime
 * @param boolean $stopBuffer
 */
PHP_METHOD(Phalcon_Cache_Backend_Redis, save){

	zval *key_name = NULL, *content = NULL, *lifetime = NULL, *stop_buffer = NULL, last_key = {}, prefix = {}, cached_content = {}, prepared_content = {}, success = {};
	zval ttl = {}, is_buffering = {}, prefixed_key = {}, frontend = {}, redis = {}, options = {}, special_key = {};

	phalcon_fetch_params(0, 0, 4, &key_name, &content, &lifetime, &stop_buffer);
	
	if (!key_name || Z_TYPE_P(key_name) == IS_NULL) {
		phalcon_return_property(&last_key, getThis(), SL("_lastKey"));
		ZVAL_STRINGL(&prefixed_key, Z_STRVAL(last_key)+5, Z_STRLEN(last_key)-5);
	} else {
		phalcon_return_property(&prefix, getThis(), SL("_prefix"));

		PHALCON_CONCAT_VV(&prefixed_key, &prefix, key_name);
		PHALCON_CONCAT_SV(&last_key, "_PHCR", &prefixed_key);
	}

	if (!zend_is_true(&last_key)) {
		PHALCON_THROW_EXCEPTION_STRW(phalcon_cache_exception_ce, "The cache must be started first");
		return;
	}
	
	/** 
	 * Check if a connection is created or make a new one
	 */
	phalcon_return_property(&redis, getThis(), SL("_redis"));
	if (Z_TYPE(redis) != IS_OBJECT) {
		PHALCON_CALL_METHODW(&redis, getThis(), "_connect");
	}

	phalcon_return_property(&frontend, getThis(), SL("_frontend"));
	if (!content || Z_TYPE_P(content) == IS_NULL) {
		PHALCON_CALL_METHODW(&cached_content, &frontend, "getcontent");
	} else {
		PHALCON_CPY_WRT(&cached_content, content);
	}
	
	/** 
	 * Prepare the content in the frontend
	 */
	PHALCON_CALL_METHODW(&prepared_content, &frontend, "beforestore", &cached_content);

	/** 
	 * Take the lifetime from the frontend or read it from the set in start()
	 */
	if (!lifetime || Z_TYPE_P(lifetime) == IS_NULL) {
		phalcon_return_property(&ttl, getThis(), SL("_lastLifetime"));

		if (Z_TYPE(ttl) == IS_NULL) {
			PHALCON_CALL_METHODW(&ttl, &frontend, "getlifetime");
		}
	} else {
		PHALCON_CPY_WRT(&ttl, lifetime);
	}
	
	if (phalcon_is_numeric(&cached_content)) {
		PHALCON_CALL_METHODW(&success, &redis, "set", &last_key, &cached_content);
	} else {
		PHALCON_CALL_METHODW(&success, &redis, "set", &last_key, &prepared_content);
	}

	PHALCON_CALL_METHODW(&success, &redis, "settimeout", &last_key, &ttl);

	if (!zend_is_true(&success)) {
		PHALCON_THROW_EXCEPTION_STRW(phalcon_cache_exception_ce, "Failed to store data in redisd");
		return;
	}
	
	phalcon_return_property(&options, getThis(), SL("_options"));

	if (unlikely(!phalcon_array_isset_fetch_str(&special_key, &options, SL("statsKey")))) {
		PHALCON_THROW_EXCEPTION_STRW(phalcon_cache_exception_ce, "Unexpected inconsistency in options");
		return;
	}

	if (Z_TYPE(special_key) != IS_NULL) {
		PHALCON_CALL_METHODW(NULL, &redis, "sadd", &special_key, &prefixed_key);
	}
	
	PHALCON_CALL_METHODW(&is_buffering, &frontend, "isbuffering");

	if (!stop_buffer || PHALCON_IS_TRUE(stop_buffer)) {
		PHALCON_CALL_METHODW(NULL, &frontend, "stop");
	}
	
	if (PHALCON_IS_TRUE(&is_buffering)) {
		zend_print_zval(&cached_content, 0);
	}
	
	phalcon_update_property_bool(getThis(), SL("_started"), 0);
}

/**
 * Deletes a value from the cache by its key
 *
 * @param int|string $keyName
 * @return boolean
 */
PHP_METHOD(Phalcon_Cache_Backend_Redis, delete){

	zval *key_name, redis = {}, prefix = {}, prefixed_key = {}, last_key = {}, options = {}, special_key = {}, ret = {};

	phalcon_fetch_params(0, 1, 0, &key_name);
	
	phalcon_return_property(&redis, getThis(), SL("_redis"));
	if (Z_TYPE(redis) != IS_OBJECT) {
		PHALCON_CALL_METHODW(&redis, getThis(), "_connect");
	}
	
	phalcon_return_property(&prefix, getThis(), SL("_prefix"));

	PHALCON_CONCAT_VV(&prefixed_key, &prefix, key_name);
	PHALCON_CONCAT_SV(&last_key, "_PHCR", &prefixed_key);
	
	phalcon_return_property(&options, getThis(), SL("_options"));
	
	if (unlikely(!phalcon_array_isset_fetch_str(&special_key, &options, SL("statsKey")))) {
		PHALCON_THROW_EXCEPTION_STRW(phalcon_cache_exception_ce, "Unexpected inconsistency in options");
		return;
	}

	if (Z_TYPE(special_key) != IS_NULL) {
		PHALCON_CALL_METHODW(NULL, &redis, "srem", &special_key, &prefixed_key);
	}
	
	/* Delete the key from redisd */
	PHALCON_CALL_METHODW(&ret, &redis, "delete", &last_key);
	if (zend_is_true(&ret)) {
		RETURN_TRUE;
	}

	RETURN_FALSE;
}

/**
 * Query the existing cached keys
 *
 * @param string $prefix
 * @return array
 */
PHP_METHOD(Phalcon_Cache_Backend_Redis, queryKeys){

	zval *prefix = NULL, redis = {}, options = {}, special_key = {}, keys = {}, *value;

	phalcon_fetch_params(0, 0, 1, &prefix);

	phalcon_return_property(&options, getThis(), SL("_options"));
	if (unlikely(!phalcon_array_isset_fetch_str(&special_key, &options, SL("statsKey")))) {
		zend_throw_exception_ex(phalcon_cache_exception_ce, 0, "Unexpected inconsistency in options");
		return;
	}

	array_init(return_value);
	if (Z_TYPE(special_key) == IS_NULL) {
		return;
	}

	phalcon_return_property(&redis, getThis(), SL("_redis"));
	if (Z_TYPE(redis) != IS_OBJECT) {
		PHALCON_CALL_METHODW(&redis, getThis(), "_connect");
	}
	
	/* Get the key from redisd */
	PHALCON_CALL_METHODW(&keys, &redis, "smembers", &special_key);
	if (Z_TYPE(keys) == IS_ARRAY) {
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL(keys), value) {
			zval real_key = {};
			if (!prefix || !zend_is_true(prefix) || phalcon_start_with(value, prefix, NULL)) {
				ZVAL_NEW_STR(&real_key, Z_STR_P(value));
				phalcon_array_append(return_value, &real_key, PH_COPY);
			}
		} ZEND_HASH_FOREACH_END();
	}
}

/**
 * Checks if cache exists and it hasn't expired
 *
 * @param  string $keyName
 * @param  long $lifetime
 * @return boolean
 */
PHP_METHOD(Phalcon_Cache_Backend_Redis, exists){

	zval *key_name = NULL, *lifetime = NULL, value = {}, last_key = {}, prefix = {}, redis = {};

	phalcon_fetch_params(0, 0, 2, &key_name, &lifetime);
	
	if (!key_name || Z_TYPE_P(key_name) == IS_NULL) {
		phalcon_return_property(&last_key, getThis(), SL("_lastKey"));
	} else {
		phalcon_return_property(&prefix, getThis(), SL("_prefix"));
		PHALCON_CONCAT_SVV(&last_key, "_PHCR", &prefix, key_name);
	}

	if (zend_is_true(&last_key)) {
		phalcon_return_property(&redis, getThis(), SL("_redis"));
		if (Z_TYPE(redis) != IS_OBJECT) {
			PHALCON_CALL_METHODW(&redis, getThis(), "_connect");
		}
	
		PHALCON_CALL_METHODW(&value, &redis, "get", &last_key);
		RETVAL_BOOL(PHALCON_IS_NOT_FALSE(&value));
	} else {
		RETURN_FALSE;
	}
}

/**
 * Atomic increment of a given key, by number $value
 * 
 * @param  string $keyName
 * @param  long $value
 * @return mixed
 */
PHP_METHOD(Phalcon_Cache_Backend_Redis, increment){

	zval *key_name = NULL, *value = NULL, redis = {}, last_key = {}, prefix = {};

	phalcon_fetch_params(0, 0, 2, &key_name, &value);

	if (!key_name || Z_TYPE_P(key_name) == IS_NULL) {
		phalcon_return_property(&last_key, getThis(), SL("_lastKey"));
	} else {
		phalcon_return_property(&prefix, getThis(), SL("_prefix"));
		PHALCON_CONCAT_SVV(&last_key, "_PHCR", &prefix, key_name);
	}

	if (!value || Z_TYPE_P(value) == IS_NULL) {
		value = &PHALCON_GLOBAL(z_one);
	} else if (Z_TYPE_P(value) != IS_LONG) {
		PHALCON_SEPARATE_PARAM(value);
		convert_to_long_ex(value);
	}

	phalcon_return_property(&redis, getThis(), SL("_redis"));
	if (Z_TYPE(redis) != IS_OBJECT) {
		PHALCON_CALL_METHODW(&redis, getThis(), "_connect");
	}

	PHALCON_RETURN_CALL_METHODW(&redis, "incrby", &last_key, value);
}

/**
 * Atomic decrement of a given key, by number $value
 * 
 * @param  string $keyName
 * @param  long $value
 * @return mixed
 */
PHP_METHOD(Phalcon_Cache_Backend_Redis, decrement){

	zval *key_name = NULL, *value = NULL, redis = {}, last_key = {}, prefix = {};

	phalcon_fetch_params(0, 0, 2, &key_name, &value);

	if (!key_name || Z_TYPE_P(key_name) == IS_NULL) {
		phalcon_return_property(&last_key, getThis(), SL("_lastKey"));
	} else {
		phalcon_return_property(&prefix, getThis(), SL("_prefix"));
		PHALCON_CONCAT_SVV(&last_key, "_PHCR", &prefix, key_name);
	}

	if (!value || Z_TYPE_P(value) == IS_NULL) {
		value = &PHALCON_GLOBAL(z_one);
	} else if (Z_TYPE_P(value) != IS_LONG) {
		PHALCON_SEPARATE_PARAM(value);
		convert_to_long_ex(value);
	}

	phalcon_return_property(&redis, getThis(), SL("_redis"));
	if (Z_TYPE(redis) != IS_OBJECT) {
		PHALCON_CALL_METHODW(&redis, getThis(), "_connect");
	}

	PHALCON_RETURN_CALL_METHODW(&redis, "decrby", &last_key, value);
}

/**
 * Immediately invalidates all existing items.
 * 
 * @return boolean
 */
PHP_METHOD(Phalcon_Cache_Backend_Redis, flush){

	zval redis = {}, options = {}, special_key = {}, keys = {}, *value;

	phalcon_return_property(&options, getThis(), SL("_options"));
	
	if (unlikely(!phalcon_array_isset_fetch_str(&special_key, &options, SL("statsKey")))) {
		PHALCON_THROW_EXCEPTION_STRW(phalcon_cache_exception_ce, "Unexpected inconsistency in options");
		return;
	}

	if (Z_TYPE(special_key) == IS_NULL) {
		RETURN_FALSE;
	}

	phalcon_return_property(&redis, getThis(), SL("_redis"));
	if (Z_TYPE(redis) != IS_OBJECT) {
		PHALCON_CALL_METHODW(&redis, getThis(), "_connect");
	}
	
	/* Get the key from redisd */
	PHALCON_CALL_METHODW(&keys, &redis, "smembers", &special_key);
	if (Z_TYPE(keys) == IS_ARRAY) {
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL(keys), value) {
			zval real_key = {}, last_key = {};

			PHALCON_CPY_WRT_CTOR(&real_key, value);
			PHALCON_CONCAT_SV(&last_key, "_PHCR", &real_key);

			PHALCON_CALL_METHODW(NULL, &redis, "delete", &last_key);	
			PHALCON_CALL_METHODW(NULL, &redis, "srem", &special_key, &real_key);
		} ZEND_HASH_FOREACH_END();
		
		zend_hash_clean(Z_ARRVAL(keys));
	}
	
	RETURN_TRUE;
}

PHP_METHOD(Phalcon_Cache_Backend_Redis, getTrackingKey)
{
	zval options = {}, stats_key = {};

	phalcon_return_property(&options, getThis(), SL("_options"));

	if (!phalcon_array_isset_fetch_str(&stats_key, &options, SL("statsKey"))) {
		RETURN_NULL();
	}

	RETURN_CTORW(&stats_key);
}

PHP_METHOD(Phalcon_Cache_Backend_Redis, setTrackingKey)
{
	zval *key;

	phalcon_fetch_params(0, 1, 0, &key);

	phalcon_update_property_array_str(getThis(), SL("_options"), SL("statsKey"), key);

	RETURN_THISW();
}
