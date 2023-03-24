/*
 * MIT License
 *
 * Copyright (c) 2022 freemine <freemine@yeah.net>
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

#include "internal.h"

#include "columns.h"
#include "tables.h"

#include "errs.h"
#include "log.h"
#include "taos_helpers.h"
#include "tsdb.h"

#include <errno.h>

void columns_args_reset(columns_args_t *args)
{
  if (!args) return;

  WILD_SAFE_FREE(args->catalog_pattern);
  WILD_SAFE_FREE(args->schema_pattern);
  WILD_SAFE_FREE(args->table_pattern);
  WILD_SAFE_FREE(args->column_pattern);
}

void columns_args_release(columns_args_t *args)
{
  if (!args) return;
  columns_args_reset(args);
}

static column_meta_t _columns_meta[] = {
  {
    /* 1 */
    /* name                            */ "TABLE_CAT",
    /* column_type_name                */ "VARCHAR",
    /* SQL_DESC_CONCISE_TYPE           */ SQL_VARCHAR,
    /* SQL_DESC_LENGTH                 */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_OCTET_LENGTH           */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 1024,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NULLABLE,
    /* SQL_DESC_UNSIGNED               */ SQL_TRUE,
    /* SQL_DESC_NUM_PREC_RADIX         */ 0,
  },{
    /* 2 */
    /* name                            */ "TABLE_SCHEM",
    /* column_type_name                */ "VARCHAR",
    /* SQL_DESC_CONCISE_TYPE           */ SQL_VARCHAR,
    /* SQL_DESC_LENGTH                 */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_OCTET_LENGTH           */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 1024,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NULLABLE,
    /* SQL_DESC_UNSIGNED               */ SQL_TRUE,
    /* SQL_DESC_NUM_PREC_RADIX         */ 0,
  },{
    /* 3 */
    /* name                            */ "TABLE_NAME",
    /* column_type_name                */ "VARCHAR",
    /* SQL_DESC_CONCISE_TYPE           */ SQL_VARCHAR,
    /* SQL_DESC_LENGTH                 */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_OCTET_LENGTH           */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 1024,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NO_NULLS,
    /* SQL_DESC_UNSIGNED               */ SQL_TRUE,
    /* SQL_DESC_NUM_PREC_RADIX         */ 0,
  },{
    /* 4 */
    /* name                            */ "COLUMN_NAME",
    /* column_type_name                */ "VARCHAR",
    /* SQL_DESC_CONCISE_TYPE           */ SQL_VARCHAR,
    /* SQL_DESC_LENGTH                 */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_OCTET_LENGTH           */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 1024,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NO_NULLS,
    /* SQL_DESC_UNSIGNED               */ SQL_TRUE,
    /* SQL_DESC_NUM_PREC_RADIX         */ 0,
  },{
    /* 5 */
    /* name                            */ "DATA_TYPE",
    /* column_type_name                */ "SMALLINT",         /* FIXME: what to fill? */
    /* SQL_DESC_CONCISE_TYPE           */ SQL_SMALLINT,
    /* SQL_DESC_LENGTH                 */ 5,                  /* FIXME: what to fill? */
    /* SQL_DESC_OCTET_LENGTH           */ 2,                  /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 5,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NO_NULLS,
    /* SQL_DESC_UNSIGNED               */ SQL_FALSE,          /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 10,                 /* NOTE: check it later */
  },{
    /* 6 */
    /* name                            */ "TYPE_NAME",
    /* column_type_name                */ "VARCHAR",
    /* SQL_DESC_CONCISE_TYPE           */ SQL_VARCHAR,
    /* SQL_DESC_LENGTH                 */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_OCTET_LENGTH           */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 1024,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NO_NULLS,
    /* SQL_DESC_UNSIGNED               */ SQL_TRUE,           /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 0,
  },{
    /* 7 */
    /* name                            */ "COLUMN_SIZE",
    /* column_type_name                */ "INT",              /* FIXME: what to fill? */
    /* SQL_DESC_CONCISE_TYPE           */ SQL_INTEGER,
    /* SQL_DESC_LENGTH                 */ 10,                 /* FIXME: what to fill? */
    /* SQL_DESC_OCTET_LENGTH           */ 4,                  /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 10,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NULLABLE,
    /* SQL_DESC_UNSIGNED               */ SQL_FALSE,          /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 10,                 /* NOTE: check it later */
  },{
    /* 8 */
    /* name                            */ "BUFFER_LENGTH",
    /* column_type_name                */ "INT",              /* FIXME: what to fill? */
    /* SQL_DESC_CONCISE_TYPE           */ SQL_INTEGER,
    /* SQL_DESC_LENGTH                 */ 10,                 /* FIXME: what to fill? */
    /* SQL_DESC_OCTET_LENGTH           */ 4,                  /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 10,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NULLABLE,
    /* SQL_DESC_UNSIGNED               */ SQL_FALSE,          /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 10,                 /* NOTE: check it later */
  },{
    /* 9 */
    /* name                            */ "DECIMAL_DIGITS",
    /* column_type_name                */ "SMALLINT",         /* FIXME: what to fill? */
    /* SQL_DESC_CONCISE_TYPE           */ SQL_SMALLINT,
    /* SQL_DESC_LENGTH                 */ 5,                  /* FIXME: what to fill? */
    /* SQL_DESC_OCTET_LENGTH           */ 2,                  /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 5,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NULLABLE,
    /* SQL_DESC_UNSIGNED               */ SQL_FALSE,          /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 10,                 /* NOTE: check it later */
  },{
    /* 10 */
    /* name                            */ "NUM_PREC_RADIX",
    /* column_type_name                */ "SMALLINT",         /* FIXME: what to fill? */
    /* SQL_DESC_CONCISE_TYPE           */ SQL_SMALLINT,
    /* SQL_DESC_LENGTH                 */ 5,                  /* FIXME: what to fill? */
    /* SQL_DESC_OCTET_LENGTH           */ 2,                  /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 5,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,  /* FIXME: SQL_ATTR_READWRITE_UNKNOWN */
    /* SQL_DESC_NULLABLE               */ SQL_NULLABLE,
    /* SQL_DESC_UNSIGNED               */ SQL_FALSE,          /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 10,                 /* NOTE: check it later */
  },{
    /* 11 */
    /* name                            */ "NULLABLE",
    /* column_type_name                */ "SMALLINT",         /* FIXME: what to fill? */
    /* SQL_DESC_CONCISE_TYPE           */ SQL_SMALLINT,
    /* SQL_DESC_LENGTH                 */ 5,                  /* FIXME: what to fill? */
    /* SQL_DESC_OCTET_LENGTH           */ 2,                  /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 5,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NO_NULLS,
    /* SQL_DESC_UNSIGNED               */ SQL_FALSE,          /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 10,                 /* NOTE: check it later */
  },{
    /* 12 */
    /* name                            */ "REMARKS",
    /* column_type_name                */ "VARCHAR",
    /* SQL_DESC_CONCISE_TYPE           */ SQL_VARCHAR,
    /* SQL_DESC_LENGTH                 */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_OCTET_LENGTH           */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 1024,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NULLABLE,
    /* SQL_DESC_UNSIGNED               */ SQL_TRUE,           /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 0,
  },{
    /* 13 */
    /* name                            */ "COLUMN_DEF",
    /* column_type_name                */ "VARCHAR",
    /* SQL_DESC_CONCISE_TYPE           */ SQL_VARCHAR,
    /* SQL_DESC_LENGTH                 */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_OCTET_LENGTH           */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 1024,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NULLABLE,
    /* SQL_DESC_UNSIGNED               */ SQL_TRUE,           /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 0,
  },{
    /* 14 */
    /* name                            */ "SQL_DATA_TYPE",
    /* column_type_name                */ "SMALLINT",         /* FIXME: what to fill? */
    /* SQL_DESC_CONCISE_TYPE           */ SQL_SMALLINT,
    /* SQL_DESC_LENGTH                 */ 5,                  /* FIXME: what to fill? */
    /* SQL_DESC_OCTET_LENGTH           */ 2,                  /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 5,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NO_NULLS,
    /* SQL_DESC_UNSIGNED               */ SQL_FALSE,          /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 10,                 /* NOTE: check it later */
  },{
    /* 15 */
    /* name                            */ "SQL_DATETIME_SUB",
    /* column_type_name                */ "SMALLINT",         /* FIXME: what to fill? */
    /* SQL_DESC_CONCISE_TYPE           */ SQL_SMALLINT,
    /* SQL_DESC_LENGTH                 */ 5,                  /* FIXME: what to fill? */
    /* SQL_DESC_OCTET_LENGTH           */ 2,                  /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 5,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NULLABLE,
    /* SQL_DESC_UNSIGNED               */ SQL_FALSE,          /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 10,                 /* NOTE: check it later */
  },{
    /* 16 */
    /* name                            */ "CHAR_OCTET_LENGTH",
    /* column_type_name                */ "INT",              /* FIXME: what to fill? */
    /* SQL_DESC_CONCISE_TYPE           */ SQL_INTEGER,
    /* SQL_DESC_LENGTH                 */ 10,                 /* FIXME: what to fill? */
    /* SQL_DESC_OCTET_LENGTH           */ 4,                  /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 10,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NULLABLE,
    /* SQL_DESC_UNSIGNED               */ SQL_FALSE,          /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 10,                 /* NOTE: check it later */
  },{
    /* 17 */
    /* name                            */ "ORDINAL_POSITION",
    /* column_type_name                */ "INT",              /* FIXME: what to fill? */
    /* SQL_DESC_CONCISE_TYPE           */ SQL_INTEGER,
    /* SQL_DESC_LENGTH                 */ 10,                 /* FIXME: what to fill? */
    /* SQL_DESC_OCTET_LENGTH           */ 4,                  /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 10,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NO_NULLS,
    /* SQL_DESC_UNSIGNED               */ SQL_FALSE,          /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 10,                 /* NOTE: check it later */
  },{
    /* 18 */
    /* name                            */ "IS_NULLABLE",
    /* column_type_name                */ "VARCHAR",
    /* SQL_DESC_CONCISE_TYPE           */ SQL_VARCHAR,
    /* SQL_DESC_LENGTH                 */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_OCTET_LENGTH           */ 1024,               /* hard-coded, big enough */
    /* SQL_DESC_PRECISION              */ 1024,
    /* SQL_DESC_SCALE                  */ 0,
    /* SQL_DESC_AUTO_UNIQUE_VALUE      */ SQL_FALSE,
    /* SQL_DESC_UPDATABLE              */ SQL_ATTR_READONLY,
    /* SQL_DESC_NULLABLE               */ SQL_NULLABLE,
    /* SQL_DESC_UNSIGNED               */ SQL_TRUE,           /* NOTE: check it later */
    /* SQL_DESC_NUM_PREC_RADIX         */ 0,
  },
};

static TAOS_FIELD _fields[] = {
  {
    /* 1 */
    /* name                            */ "TABLE_CAT",
    /* type                            */ TSDB_DATA_TYPE_VARCHAR,
    /* bytes                           */ 1024,               /* hard-coded, big enough */
  },{
    /* 2 */
    /* name                            */ "TABLE_SCHEM",
    /* type                            */ TSDB_DATA_TYPE_VARCHAR,
    /* bytes                           */ 1024,               /* hard-coded, big enough */
  },{
    /* 3 */
    /* name                            */ "TABLE_NAME",
    /* type                            */ TSDB_DATA_TYPE_VARCHAR,
    /* bytes                           */ 1024,               /* hard-coded, big enough */
  },{
    /* 4 */
    /* name                            */ "COLUMN_NAME",
    /* type                            */ TSDB_DATA_TYPE_VARCHAR,
    /* bytes                           */ 1024,               /* hard-coded, big enough */
  },{
    /* 5 */
    /* name                            */ "DATA_TYPE",
    /* type                            */ TSDB_DATA_TYPE_SMALLINT,
    /* bytes                           */ 2,
  },{
    /* 6 */
    /* name                            */ "TYPE_NAME",
    /* type                            */ TSDB_DATA_TYPE_VARCHAR,
    /* bytes                           */ 1024,               /* hard-coded, big enough */
  },{
    /* 7 */
    /* name                            */ "COLUMN_SIZE",
    /* type                            */ TSDB_DATA_TYPE_INT,
    /* bytes                           */ 4,
  },{
    /* 8 */
    /* name                            */ "BUFFER_LENGTH",
    /* type                            */ TSDB_DATA_TYPE_INT,
    /* bytes                           */ 4,
  },{
    /* 9 */
    /* name                            */ "DECIMAL_DIGITS",
    /* type                            */ TSDB_DATA_TYPE_SMALLINT,
    /* bytes                           */ 2,
  },{
    /* 10 */
    /* name                            */ "NUM_PREC_RADIX",
    /* type                            */ TSDB_DATA_TYPE_SMALLINT,
    /* bytes                           */ 2,
  },{
    /* 11 */
    /* name                            */ "NULLABLE",
    /* type                            */ TSDB_DATA_TYPE_SMALLINT,
    /* bytes                           */ 2,
  },{
    /* 12 */
    /* name                            */ "REMARKS",
    /* type                            */ TSDB_DATA_TYPE_VARCHAR,
    /* bytes                           */ 1024,               /* hard-coded, big enough */
  },{
    /* 13 */
    /* name                            */ "COLUMN_DEF",
    /* type                            */ TSDB_DATA_TYPE_VARCHAR,
    /* bytes                           */ 1024,               /* hard-coded, big enough */
  },{
    /* 14 */
    /* name                            */ "SQL_DATA_TYPE",
    /* type                            */ TSDB_DATA_TYPE_SMALLINT,
    /* bytes                           */ 2,
  },{
    /* 15 */
    /* name                            */ "SQL_DATETIME_SUB",
    /* type                            */ TSDB_DATA_TYPE_SMALLINT,
    /* bytes                           */ 2,
  },{
    /* 16 */
    /* name                            */ "CHAR_OCTET_LENGTH",
    /* type                            */ TSDB_DATA_TYPE_INT,
    /* bytes                           */ 4,
  },{
    /* 17 */
    /* name                            */ "ORDINAL_POSITION",
    /* type                            */ TSDB_DATA_TYPE_INT,
    /* bytes                           */ 4,
  },{
    /* 18 */
    /* name                            */ "IS_NULLABLE",
    /* type                            */ TSDB_DATA_TYPE_VARCHAR,
    /* bytes                           */ 1024,               /* hard-coded, big enough */
  },
};

SQLSMALLINT columns_get_count_of_col_meta(void)
{
  SQLSMALLINT nr = (SQLSMALLINT)(sizeof(_columns_meta) / sizeof(_columns_meta[0]));
  return nr;
}

const column_meta_t* columns_get_col_meta(int i_col)
{
  int nr = (int)(sizeof(_columns_meta) / sizeof(_columns_meta[0]));
  if (i_col < 0) return NULL;
  if (i_col >= nr) return NULL;
  return _columns_meta + i_col;
}

void columns_reset(columns_t *columns)
{
  if (!columns) return;

  tsdb_stmt_reset(&columns->desc);
  tables_reset(&columns->tables);

  mem_reset(&columns->column_cache);

  columns_args_reset(&columns->columns_args);

  columns->ordinal_order = 0;
}

void columns_release(columns_t *columns)
{
  if (!columns) return;
  columns_reset(columns);

  tsdb_stmt_release(&columns->desc);
  tables_release(&columns->tables);

  mem_release(&columns->column_cache);

  columns_args_release(&columns->columns_args);

  columns->owner = NULL;
}

static SQLRETURN _query(stmt_base_t *base, const char *sql)
{
  (void)sql;

  columns_t *columns = (columns_t*)base;
  (void)columns;
  stmt_append_err(columns->owner, "HY000", 0, "General error:internal logic error");
  return SQL_ERROR;
}

static SQLRETURN _execute(stmt_base_t *base)
{
  columns_t *columns = (columns_t*)base;
  (void)columns;
  stmt_append_err(columns->owner, "HY000", 0, "General error:internal logic error");
  return SQL_ERROR;
}

static SQLRETURN _get_fields(stmt_base_t *base, TAOS_FIELD **fields, size_t *nr)
{
  (void)fields;
  (void)nr;
  columns_t *columns = (columns_t*)base;
  (void)columns;
  *fields = _fields;
  *nr = sizeof(_fields)/sizeof(_fields[0]);
  return SQL_SUCCESS;
}

static SQLRETURN _fetch_and_desc_next_table(columns_t *columns)
{
  SQLRETURN sr = SQL_SUCCESS;

  sr = columns->tables.base.fetch_row(&columns->tables.base);
  if (sr == SQL_NO_DATA) return SQL_NO_DATA;
  if (sr != SQL_SUCCESS) return SQL_ERROR;

  sr = columns->tables.base.get_data(&columns->tables.base, 1, &columns->current_catalog);
  if (sr != SQL_SUCCESS) return SQL_ERROR;

  sr = columns->tables.base.get_data(&columns->tables.base, 2, &columns->current_schema);
  if (sr != SQL_SUCCESS) return SQL_ERROR;

  sr = columns->tables.base.get_data(&columns->tables.base, 3, &columns->current_table);
  if (sr != SQL_SUCCESS) return SQL_ERROR;

  sr = columns->tables.base.get_data(&columns->tables.base, 4, &columns->current_table_type);
  if (sr != SQL_SUCCESS) return SQL_ERROR;

  char sql[4096];
  int n = snprintf(sql, sizeof(sql), "desc `%.*s`.`%.*s`",
      (int)columns->current_catalog.str.len, columns->current_catalog.str.str,
      (int)columns->current_table.str.len, columns->current_table.str.str);
  if (n < 0 || (size_t)n >= sizeof(sql)) {
    stmt_append_err(columns->owner, "HY000", 0, "General error:internal logic error or buffer too small");
    return SQL_ERROR;
  }

  tsdb_stmt_reset(&columns->desc);
  tsdb_stmt_init(&columns->desc, columns->owner);

  sr = tsdb_stmt_query(&columns->desc, sql);
  if (sr != SQL_SUCCESS) {
    stmt_append_err(columns->owner, "HY000", 0, "General error:internal logic error or buffer too small");
    return SQL_ERROR;
  }

  return SQL_SUCCESS;
}

static SQLRETURN _fetch_row_with_tsdb(stmt_base_t *base, tsdb_data_t *tsdb)
{
  SQLRETURN sr = SQL_SUCCESS;
  int r = 0;

  columns_t *columns = (columns_t*)base;

  charset_conv_t *cnv = &columns->owner->conn->cnv_tsdb_varchar_to_sql_c_wchar;

again:

  sr = columns->desc.base.fetch_row(&columns->desc.base);
  if (sr == SQL_NO_DATA) {
    sr = _fetch_and_desc_next_table(columns);
    if (sr == SQL_NO_DATA) return SQL_NO_DATA;
    if (sr != SQL_SUCCESS) return SQL_ERROR;
    goto again;
  }
  if (sr != SQL_SUCCESS) return SQL_ERROR;

  if (columns->columns_args.column_pattern) {
    sr = columns->desc.base.get_data(&columns->desc.base, 1, tsdb);
    if (sr != SQL_SUCCESS) return SQL_ERROR;

    r = wildexec_n_ex(columns->columns_args.column_pattern, cnv->from, tsdb->str.str, tsdb->str.len);

    if (r) {
      // FIXME: out of memory
      goto again;
    }
  }

  return SQL_SUCCESS;
}

static SQLRETURN _fetch_row(stmt_base_t *base)
{
  SQLRETURN sr = SQL_SUCCESS;

  columns_t *columns = (columns_t*)base;

  tsdb_data_t tsdb = {0};

  sr = _fetch_row_with_tsdb(base, &tsdb);
  if (sr == SQL_SUCCESS) {
    ++columns->ordinal_order;
  }

  return sr;
}

static SQLRETURN _describe_param(stmt_base_t *base,
    SQLUSMALLINT    ParameterNumber,
    SQLSMALLINT    *DataTypePtr,
    SQLULEN        *ParameterSizePtr,
    SQLSMALLINT    *DecimalDigitsPtr,
    SQLSMALLINT    *NullablePtr)
{
  (void)ParameterNumber;
  (void)DataTypePtr;
  (void)ParameterSizePtr;
  (void)DecimalDigitsPtr;
  (void)NullablePtr;

  columns_t *columns = (columns_t*)base;
  (void)columns;
  stmt_append_err(columns->owner, "HY000", 0, "General error:not implemented yet");
  return SQL_ERROR;
}

static SQLRETURN _describe_col(stmt_base_t *base,
    SQLUSMALLINT   ColumnNumber,
    SQLCHAR       *ColumnName,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT   *NameLengthPtr,
    SQLSMALLINT   *DataTypePtr,
    SQLULEN       *ColumnSizePtr,
    SQLSMALLINT   *DecimalDigitsPtr,
    SQLSMALLINT   *NullablePtr)
{
  columns_t *columns = (columns_t*)base;

  const column_meta_t *col_meta = columns_get_col_meta(ColumnNumber - 1);

  *NameLengthPtr    = (SQLSMALLINT)strlen(col_meta->name);
  *DataTypePtr      = (SQLSMALLINT)col_meta->DESC_CONCISE_TYPE;
  *ColumnSizePtr    = col_meta->DESC_OCTET_LENGTH;
  *DecimalDigitsPtr = 0;
  *NullablePtr      = (SQLSMALLINT)col_meta->DESC_NULLABLE;

  int n = snprintf((char*)ColumnName, BufferLength, "%s", col_meta->name);
  if (n < 0 || n >= BufferLength) {
    stmt_append_err(columns->owner, "01004", 0, "String data, right truncated");
    return SQL_SUCCESS_WITH_INFO;
  }

  return SQL_SUCCESS;
}

static SQLRETURN _get_num_params(stmt_base_t *base, SQLSMALLINT *ParameterCountPtr)
{
  (void)ParameterCountPtr;

  columns_t *columns = (columns_t*)base;
  (void)columns;
  stmt_append_err(columns->owner, "HY000", 0, "General error:not implemented yet");
  return SQL_ERROR;
}

static SQLRETURN _check_params(stmt_base_t *base)
{
  columns_t *columns = (columns_t*)base;
  (void)columns;
  stmt_append_err(columns->owner, "HY000", 0, "General error:not implemented yet");
  return SQL_ERROR;
}

static SQLRETURN _tsdb_field_by_param(stmt_base_t *base, int i_param, TAOS_FIELD_E **field)
{
  (void)i_param;
  (void)field;

  columns_t *columns = (columns_t*)base;
  (void)columns;
  stmt_append_err(columns->owner, "HY000", 0, "General error:not implemented yet");
  return SQL_ERROR;
}

static SQLRETURN _row_count(stmt_base_t *base, SQLLEN *row_count_ptr)
{
  columns_t *columns = (columns_t*)base;
  (void)columns;

  if (row_count_ptr) *row_count_ptr = 0;

  return SQL_SUCCESS;
}

static SQLRETURN _get_num_cols(stmt_base_t *base, SQLSMALLINT *ColumnCountPtr)
{
  (void)base;

  *ColumnCountPtr = columns_get_count_of_col_meta();

  return SQL_SUCCESS;
}

static SQLRETURN _get_data(stmt_base_t *base, SQLUSMALLINT Col_or_Param_Num, tsdb_data_t *tsdb)
{
  SQLRETURN sr = SQL_SUCCESS;

  columns_t *columns = (columns_t*)base;
  tsdb_data_t *col_name        = &columns->current_col_name;
  tsdb_data_t *col_type        = &columns->current_col_type;
  tsdb_data_t *col_length      = &columns->current_col_length;
  tsdb_data_t *col_note        = &columns->current_col_note;

  sr = columns->desc.base.get_data(&columns->desc.base, 1, col_name);
  if (sr != SQL_SUCCESS) return SQL_ERROR;
  if (col_name->type != TSDB_DATA_TYPE_VARCHAR) {
    stmt_append_err_format(columns->owner, "HY000", 0, "General error:internal logic error:`%s`", taos_data_type(col_name->type));
    return SQL_ERROR;
  }
  sr = columns->desc.base.get_data(&columns->desc.base, 2, col_type);
  if (sr != SQL_SUCCESS) return SQL_ERROR;
  if (col_type->type != TSDB_DATA_TYPE_VARCHAR) {
    stmt_append_err_format(columns->owner, "HY000", 0, "General error:internal logic error:`%s`", taos_data_type(col_type->type));
    return SQL_ERROR;
  }
  sr = columns->desc.base.get_data(&columns->desc.base, 3, col_length);
  if (sr != SQL_SUCCESS) return SQL_ERROR;
  if (col_length->type != TSDB_DATA_TYPE_INT) {
    stmt_append_err_format(columns->owner, "HY000", 0, "General error:internal logic error:`%s`", taos_data_type(col_length->type));
    return SQL_ERROR;
  }
  sr = columns->desc.base.get_data(&columns->desc.base, 4, col_note);
  if (sr != SQL_SUCCESS) return SQL_ERROR;
  if (col_note->type != TSDB_DATA_TYPE_VARCHAR) {
    stmt_append_err_format(columns->owner, "HY000", 0, "General error:internal logic error:`%s`", taos_data_type(col_note->type));
    return SQL_ERROR;
  }

  TAOS_FIELD fake = {0};
  int n = snprintf(fake.name, sizeof(fake.name), "%.*s", (int)col_name->str.len, col_name->str.str);
  if (n < 0 || (size_t)n >= sizeof(fake.name)) {
    stmt_append_err_format(columns->owner, "HY000", 0, "General error:buffer too small to hold `%.*s`", (int)col_name->str.len, col_name->str.str);
    return SQL_ERROR;
  }

  static const struct {
    const char *s;
    int         tsdb_type;
    int         bytes;
  } supported[] = {
    {"TIMESTAMP",                 TSDB_DATA_TYPE_TIMESTAMP,           8},
    {"VARCHAR",                   TSDB_DATA_TYPE_VARCHAR,            -1},
    {"NCHAR",                     TSDB_DATA_TYPE_NCHAR,              -1},
    {"BOOL",                      TSDB_DATA_TYPE_BOOL,                1},
    {"TINYINT",                   TSDB_DATA_TYPE_TINYINT,             1},
    {"SMALLINT",                  TSDB_DATA_TYPE_SMALLINT,            2},
    {"INT",                       TSDB_DATA_TYPE_INT,                 4},
    {"BIGINT",                    TSDB_DATA_TYPE_BIGINT,              8},
    {"FLOAT",                     TSDB_DATA_TYPE_FLOAT,               4},
    {"DOUBLE",                    TSDB_DATA_TYPE_DOUBLE,              8},
    {"TINYINT UNSIGNED",          TSDB_DATA_TYPE_UTINYINT,            1},
    {"SMALLINT UNSIGNED",         TSDB_DATA_TYPE_USMALLINT,           2},
    {"INT UNSIGNED",              TSDB_DATA_TYPE_UINT,                4},
    {"BIGINT UNSIGNED",           TSDB_DATA_TYPE_UBIGINT,             8},
  };

  for (size_t i=0; i<=sizeof(supported)/sizeof(supported[0]); ++i) {
    if (i == sizeof(supported)/sizeof(supported[0])) {
      stmt_append_err_format(columns->owner, "HY000", 0, "General error:not implemented yet for column[%d] `%.*s`",
          Col_or_Param_Num, (int)col_type->str.len, col_type->str.str);
      return SQL_ERROR;
    }
    const char *s = supported[i].s;
    size_t len = strlen(s);
    int tsdb_type = supported[i].tsdb_type;
    int bytes = supported[i].bytes;
    if (col_type->str.len != len) continue;
    if (strncmp(col_type->str.str, s, len)) continue;
    fake.type = tsdb_type;
    fake.bytes = col_length->i32;
    if (bytes != -1 && fake.bytes != bytes) {
      stmt_append_err_format(columns->owner, "HY000", 0, "General error:internal logic error for Column[%d] `%.*s`, ColumnSize is expected %d, but got ==%d==",
          Col_or_Param_Num, (int)col_type->str.len, col_type->str.str, bytes, fake.bytes);
      return SQL_ERROR;
    }
    break;
  }

  tsdb->is_null = 0;

  switch (Col_or_Param_Num) {
    case 1: // TABLE_CAT
      // better approach?
      tsdb->type = TSDB_DATA_TYPE_VARCHAR;
      tsdb->str  = columns->current_catalog.str;
      OW("TABLE_CAT:[%.*s]", (int)tsdb->str.len, tsdb->str.str);
      break;
    case 2: // TABLE_SCHEM
      // better approach?
      tsdb->type = TSDB_DATA_TYPE_VARCHAR;
      tsdb->str  = columns->current_schema.str;
      OW("TABLE_SCHEM:[%.*s]", (int)tsdb->str.len, tsdb->str.str);
      break;
    case 3: // TABLE_NAME
      // better approach?
      tsdb->type = TSDB_DATA_TYPE_VARCHAR;
      tsdb->str  = columns->current_table.str;
      OW("TABLE_NAME:[%.*s]", (int)tsdb->str.len, tsdb->str.str);
      break;
    case 4: // COLUMN_NAME
      // better approach?
      tsdb->type = TSDB_DATA_TYPE_VARCHAR;
      tsdb->str  = col_name->str;
      OW("COLUMN_NAME:[%.*s]", (int)tsdb->str.len, tsdb->str.str);
      break;
    case 5: // DATA_TYPE
      {
        static const struct {
          int               tsdb_type;
          int16_t           sql_type;
        } supported[] = {
          {TSDB_DATA_TYPE_TIMESTAMP,          SQL_TYPE_TIMESTAMP},
          {TSDB_DATA_TYPE_VARCHAR,            SQL_VARCHAR},
          {TSDB_DATA_TYPE_NCHAR,              SQL_WVARCHAR},
          {TSDB_DATA_TYPE_BOOL,               SQL_BIT},
          {TSDB_DATA_TYPE_TINYINT,            SQL_TINYINT},
          {TSDB_DATA_TYPE_SMALLINT,           SQL_SMALLINT},
          {TSDB_DATA_TYPE_INT,                SQL_INTEGER},
          {TSDB_DATA_TYPE_BIGINT,             SQL_BIGINT},
          {TSDB_DATA_TYPE_FLOAT,              SQL_REAL},
          {TSDB_DATA_TYPE_DOUBLE,             SQL_DOUBLE},
          {TSDB_DATA_TYPE_UTINYINT,           SQL_TINYINT},
          {TSDB_DATA_TYPE_USMALLINT,          SQL_SMALLINT},
          {TSDB_DATA_TYPE_UINT,               SQL_INTEGER},
          {TSDB_DATA_TYPE_UBIGINT,            SQL_BIGINT},
        };

        for (size_t i=0; i<=sizeof(supported)/sizeof(supported[0]); ++i) {
          if (i == sizeof(supported)/sizeof(supported[0])) {
            stmt_append_err_format(columns->owner, "HY000", 0, "General error:not implemented yet for column[%d] `%.*s`",
                Col_or_Param_Num, (int)col_type->str.len, col_type->str.str);
            return SQL_ERROR;
          }
          int tsdb_type = supported[i].tsdb_type;
          int16_t sql_type = supported[i].sql_type;
          if (fake.type != tsdb_type) continue;
          tsdb->type = TSDB_DATA_TYPE_SMALLINT;
          tsdb->i16 = sql_type;
          break;
        }
        break;
      }
    case 6: // TYPE_NAME
      // better approach?
      tsdb->type = TSDB_DATA_TYPE_VARCHAR;
      tsdb->str  = col_type->str;
      OW("TYPE_NAME:[%.*s]", (int)tsdb->str.len, tsdb->str.str);
      break;
    case 7: // COLUMN_SIZE
      // better approach?
      tsdb->type = TSDB_DATA_TYPE_INT;
      tsdb->i32  = col_length->i32;
      OW("COLUMN_SIZE:[%d]", tsdb->i32);
      break;
    case 8: // BUFFER_LENGTH
      {
        static const struct {
          int               tsdb_type;
          int               len;
        } supported[] = {
          {TSDB_DATA_TYPE_TIMESTAMP,          8},
          {TSDB_DATA_TYPE_VARCHAR,            -1},
          {TSDB_DATA_TYPE_NCHAR,              -1},
          {TSDB_DATA_TYPE_BOOL,               1},
          {TSDB_DATA_TYPE_TINYINT,            1},
          {TSDB_DATA_TYPE_SMALLINT,           2},
          {TSDB_DATA_TYPE_INT,                4},
          {TSDB_DATA_TYPE_BIGINT,             8},
          {TSDB_DATA_TYPE_FLOAT,              4},
          {TSDB_DATA_TYPE_DOUBLE,             8},
          {TSDB_DATA_TYPE_UTINYINT,           1},
          {TSDB_DATA_TYPE_USMALLINT,          2},
          {TSDB_DATA_TYPE_UINT,               4},
          {TSDB_DATA_TYPE_UBIGINT,            8},
        };

        for (size_t i=0; i<=sizeof(supported)/sizeof(supported[0]); ++i) {
          if (i == sizeof(supported)/sizeof(supported[0])) {
            stmt_append_err_format(columns->owner, "HY000", 0, "General error:not implemented yet for column[%d] `%.*s`",
                Col_or_Param_Num, (int)col_type->str.len, col_type->str.str);
            return SQL_ERROR;
          }
          int tsdb_type = supported[i].tsdb_type;
          int len = supported[i].len;
          if (fake.type != tsdb_type) continue;
          tsdb->type = TSDB_DATA_TYPE_INT;
          tsdb->i32 = len == -1 ? fake.bytes : len;
          break;
        }
        break;
      }
    case 9: // DECIMAL_DIGITS
      if (fake.type == TSDB_DATA_TYPE_TIMESTAMP) {
        tsdb->type = TSDB_DATA_TYPE_INT;
        tsdb->i32  = 3; // FIXME:
        OW("DECIMAL_DIGITS:[%d]", tsdb->i32);
        break;
      }
      tsdb->type = TSDB_DATA_TYPE_INT;
      tsdb->i32  = 0;
      OW("DECIMAL_DIGITS:[%d]", tsdb->i32);
      break;
    case 10: // NUM_PREC_RADIX
      tsdb->type = TSDB_DATA_TYPE_INT;
      tsdb->i32  = 10;
      OW("NUM_PREC_RADIX:[%d]", tsdb->i32);
      break;
    case 11: // NULLABLE
      tsdb->type = TSDB_DATA_TYPE_INT;
      tsdb->i32  = SQL_NULLABLE_UNKNOWN;
      OW("NULLABLE:[%d]", tsdb->i32);
      break;
    case 12: // REMARKS
      tsdb->type = TSDB_DATA_TYPE_VARCHAR;
      tsdb->str  = col_note->str;
      tsdb->is_null = col_note->is_null;
      OW("REMARKS:[%.*s]", tsdb->is_null ? 4 : (int)tsdb->str.len, tsdb->is_null ? "null" : tsdb->str.str);
      break;
    case 13: // COLUMN_DEF
      tsdb->type = TSDB_DATA_TYPE_VARCHAR;
      tsdb->str.len = 0;
      tsdb->str.str = "";
      tsdb->is_null = 1;
      OW("COLUMN_DEF:[%s]", "null");
      break;
    case 14: // SQL_DATA_TYPE
      {
        static const struct {
          int               tsdb_type;
          int               sql_type; // non-concise-type
        } supported[] = {
          {TSDB_DATA_TYPE_TIMESTAMP,          SQL_TIMESTAMP},
          {TSDB_DATA_TYPE_VARCHAR,            SQL_VARCHAR},
          {TSDB_DATA_TYPE_NCHAR,              SQL_WVARCHAR},
          {TSDB_DATA_TYPE_BOOL,               SQL_BIT},
          {TSDB_DATA_TYPE_TINYINT,            SQL_TINYINT},
          {TSDB_DATA_TYPE_SMALLINT,           SQL_SMALLINT},
          {TSDB_DATA_TYPE_INT,                SQL_INTEGER},
          {TSDB_DATA_TYPE_BIGINT,             SQL_BIGINT},
          {TSDB_DATA_TYPE_FLOAT,              SQL_REAL},
          {TSDB_DATA_TYPE_DOUBLE,             SQL_DOUBLE},
          {TSDB_DATA_TYPE_UTINYINT,           SQL_TINYINT},
          {TSDB_DATA_TYPE_USMALLINT,          SQL_SMALLINT},
          {TSDB_DATA_TYPE_UINT,               SQL_INTEGER},
          {TSDB_DATA_TYPE_UBIGINT,            SQL_BIGINT},
        };

        for (size_t i=0; i<=sizeof(supported)/sizeof(supported[0]); ++i) {
          if (i == sizeof(supported)/sizeof(supported[0])) {
            stmt_append_err_format(columns->owner, "HY000", 0, "General error:not implemented yet for column[%d] `%.*s`",
                Col_or_Param_Num, (int)col_type->str.len, col_type->str.str);
            return SQL_ERROR;
          }
          int tsdb_type = supported[i].tsdb_type;
          int16_t sql_type = supported[i].sql_type;
          if (fake.type != tsdb_type) continue;
          tsdb->type = TSDB_DATA_TYPE_INT;
          tsdb->i32 = sql_type;
          break;
        }
        break;
      }
    case 15: // SQL_DATETIME_SUB
      tsdb->type = TSDB_DATA_TYPE_INT;
      tsdb->i32  = 0;
      tsdb->is_null = 1;
      OW("SQL_DATATIME_SUB:[%s]", "null");
      break;
    case 16: // CHAR_OCTET_LENGTH
      if (fake.type == TSDB_DATA_TYPE_VARCHAR) {
        tsdb->type = TSDB_DATA_TYPE_INT;
        tsdb->i32  = fake.bytes;
        OW("CHAR_OCTET_LENGTH:[%d]", tsdb->i32);
      } else {
        tsdb->type = TSDB_DATA_TYPE_INT;
        tsdb->i32  = 0;
        tsdb->is_null = 1;
        OW("CHAR_OCTET_LENGTH:[%s]", "null");
      }
      break;
    case 17: // ORDINAL_POSITION
      tsdb->type = TSDB_DATA_TYPE_INT;
      tsdb->i32  = columns->ordinal_order;
      OW("ORDINAL_POSITION:[%d]", tsdb->i32);
      break;
    case 18: // IS_NULLABLE
      tsdb->type = TSDB_DATA_TYPE_VARCHAR;
      tsdb->str.str = "";
      tsdb->str.len = 0;
      OW("IS_NULLABLE:[%s]", "");
      break;
    default:
      stmt_append_err_format(columns->owner, "HY000", 0, "General error:not implemented yet for column[%d]", Col_or_Param_Num);
      return SQL_ERROR;
  }

  return SQL_SUCCESS;
}

void columns_init(columns_t *columns, stmt_t *stmt)
{
  columns->owner = stmt;
  tables_init(&columns->tables, stmt);
  tsdb_stmt_init(&columns->desc, stmt);
  columns->base.query                        = _query;
  columns->base.execute                      = _execute;
  columns->base.get_fields                   = _get_fields;
  columns->base.fetch_row                    = _fetch_row;
  columns->base.describe_param               = _describe_param;
  columns->base.describe_col                 = _describe_col;
  columns->base.get_num_params               = _get_num_params;
  columns->base.check_params                 = _check_params;
  columns->base.tsdb_field_by_param          = _tsdb_field_by_param;
  columns->base.row_count                    = _row_count;
  columns->base.get_num_cols                 = _get_num_cols;
  columns->base.get_data                     = _get_data;
}

SQLRETURN columns_open(
    columns_t      *columns,
    SQLCHAR       *CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR       *SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR       *TableName,
    SQLSMALLINT    NameLength3,
    SQLCHAR       *ColumnName,
    SQLSMALLINT    NameLength4)
{
  SQLRETURN sr = SQL_SUCCESS;

  columns_reset(columns);

  if (CatalogName && NameLength1 == SQL_NTS) NameLength1 = (SQLSMALLINT)strlen((const char*)CatalogName);
  if (SchemaName && NameLength2 == SQL_NTS)  NameLength2 = (SQLSMALLINT)strlen((const char*)SchemaName);
  if (TableName && NameLength3 == SQL_NTS)   NameLength3 = (SQLSMALLINT)strlen((const char*)TableName);
  if (ColumnName && NameLength4 == SQL_NTS)  NameLength4 = (SQLSMALLINT)strlen((const char*)ColumnName);

  OW("CatalogName:%p,%.*s", CatalogName, (int)NameLength1, CatalogName);
  OW("SchemaName:%p,%.*s", SchemaName, (int)NameLength2, SchemaName);
  OW("TableName:%p,%.*s", TableName, (int)NameLength3, TableName);
  OW("ColumnName:%p,%.*s", ColumnName, (int)NameLength4, ColumnName);

  charset_conv_t *cnv = &columns->owner->conn->cnv_sql_c_char_to_sql_c_wchar;
  if (ColumnName) {
    if (mem_conv(&columns->column_cache, cnv->cnv, (const char*)ColumnName, NameLength4)) {
      stmt_oom(columns->owner);
      return SQL_ERROR;
    }
    if (wildcomp_n_ex(&columns->columns_args.column_pattern, cnv->from, (const char*)ColumnName, NameLength4)) {
      stmt_append_err_format(columns->owner, "HY000", 0,
          "General error:wild compile failed for ColumnName[%.*s]", (int)NameLength4, (const char*)ColumnName);
      return SQL_ERROR;
    }
  }

  sr = tables_open(&columns->tables, CatalogName, NameLength1, SchemaName, NameLength2, TableName, NameLength3, (SQLCHAR*)"TABLE", SQL_NTS);
  if (sr != SQL_SUCCESS) return SQL_ERROR;

  return _fetch_and_desc_next_table(columns);
}
