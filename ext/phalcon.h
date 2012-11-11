
/*
  +------------------------------------------------------------------------+
  | Phalcon Framework                                                      |
  +------------------------------------------------------------------------+
  | Copyright (c) 2011-2012 Phalcon Team (http://www.phalconphp.com)       |
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

#include "di/injectionawareinterface.h"
#include "events/eventsawareinterface.h"
#include "mvc/model/validatorinterface.h"
#include "cache/frontendinterface.h"
#include "mvc/model/metadatainterface.h"
#include "cache/backendinterface.h"
#include "db/dialectinterface.h"
#include "db/adapterinterface.h"
#include "mvc/model/resultsetinterface.h"
#include "flashinterface.h"
#include "paginator/adapterinterface.h"
#include "mvc/view/engineinterface.h"
#include "mvc/model/managerinterface.h"
#include "mvc/model/query/statusinterface.h"
#include "mvc/model/messageinterface.h"
#include "mvc/model/criteriainterface.h"
#include "session/baginterface.h"
#include "mvc/model/transaction/managerinterface.h"
#include "mvc/model/queryinterface.h"
#include "translate/adapterinterface.h"
#include "mvc/routerinterface.h"
#include "mvc/router/routeinterface.h"
#include "diinterface.h"
#include "mvc/viewinterface.h"
#include "mvc/urlinterface.h"
#include "logger/adapterinterface.h"
#include "mvc/modelinterface.h"
#include "mvc/dispatcherinterface.h"
#include "escaperinterface.h"
#include "db/indexinterface.h"
#include "db/columninterface.h"
#include "dispatcherinterface.h"
#include "db/referenceinterface.h"
#include "http/request/fileinterface.h"
#include "http/responseinterface.h"
#include "acl/resourceinterface.h"
#include "acl/roleinterface.h"
#include "session/adapterinterface.h"
#include "filterinterface.h"
#include "http/requestinterface.h"
#include "acl/adapterinterface.h"
#include "events/managerinterface.h"
#include "exception.h"
#include "di/injectable.h"
#include "mvc/model/validator.h"
#include "db/adapter.h"
#include "cache/backend.h"
#include "mvc/model/metadata.h"
#include "db/adapter/pdo.h"
#include "db/dialect.h"
#include "di.h"
#include "flash.h"
#include "mvc/model/resultset.h"
#include "dispatcher.h"
#include "mvc/view/engine.h"
#include "session/adapter.h"
#include "pconfig.h"
#include "mvc/model/exception.h"
#include "logger/adapter.h"
#include "translate/adapter.h"
#include "di/factorydefault.h"
#include "acl.h"
#include "tag.h"
#include "db.h"
#include "cache/exception.h"
#include "cache/backend/apc.h"
#include "cache/backend/file.h"
#include "cache/backend/mongo.h"
#include "cache/backend/memcache.h"
#include "cache/frontend/data.h"
#include "cache/frontend/none.h"
#include "cache/frontend/base64.h"
#include "cache/frontend/output.h"
#include "tag/select.h"
#include "tag/exception.h"
#include "paginator/exception.h"
#include "paginator/adapter/model.h"
#include "paginator/adapter/nativearray.h"
#include "db/column.h"
#include "db/index.h"
#include "db/adapter/pdo/mysql.h"
#include "db/adapter/pdo/sqlite.h"
#include "db/adapter/pdo/postgresql.h"
#include "db/dialect/sqlite.h"
#include "db/dialect/mysql.h"
#include "db/dialect/postgresql.h"
#include "db/exception.h"
#include "db/profiler.h"
#include "db/profiler/item.h"
#include "db/reference.h"
#include "db/rawvalue.h"
#include "db/result/pdo.h"
#include "db/resultinterface.h"
#include "acl/role.h"
#include "acl/resource.h"
#include "acl/adapter/memory.h"
#include "acl/exception.h"
#include "text.h"
#include "version.h"
#include "session.h"
#include "session/bag.h"
#include "session/exception.h"
#include "session/adapter/files.h"
#include "filter.h"
#include "di/exception.h"
#include "di/factorydefault/cli.h"
#include "filter/exception.h"
#include "flash/direct.h"
#include "flash/session.h"
#include "flash/exception.h"
#include "cli/task.h"
#include "cli/router.h"
#include "cli/console.h"
#include "cli/console/exception.h"
#include "cli/dispatcher.h"
#include "cli/dispatcher/exception.h"
#include "cli/router/exception.h"
#include "loader.h"
#include "logger.h"
#include "loader/exception.h"
#include "logger/item.h"
#include "logger/exception.h"
#include "logger/adapter/file.h"
#include "config/adapter/ini.h"
#include "config/exception.h"
#include "translate.h"
#include "translate/exception.h"
#include "translate/adapter/nativearray.h"
#include "escaper.h"
#include "escaper/exception.h"
#include "http/request.h"
#include "http/cookie.h"
#include "http/response.h"
#include "http/request/file.h"
#include "http/response/headers.h"
#include "http/request/exception.h"
#include "http/response/exception.h"
#include "mvc/model.h"
#include "mvc/micro.h"
#include "mvc/view.h"
#include "mvc/url.h"
#include "mvc/router.h"
#include "mvc/application.h"
#include "mvc/application/exception.h"
#include "mvc/controller.h"
#include "mvc/collection.h"
#include "mvc/collection/manager.h"
#include "mvc/collection/exception.h"
#include "mvc/dispatcher.h"
#include "mvc/dispatcher/exception.h"
#include "mvc/model/query.h"
#include "mvc/model/row.h"
#include "mvc/micro/exception.h"
#include "mvc/model/manager.h"
#include "mvc/model/message.h"
#include "mvc/model/criteria.h"
#include "mvc/model/query/lang.h"
#include "mvc/model/transaction.h"
#include "mvc/model/query/status.h"
#include "mvc/model/metadata/files.h"
#include "mvc/model/metadata/apc.h"
#include "mvc/model/resultset/complex.h"
#include "mvc/model/metadata/session.h"
#include "mvc/model/metadata/memory.h"
#include "mvc/model/resultset/simple.h"
#include "mvc/model/validator/email.h"
#include "mvc/model/validator/regex.h"
#include "mvc/model/validator/uniqueness.h"
#include "mvc/model/transaction/manager.h"
#include "mvc/model/transactioninterface.h"
#include "mvc/model/validator/presenceof.h"
#include "mvc/model/transaction/failed.h"
#include "mvc/model/transaction/exception.h"
#include "mvc/model/validator/numericality.h"
#include "mvc/model/validator/inclusionin.h"
#include "mvc/model/validator/stringlength.h"
#include "mvc/model/validator/exclusionin.h"
#include "mvc/router/route.h"
#include "mvc/router/exception.h"
#include "mvc/user/module.h"
#include "mvc/user/plugin.h"
#include "mvc/url/exception.h"
#include "mvc/user/component.h"
#include "mvc/view/exception.h"
#include "mvc/view/engine/pphp.h"
#include "mvc/view/engine/volt.h"
#include "mvc/view/engine/volt/compiler.h"
#include "events/event.h"
#include "events/manager.h"
#include "events/exception.h"
