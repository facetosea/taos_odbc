/*
 * MIT License
 *
 * Copyright (c) 2022-2023 freemine <freemine@yeah.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _internal_h_
#define _internal_h_

#include "os_port.h"
#include "enums.h"

#include "list.h"
#include "utils.h"

#include "typedefs.h"

#include "taos_helpers.h"

#include <taos.h>

EXTERN_C_BEGIN

// <TDengine/ include/util/tdef.h
// ...
// #define TSDB_NODE_NAME_LEN  64
// #define TSDB_TABLE_NAME_LEN 193  // it is a null-terminated string
// #define TSDB_TOPIC_NAME_LEN 193  // it is a null-terminated string
// #define TSDB_CGROUP_LEN     193  // it is a null-terminated string
// #define TSDB_DB_NAME_LEN    65
// #define TSDB_DB_FNAME_LEN   (TSDB_ACCT_ID_LEN + TSDB_DB_NAME_LEN + TSDB_NAME_DELIMITER_LEN)
// ...
// #define TSDB_COL_NAME_LEN        65
// ...

#define MAX_CATALOG_NAME_LEN        64
#define MAX_SCHEMA_NAME_LEN         64
#define MAX_TABLE_NAME_LEN         192
#define MAX_COLUMN_NAME_LEN         64

typedef struct err_s             err_t;

struct err_s {
  int                         err;
  const char                 *estr;
  SQLCHAR                     sql_state[6];

  char                        buf[1024];

  struct tod_list_head        node;
};

struct errs_s {
  struct tod_list_head        errs;
  struct tod_list_head        frees;

  conn_t                     *connected_conn; // NOTE: no ownership
};

#define conn_data_source(_conn) _conn->cfg.dsn ? _conn->cfg.dsn : (_conn->cfg.driver ? _conn->cfg.driver : "")

#define env_append_err(_env, _sql_state, _e, _estr) errs_append(&_env->errs, _sql_state, _e, _estr)

#define env_append_err_format(_env, _sql_state, _e, _fmt, ...) errs_append_format(&_env->errs, _sql_state, _e, _fmt, ##__VA_ARGS__)

#define env_oom(_env) errs_oom(&_env->errs)

#define conn_append_err(_conn, _sql_state, _e, _estr) errs_append(&_conn->errs, _sql_state, _e, _estr)

#define conn_append_err_format(_conn, _sql_state, _e, _fmt, ...) errs_append_format(&_conn->errs, _sql_state, _e, _fmt, ##__VA_ARGS__)

#define conn_oom(_conn) errs_oom(&_conn->errs)

#define conn_niy(_conn) errs_niy(&_conn->errs)

#define conn_nsy(_conn) errs_nsy(&_conn->errs)

#define stmt_append_err(_stmt, _sql_state, _e, _estr) errs_append(&_stmt->errs, _sql_state, _e, _estr)

#define stmt_append_err_format(_stmt, _sql_state, _e, _fmt, ...) errs_append_format(&_stmt->errs, _sql_state, _e, _fmt, ##__VA_ARGS__)

#define stmt_oom(_stmt) errs_oom(&_stmt->errs)

#define stmt_niy(_stmt) errs_niy(&_stmt->errs)

#define stmt_nsy(_stmt) errs_nsy(&_stmt->errs)

#define desc_append_err(_desc, _sql_state, _e, _estr) errs_append(&_desc->errs, _sql_state, _e, _estr)

#define desc_append_err_format(_desc, _sql_state, _e, _fmt, ...) errs_append_format(&_desc->errs, _sql_state, _e, _fmt, ##__VA_ARGS__)

#define desc_oom(_desc) errs_oom(&_desc->errs)

#define desc_niy(_desc) errs_niy(&_desc->errs)

#define desc_nsy(_desc) errs_nsy(&_desc->errs)


static inline int sql_succeeded(SQLRETURN sr)
{
  return sr == SQL_SUCCESS || sr == SQL_SUCCESS_WITH_INFO;
}

typedef struct sqlc_data_s             sqlc_data_t;
struct sqlc_data_s {
  // https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/c-data-types?view=sql-server-ver16
  SQLSMALLINT           type;
  union {
    uint8_t             b;
    int8_t              i8;
    uint8_t             u8;
    int16_t             i16;
    uint16_t            u16;
    int32_t             i32;
    uint32_t            u32;
    int64_t             i64;
    uint64_t            u64;
    float               flt;
    double              dbl;
    struct {
      const char *str;
      size_t      len;
    }                   str;
    int64_t             ts;
  };

  mem_t          mem;
  size_t         len;
  size_t         cur;

  uint8_t               is_null:1;
};

typedef enum {
  DATA_TYPE_UNKNOWM,
  DATA_TYPE_INT8,
  DATA_TYPE_UINT8,
  DATA_TYPE_INT16,
  DATA_TYPE_UINT16,
  DATA_TYPE_INT32,
  DATA_TYPE_UINT32,
  DATA_TYPE_INT64,
  DATA_TYPE_UINT64,
  DATA_TYPE_FLOAT,
  DATA_TYPE_DOUBLE,
  DATA_TYPE_STR,
  DATA_TYPE_MAX,
} data_type;

struct env_s {
  atomic_int          refc;

  atomic_int          conns;

  errs_t              errs;

  mem_t               mem;

  unsigned int        debug_flex:1;
  unsigned int        debug_bison:1;
};

// https://github.com/MicrosoftDocs/sql-docs/blob/live/docs/odbc/reference/develop-app/descriptor-field-conformance.md

struct desc_header_s {
  // header fields settable by SQLSetStmtAttr
  SQLULEN             DESC_ARRAY_SIZE;
  SQLUSMALLINT       *DESC_ARRAY_STATUS_PTR;
  SQLULEN            *DESC_BIND_OFFSET_PTR;
  SQLULEN             DESC_BIND_TYPE;
  SQLULEN            *DESC_ROWS_PROCESSED_PTR;

  // header fields else
  SQLUSMALLINT        DESC_COUNT;
  // SQL_DESC_ALLOC_TYPE
};

struct get_data_ctx_s {
  SQLUSMALLINT   Col_or_Param_Num;
  SQLSMALLINT    TargetType;

  tsdb_data_t    tsdb;
  sqlc_data_t    sqlc;

  //
  char           buf[64];
  mem_t          mem;

  const char    *pos;
  size_t         nr;
};

typedef struct tsdb_param_column_s         tsdb_param_column_t;
struct tsdb_param_column_s {
  mem_t           mem;
  mem_t           mem_length;
  mem_t           mem_is_null;
  SQLRETURN (*conv)(stmt_t *stmt, int i_param);
};

typedef struct tsdb_paramset_s             tsdb_paramset_t;
struct tsdb_paramset_s {
  int                   cap;
  int                   nr;
  tsdb_param_column_t  *params;
};

struct tsdb_binds_s {
  int                        cap;
  int                        nr;

  TAOS_MULTI_BIND           *mbs;
};

struct desc_record_s {
  SQLLEN                       *DESC_INDICATOR_PTR;
  SQLLEN                       *DESC_OCTET_LENGTH_PTR;
  SQLLEN                        DESC_PARAMETER_TYPE;

  SQLLEN                        DESC_AUTO_UNIQUE_VALUE;
  SQLCHAR                       DESC_BASE_COLUMN_NAME[192+1];
  SQLCHAR                       DESC_BASE_TABLE_NAME[192+1];
  SQLLEN                        DESC_CASE_SENSITIVE;
  SQLCHAR                       DESC_CATALOG_NAME[192+1];
  SQLLEN                        DESC_CONCISE_TYPE;
  SQLPOINTER                    DESC_DATA_PTR;
  SQLLEN                        DESC_COUNT;
  SQLLEN                        DESC_DISPLAY_SIZE;
  SQLLEN                        DESC_FIXED_PREC_SCALE;
  SQLCHAR                       DESC_LABEL[192+1];
  SQLLEN                        DESC_LENGTH;
  SQLCHAR                       DESC_LITERAL_PREFIX[128+1];
  SQLCHAR                       DESC_LITERAL_SUFFIX[128+1];
  SQLCHAR                       DESC_LOCAL_TYPE_NAME[128+1];
  SQLCHAR                       DESC_NAME[192+1];
  SQLLEN                        DESC_NULLABLE;
  SQLLEN                        DESC_NUM_PREC_RADIX;
  SQLLEN                        DESC_OCTET_LENGTH;
  SQLLEN                        DESC_PRECISION;
  SQLLEN                        DESC_SCALE;
  SQLCHAR                       DESC_SCHEMA_NAME[192+1];
  SQLLEN                        DESC_SEARCHABLE;
  SQLCHAR                       DESC_TABLE_NAME[192+1];
  SQLLEN                        DESC_TYPE;
  SQLCHAR                       DESC_TYPE_NAME[64+1];
  SQLLEN                        DESC_UNNAMED;
  SQLLEN                        DESC_UNSIGNED;
  SQLLEN                        DESC_UPDATABLE;


  // SQL_DESC_DATETIME_INTERVAL_CODE
  // SQL_DESC_DATETIME_INTERVAL_PRECISION


  int                           tsdb_type;



  unsigned int                  bound:1;
};

struct descriptor_s {
  desc_header_t                 header;

  desc_record_t                *records;
  size_t                        cap;
};

struct desc_s {
  atomic_int                    refc;

  descriptor_t                  descriptor;

  conn_t                       *conn;

  // https://learn.microsoft.com/en-us/sql/odbc/reference/develop-app/types-of-descriptors?view=sql-server-ver16
  // `A row descriptor in one statement can serve as a parameter descriptor in another statement.`
  struct tod_list_head          associated_stmts_as_ARD; // struct stmt_s*
  struct tod_list_head          associated_stmts_as_APD; // struct stmt_s*

  errs_t                        errs;
};

struct charset_conv_s {
  char         from[64];
  char         to[64];
  iconv_t      cnv;

  struct tod_list_head        node;
};

struct charset_conv_mgr_s {
  struct tod_list_head        convs; // charset_conv_t*
};

struct conn_cfg_s {
  char                  *driver;
  char                  *dsn;
  char                  *uid;
  char                  *pwd;
  char                  *ip;
  char                  *db;
  int                    port;

  // NOTE: 1.this is to hack node.odbc, which maps SQL_TINYINT to SQL_C_UTINYINT
  //       2.node.odbc does not call SQLGetInfo/SQLColAttribute to get signess of integers
  unsigned int           unsigned_promotion:1;
  // NOTE: this is to hack PowerBI, which seems not displace seconds-fractional,
  //       thus, if timestamp_as_is is not set, TSDB_DATA_TYPE_TIMESTAMP would map to SQL_WVARCHAR
  unsigned int           timestamp_as_is:1;

  unsigned int           unsigned_promotion_set:1;
  unsigned int           timestamp_as_is_set:1;
  unsigned int           port_set:1;
};

struct parser_nterm_s {
  size_t           start;
  size_t           end;
};

struct sqls_s {
  parser_nterm_t        *sqls;
  size_t                 cap;
  size_t                 nr;

  size_t                 pos; // 1-based

  uint8_t                failed:1;
};

struct parser_token_s {
  const char      *text;
  size_t           leng;
};

struct topic_cfg_s {
  char                 **names;
  size_t                 names_cap;
  size_t                 names_nr;

  kvs_t                  kvs;
};

struct parser_ctx_s {
  int                    row0, col0;
  int                    row1, col1;
  char                   err_msg[1024];

  // globally 0-based
  size_t                 token_start;
  size_t                 token_end;

  unsigned int           debug_flex:1;
  unsigned int           debug_bison:1;
  unsigned int           oom:1;
};

struct conn_parser_param_s {
  conn_cfg_t             conn_cfg;

  parser_ctx_t           ctx;
};

struct ext_parser_param_s {
  topic_cfg_t            topic_cfg;

  parser_ctx_t           ctx;
};

struct sqls_parser_param_s {
  int (*sql_found)(sqls_parser_param_t *param, size_t start, size_t end, void *arg);
  void                  *arg;

  parser_ctx_t           ctx;
};

struct conn_s {
  atomic_int          refc;
  atomic_int          stmts;
  atomic_int          descs;
  atomic_int          outstandings;

  env_t              *env;

  conn_cfg_t          cfg;

  // server info
  const char         *svr_info;
  // client-side-timezone, which is set via `taos.cfg`
  // we use 'select to_iso8601(0)' to get the timezone info per connection
  // currently, we just get this info but not use it in anyway
  // all timestamp would be converted into SQL_C_CHAR/WCHAR according to local-timezone of your machine, via system-call `localtime_r`
  // which is the ODBC convention we believe
  // if you really wanna map timestamp to timezone that is different, you might SQLGetData(...SQL_C_BIGINT...) to get the raw int64_t of timestamp,
  // whose main part (excluding seconds fraction) represents the time in seconds since the Epoch (00:00:00 UTC, January 1, 1970)
  int64_t             tz;         // +0800 for Asia/Shanghai
  int64_t             tz_seconds; // +28800 for Asia/Shanghai

  // config from information_schema.ins_configs
  char               *s_statusInterval;
  char               *s_timezone; // this is server-side timezone
  char               *s_locale;
  char               *s_charset;

  char               *sql_c_char_charset;
  char               *tsdb_varchar_charset;

  charset_conv_t      _cnv_tsdb_varchar_to_sql_c_char;
  charset_conv_t      _cnv_tsdb_varchar_to_sql_c_wchar;

  charset_conv_t      _cnv_sql_c_char_to_tsdb_varchar;
  charset_conv_t      _cnv_sql_c_char_to_sql_c_wchar;

  errs_t              errs;

  TAOS               *taos;

  unsigned int        fmt_time:1;
};

struct stmt_get_data_args_s {
  SQLUSMALLINT   Col_or_Param_Num;
  SQLSMALLINT    TargetType;
  SQLPOINTER     TargetValuePtr;
  SQLLEN         BufferLength;
  SQLLEN        *StrLenPtr;
  SQLLEN        *IndPtr;
};

struct stmt_base_s {
  SQLRETURN (*query)(stmt_base_t *base, const char *sql);
  SQLRETURN (*execute)(stmt_base_t *base);
  SQLRETURN (*get_fields)(stmt_base_t *base, TAOS_FIELD **fields, size_t *nr);
  SQLRETURN (*fetch_row)(stmt_base_t *base);
  SQLRETURN (*more_results)(stmt_base_t *base);
  SQLRETURN (*describe_param)(stmt_base_t *base,
      SQLUSMALLINT    ParameterNumber,
      SQLSMALLINT    *DataTypePtr,
      SQLULEN        *ParameterSizePtr,
      SQLSMALLINT    *DecimalDigitsPtr,
      SQLSMALLINT    *NullablePtr);
  SQLRETURN (*get_num_params)(stmt_base_t *base, SQLSMALLINT *ParameterCountPtr);
  SQLRETURN (*check_params)(stmt_base_t *base);
  SQLRETURN (*tsdb_field_by_param)(stmt_base_t *base, int i_param, TAOS_FIELD_E **field);
  SQLRETURN (*row_count)(stmt_base_t *base, SQLLEN *row_count_ptr);
  SQLRETURN (*get_num_cols)(stmt_base_t *base, SQLSMALLINT *ColumnCountPtr);
  SQLRETURN (*get_data)(stmt_base_t *base, SQLUSMALLINT Col_or_Param_Num, tsdb_data_t *tsdb);
};

struct tsdb_fields_s {
  TAOS_FIELD                *fields;
  size_t                     nr;
};

struct tsdb_rows_block_s {
  TAOS_ROW            rows;
  size_t              nr;
  size_t              pos;           // 1-based
};

struct tsdb_res_s {
  TAOS_RES                  *res;
  size_t                     affected_row_count;
  int                        time_precision;
  tsdb_fields_t              fields;
  tsdb_rows_block_t          rows_block;

  unsigned int               res_is_from_taos_query:1;
};

struct tsdb_params_s {
  tsdb_stmt_t                        *owner;
  char                               *subtbl;
  TAOS_FIELD_E                        subtbl_field;
  TAOS_FIELD_E                       *tag_fields;
  int                                 nr_tag_fields;
  TAOS_FIELD_E                       *col_fields;
  int                                 nr_col_fields;

  mem_t                               mem;

  unsigned int                        prepared:1;
  unsigned int                        is_insert_stmt:1;
  unsigned int                        subtbl_required:1;
};

struct tsdb_stmt_s {
  stmt_base_t                base;

  stmt_t                    *owner;

  TAOS_STMT                 *stmt;
  // for insert-parameterized-statement
  tsdb_params_t              params;

  tsdb_binds_t               binds;

  tsdb_res_t                 res;

  unsigned int               prepared:1;
};

struct topic_s {
  stmt_base_t                base;
  stmt_t                    *owner;

  char                       name[193];
  topic_cfg_t                cfg;

  tmq_conf_t                *conf;
  tmq_t                     *tmq;

  TAOS_RES                  *res;
  mem_t                      res_topic_name;
  mem_t                      res_db_name;
  int32_t                    res_vgroup_id;

  TAOS_FIELD                *fields;
  size_t                     fields_cap;
  size_t                     fields_nr;

  TAOS_ROW                   row;

  uint8_t                    subscribed:1;
  uint8_t                    do_not_commit:1;
};

struct tables_args_s {
  wildex_t        *catalog_pattern;
  wildex_t        *schema_pattern;
  wildex_t        *table_pattern;
  wildex_t        *type_pattern;
};

typedef enum tables_type_e     tables_type_t;

enum tables_type_e {
  TABLES_FOR_GENERIC,
  TABLES_FOR_CATALOGS,
  TABLES_FOR_SCHEMAS,
  TABLES_FOR_TABLETYPES,
};

struct tables_s {
  stmt_base_t                base;
  stmt_t                    *owner;

  tables_args_t              tables_args;

  tsdb_stmt_t                stmt;

  const unsigned char       *catalog;
  const unsigned char       *schema;
  const unsigned char       *table;
  const unsigned char       *type;

  mem_t                      catalog_cache;
  mem_t                      schema_cache;
  mem_t                      table_cache;
  mem_t                      type_cache;

  mem_t                      table_types;

  tables_type_t              tables_type;
};

struct columns_args_s {
  wildex_t        *catalog_pattern;
  wildex_t        *schema_pattern;
  wildex_t        *table_pattern;
  wildex_t        *column_pattern;
};

struct columns_s {
  stmt_base_t                base;
  stmt_t                    *owner;

  columns_args_t             columns_args;

  tables_t                   tables;

  tsdb_data_t                current_catalog;
  tsdb_data_t                current_schema;
  tsdb_data_t                current_table;
  tsdb_data_t                current_table_type;

  tsdb_data_t                current_col_name;
  tsdb_data_t                current_col_type;
  tsdb_data_t                current_col_length;
  tsdb_data_t                current_col_note;

  tsdb_stmt_t                desc;      // desc <catalog>.<table_name>

  tsdb_stmt_t                query;     // select * from <catalog>.<table_name>

  int                        ordinal_order;

  mem_t                      column_cache;
};

struct primarykeys_args_s {
  wildex_t        *catalog_pattern;
  wildex_t        *schema_pattern;
  wildex_t        *table_pattern;
};

struct primarykeys_s {
  stmt_base_t                base;
  stmt_t                    *owner;

  primarykeys_args_t         primarykeys_args;

  tables_t                   tables;

  tsdb_data_t                current_catalog;
  tsdb_data_t                current_schema;
  tsdb_data_t                current_table;
  tsdb_data_t                current_table_type;

  tsdb_data_t                current_col_name;
  tsdb_data_t                current_col_type;
  tsdb_data_t                current_col_length;
  tsdb_data_t                current_col_note;

  tsdb_stmt_t                desc;

  int                        ordinal_order;
};

struct typesinfo_s {
  stmt_base_t                base;
  stmt_t                    *owner;

  SQLSMALLINT                data_type;

  size_t                     pos; // 1-based
};

struct stmt_s {
  atomic_int                 refc;

  conn_t                    *conn;

  errs_t                     errs;

  struct tod_list_head       associated_APD_node;
  desc_t                    *associated_APD;

  struct tod_list_head       associated_ARD_node;
  desc_t                    *associated_ARD;

  descriptor_t               APD, IPD;
  descriptor_t               ARD, IRD;

  descriptor_t              *current_APD;
  descriptor_t              *current_ARD;

  get_data_ctx_t             get_data_ctx;

  mem_t                      raw;
  sqls_t                     sqls;

  mem_t                      sql;

  tsdb_paramset_t            paramset;

  tsdb_binds_t               tsdb_binds;

  tsdb_stmt_t                tsdb_stmt;
  tables_t                   tables;
  columns_t                  columns;
  typesinfo_t                typesinfo;
  primarykeys_t              primarykeys;
  topic_t                    topic;

  mem_t                      mem;

  stmt_base_t               *base;

  unsigned int               strict:1; // 1: param-truncation as failure
};

struct tls_s {
  mem_t                      intermediate;
  charset_conv_mgr_t        *mgr;
  // debug leakage only
  char                      *leakage;
};

EXTERN_C_END

#endif // _internal_h_

