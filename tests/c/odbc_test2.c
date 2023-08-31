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

#include "odbc_helpers.h"
#include "c_test_helper.h"

#include "enums.h"

#include <assert.h>
#ifndef _WIN32
#include <dlfcn.h>
#include <unistd.h>
#endif
#include <errno.h>
#include <sql.h>
#include <sqlext.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <taos.h>

struct test_context {
  SQLHANDLE henv;
  SQLHANDLE hconn;
  SQLHANDLE hstmt;

  char buf[1024];
  simple_str_t sql_str;
};

typedef struct {
  struct test_context ctx;
  const char* test_name;
  const char* connstr;
  const char* dsn;
  const char* user;
  const char* pwd;
  bool  valid;
} data_source_case;

#define LINKMODENATIVE "native"
#define LINKMODEWS "ws"
#define LINKMODECLOUD "cloud"
#define LINKMODEMYSQL "mysql"
#define LINKMODESQLSERVER "ss"
data_source_case _native_link = {
    {
      .henv = SQL_NULL_HANDLE,
      .hconn = SQL_NULL_HANDLE,
      .hstmt = SQL_NULL_HANDLE,
    },
    LINKMODENATIVE,
    "DSN=TAOS_ODBC_DSN;Server=192.168.1.93:6030;Uid=root;pwd=taosdata",
    "TAOS_ODBC_DSN",
    NULL,
    NULL,
    false
};
data_source_case _websocket_link = {
    {
      .henv = SQL_NULL_HANDLE,
      .hconn = SQL_NULL_HANDLE,
      .hstmt = SQL_NULL_HANDLE,
    },
    LINKMODEWS,
    "DSN=TAOS_ODBC_WS_DSN;Server=192.168.1.93:6041",
    "TAOS_ODBC_WS_DSN",
    NULL,
    NULL,
    false
};
data_source_case _cloud_link = {
    {
      .henv = SQL_NULL_HANDLE,
      .hconn = SQL_NULL_HANDLE,
      .hstmt = SQL_NULL_HANDLE,
    },
    LINKMODECLOUD,
    "DSN=TAOS_CLOUD;url={taos+wss://gw.us-east-1.aws.cloud.tdengine.com/meter?token=bf55680e8b4bd94e48401bb768c312e88805bb73}",
    "TAOS_CLOUD",
    NULL,
    NULL,
    false
};
data_source_case _mysql_link = {
  {
    .henv = SQL_NULL_HANDLE,
    .hconn = SQL_NULL_HANDLE,
    .hstmt = SQL_NULL_HANDLE,
  },
  LINKMODEMYSQL,
  "DSN=MYSQL_ODBC",
  "MYSQL_ODBC",
  NULL,
  NULL,
  false
};
data_source_case _sqlserver_link = {
  {
    .henv = SQL_NULL_HANDLE,
    .hconn = SQL_NULL_HANDLE,
    .hstmt = SQL_NULL_HANDLE,
  },
    LINKMODESQLSERVER,
    "DSN=SQLSERVER_ODBC_DSN",
    "SQLSERVER_ODBC_DSN",
    "sa",
    "Aa123456",
    false
};
data_source_case* link_info;

#define MAX_TABLE_NUMBER 100
#define MAX_TABLE_NAME_LEN 128
#define test_db "meter"

#define count_10 10
#define count_100 100
#define count_1000 1000

#define CHKENVR(henv, r)  do {                                                                                                   \
   if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO)                                                                           \
   {                                                                                                                             \
     SQLCHAR sqlState[6];                                                                                                        \
     SQLCHAR message[SQL_MAX_MESSAGE_LENGTH];                                                                                    \
     SQLSMALLINT messageLength;                                                                                                  \
     SQLINTEGER nativeError;                                                                                                     \
     if(SQLGetDiagRec(SQL_HANDLE_ENV, henv, 1, sqlState, &nativeError, message, sizeof(message), &messageLength)  != 100)        \
     {                                                                                                                           \
       E("Error occurred : %s", message);                                                                        \
     }                                                                                                                           \
     return -1;                                                                                                                  \
   }                                                                                                                             \
} while (0)                                                                                                                      \

#define CHKDBR(hconn, r)  do {                                                                                                   \
  if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO)                                                                            \
  {                                                                                                                              \
    SQLCHAR sqlState[6];                                                                                                         \
    SQLCHAR message[SQL_MAX_MESSAGE_LENGTH];                                                                                     \
    SQLSMALLINT messageLength;                                                                                                   \
    SQLINTEGER nativeError;                                                                                                      \
    if(SQLGetDiagRec(SQL_HANDLE_DBC, hconn, 1, sqlState, &nativeError, message, sizeof(message), &messageLength)  != 100)        \
    {                                                                                                                            \
      E("error: %s", message);                                                                                                   \
    }                                                                                                                            \
    return -1;                                                                                                                   \
  }                                                                                                                              \
} while (0)

#define CHKSTMTR(hstmt, r)  do {                                                                                                 \
   if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO)                                                                           \
   {                                                                                                                             \
     SQLCHAR sqlState[6];                                                                                                        \
     SQLCHAR message[SQL_MAX_MESSAGE_LENGTH]={0};                                                                                \
     SQLSMALLINT messageLength;                                                                                                  \
     SQLINTEGER nativeError;                                                                                                     \
     if(SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlState, &nativeError, message, sizeof(message), &messageLength)  != 100)      \
     {                                                                                                                           \
       E("error: %s", message);                                                                                                  \
     }                                                                                                                           \
     return -1;                                                                                                                  \
   }                                                                                                                             \
} while (0)  


#define ASSERT_EQUAL(actual, expected)                                                               \
    do {                                                                                             \
        if ((expected) != (actual)) {                                                                \
            printf("Assertion failed: Expected %d, but got %d\n", (expected), (actual));             \
            return -1;                                                                               \
        }                                                                                            \
    } while(0)

#define ASSERT_EQUALS(actual, expected)                                                              \
    do {                                                                                             \
        if (strcmp(actual, expected) != 0) {                                                         \
            printf("Assertion failed: Expected %s, but got %s\n", (expected), (actual));             \
            return -1;                                                                               \
        }                                                                                            \
    } while(0)


const char* t_table_create = "CREATE TABLE if not exists `t_table` (`ts` TIMESTAMP, `double_val` DOUBLE, `int_val` INT)";

static void get_diag_rec(SQLSMALLINT handleType, SQLHANDLE h) {

  SQLRETURN r;
  SQLCHAR sqlState[6];
  SQLINTEGER nativeError;
  SQLCHAR messageText[SQL_MAX_MESSAGE_LENGTH];
  SQLSMALLINT textLength;

  r = SQLGetDiagRec(handleType, h, 1, sqlState, &nativeError, messageText, sizeof(messageText), &textLength);
  X("SQLGetDiagRec result: %d", r);
  X("SQLState: %s", sqlState);
  X("Native Error: %d", nativeError);
  X("Message Text: %s", messageText);
}

static int init_henv(void) {
  SQLRETURN r;
  r = CALL_SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &link_info->ctx.henv);
  if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;

  r = SQLSetEnvAttr(link_info->ctx.henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
  if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;

  r = SQLSetEnvAttr(link_info->ctx.henv, SQL_ATTR_CONNECTION_POOLING, (SQLPOINTER)SQL_CP_ONE_PER_DRIVER, 0);
  if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;

  return 0;
}

static bool is_sql_server_test(void) {
  return strcmp(link_info->test_name, LINKMODESQLSERVER) == 0;
}

static int create_sql_connect(void) {
  int r = 0;

  r = CALL_SQLAllocHandle(SQL_HANDLE_DBC, link_info->ctx.henv, &link_info->ctx.hconn);
  if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;

  D("connect dsn:%s user:%s pwd:%s", link_info->dsn, link_info->user, link_info->pwd);
  r = CALL_SQLConnect(link_info->ctx.hconn, (SQLCHAR*)link_info->dsn, SQL_NTS, (SQLCHAR*)link_info->user, SQL_NTS, (SQLCHAR*)link_info->pwd, SQL_NTS);
  if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;
  D("connect dsn:%s result:%d", link_info->dsn, r);

  return 0;
}

static int free_connect(void) {
  int r = 0;
  if (link_info->ctx.hstmt != SQL_NULL_HANDLE) {
    r = CALL_SQLFreeHandle(SQL_HANDLE_STMT, link_info->ctx.hstmt);
    if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;

    link_info->ctx.hstmt = SQL_NULL_HANDLE;
  }

  if (link_info->ctx.hconn != SQL_NULL_HANDLE) {
    X("disconnect dsn:%s", link_info->dsn);
    r = CALL_SQLDisconnect(link_info->ctx.hconn);
    if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return 0;
    X("disconnect dsn:%s result:%d", link_info->dsn, r);

    r = CALL_SQLFreeHandle(SQL_HANDLE_DBC, link_info->ctx.hconn);
    if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;

    link_info->ctx.hconn = SQL_NULL_HANDLE;
  }
  return 0;
}

static int create_driver_conn(void)
{
  int r = 0;
  r = CALL_SQLAllocHandle(SQL_HANDLE_DBC, link_info->ctx.henv, &link_info->ctx.hconn);
  if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;

  SQLHWND WindowHandle = NULL;
  SQLCHAR* InConnectionString = (SQLCHAR*)link_info->connstr;
  SQLSMALLINT StringLength1 = (SQLSMALLINT)strlen(link_info->connstr);
  SQLCHAR OutConnectionString[1024];
  SQLSMALLINT BufferLength = sizeof(OutConnectionString);
  SQLSMALLINT StringLength2 = 0;
  SQLUSMALLINT DriverCompletion = SQL_DRIVER_NOPROMPT;

  OutConnectionString[0] = '\0';

  r = CALL_SQLDriverConnect(link_info->ctx.hconn, WindowHandle, InConnectionString, StringLength1, OutConnectionString, BufferLength, &StringLength2, DriverCompletion);
  if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;
  return 0;
}

static int reset_stmt(void) {
  int r = 0;
  if (link_info->ctx.hstmt != SQL_NULL_HANDLE) {
    r = CALL_SQLFreeHandle(SQL_HANDLE_STMT, link_info->ctx.hstmt);
    if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;
  }
  link_info->ctx.hstmt = SQL_NULL_HANDLE;
  r = CALL_SQLAllocHandle(SQL_HANDLE_STMT, link_info->ctx.hconn, &link_info->ctx.hstmt);
  if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;

  return 0;
}

static int exec_sql(const char* sql) {
  int r = 0;
  CHK0(reset_stmt, 0);
  SQLHANDLE hstmt = link_info->ctx.hstmt;
  r = CALL_SQLExecDirect(hstmt, (SQLCHAR*)sql, SQL_NTS);
  D("CALL_SQLExecDirect: %s result:%d", sql, r);
  if (r == SQL_SUCCESS_WITH_INFO) {
    get_diag_rec(SQL_HANDLE_STMT, hstmt);
  }
  CHKSTMTR(hstmt, r);
  if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;
  return 0;
}

static int drop_database(char* db) {
  char sql[128];
  memset(sql, 0, sizeof(sql));
  strcat(sql, "drop database if exists ");
  strcat(sql, db);
  strcat(sql, ";");
  CHK1(exec_sql, sql, 0);
  X("drop database %s finished", db);
  return 0;
}

static int create_database(char* db) {
  if (is_sql_server_test()) return 0;
  char sql[128];
  memset(sql, 0, sizeof(sql));
  strcat(sql, "create database if not exists ");
  strcat(sql, db);
  strcat(sql, " precision 'us'");
  CHK1(exec_sql, sql, 0);
  X("create database %s finished", db);
  return 0;
}

static int get_tables(SQLCHAR table_names[MAX_TABLE_NUMBER][MAX_TABLE_NAME_LEN]) {
  int r = 0;
  CHK0(reset_stmt, 0);

  int table_count = 0;

  SQLHANDLE hstmt = link_info->ctx.hstmt;
  SQLCHAR* tableType = (SQLCHAR*)"TABLE";
  r = SQLTables(hstmt, NULL, 0, NULL, 0, NULL, 0, tableType, SQL_NTS);
  X("SQLTables result:%d", r);
  while (CALL_SQLFetch(hstmt) == SQL_SUCCESS) {
    CALL_SQLGetData(hstmt, 3, SQL_C_CHAR, table_names[table_count], MAX_TABLE_NAME_LEN, NULL);
    table_count++;
  }
  X("table count:%d", table_count);
  return table_count;
}

static int show_tables(void) {
  CHK0(reset_stmt, 0);

  SQLCHAR table_names[MAX_TABLE_NUMBER][MAX_TABLE_NAME_LEN];
  int table_count = get_tables(table_names);

  for (int j = 0; j < table_count; j++) {
    X("table name: %s", table_names[j]);
  }
  return 0;
}

static int init(void) {
  CHK0(init_henv, 0);
  CHK0(create_sql_connect, 0);
  CHK1(create_database, test_db, 0);
  CHK0(free_connect, 0);

  X("init finished!");
  return 0;
}

static int db_test(void) {
  CHK0(create_driver_conn, 0);
  CHK0(show_tables, 0);

  CHK0(free_connect, 0);
  X("basic test finished!");
  return 0;
}

static int use_db(char* db) {
  char sql[1024];
  strcpy(sql, "use ");
  strcat(sql, db);
  CHK1(exec_sql, sql, 0);
  return 0;
}

static int show_columns1(char* table_name) {
#define STR_LEN 128 + 1
#define REM_LEN 254 + 1
  // Declare buffers for result set data
  SQLCHAR strSchema[STR_LEN];
  SQLCHAR strCatalog[STR_LEN];
  SQLCHAR strColumnName[STR_LEN];
  SQLCHAR strTableName[STR_LEN];
  SQLCHAR strTypeName[STR_LEN];
  SQLCHAR strRemarks[REM_LEN];
  SQLCHAR strColumnDefault[STR_LEN];
  SQLCHAR strIsNullable[STR_LEN];

  SQLINTEGER ColumnSize;
  SQLINTEGER BufferLength;
  SQLINTEGER CharOctetLength;
  SQLINTEGER OrdinalPosition;

  SQLSMALLINT DataType;
  SQLSMALLINT DecimalDigits;
  SQLSMALLINT NumPrecRadix;
  SQLSMALLINT Nullable;
  SQLSMALLINT SQLDataType;
  SQLSMALLINT DatetimeSubtypeCode;

  SQLLEN lenCatalog;
  SQLLEN lenSchema;
  SQLLEN lenTableName;
  SQLLEN lenColumnName;
  SQLLEN lenDataType;
  SQLLEN lenTypeName;
  SQLLEN lenColumnSize;
  SQLLEN lenBufferLength;
  SQLLEN lenDecimalDigits;
  SQLLEN lenNumPrecRadix;
  SQLLEN lenNullable;
  SQLLEN lenRemarks;
  SQLLEN lenColumnDefault;
  SQLLEN lenSQLDataType;
  SQLLEN lenDatetimeSubtypeCode;
  SQLLEN lenCharOctetLength;
  SQLLEN lenOrdinalPosition;
  SQLLEN lenIsNullable;

  SQLRETURN retcode;
  CHK0(reset_stmt, 0);
  SQLHANDLE hstmt = link_info->ctx.hstmt;

  retcode = CALL_SQLColumns(hstmt, NULL, 0, NULL, 0,
    (SQLCHAR*)table_name, SQL_NTS, NULL, 0);

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
    SQLBindCol(hstmt, 1, SQL_C_CHAR, strCatalog,
      STR_LEN, &lenCatalog);
    SQLBindCol(hstmt, 2, SQL_C_CHAR, strSchema,
      STR_LEN, &lenSchema);
    SQLBindCol(hstmt, 3, SQL_C_CHAR, strTableName,
      STR_LEN, &lenTableName);
    SQLBindCol(hstmt, 4, SQL_C_CHAR, strColumnName,
      STR_LEN, &lenColumnName);
    SQLBindCol(hstmt, 5, SQL_C_SSHORT, &DataType,
      0, &lenDataType);
    SQLBindCol(hstmt, 6, SQL_C_CHAR, strTypeName,
      STR_LEN, &lenTypeName);
    SQLBindCol(hstmt, 7, SQL_C_SLONG, &ColumnSize,
      0, &lenColumnSize);
    SQLBindCol(hstmt, 8, SQL_C_SLONG, &BufferLength,
      0, &lenBufferLength);
    SQLBindCol(hstmt, 9, SQL_C_SSHORT, &DecimalDigits,
      0, &lenDecimalDigits);
    SQLBindCol(hstmt, 10, SQL_C_SSHORT, &NumPrecRadix,
      0, &lenNumPrecRadix);
    SQLBindCol(hstmt, 11, SQL_C_SSHORT, &Nullable,
      0, &lenNullable);
    SQLBindCol(hstmt, 12, SQL_C_CHAR, strRemarks,
      REM_LEN, &lenRemarks);
    SQLBindCol(hstmt, 13, SQL_C_CHAR, strColumnDefault,
      STR_LEN, &lenColumnDefault);
    SQLBindCol(hstmt, 14, SQL_C_SSHORT, &SQLDataType,
      0, &lenSQLDataType);
    SQLBindCol(hstmt, 15, SQL_C_SSHORT, &DatetimeSubtypeCode,
      0, &lenDatetimeSubtypeCode);
    SQLBindCol(hstmt, 16, SQL_C_SLONG, &CharOctetLength,
      0, &lenCharOctetLength);
    SQLBindCol(hstmt, 17, SQL_C_SLONG, &OrdinalPosition,
      0, &lenOrdinalPosition);
    SQLBindCol(hstmt, 18, SQL_C_CHAR, strIsNullable,
      STR_LEN, &lenIsNullable);

    // retrieve column data
    while (SQL_SUCCESS == retcode) {
      retcode = CALL_SQLFetch(hstmt);
      CHKSTMTR(hstmt, retcode);
      X("strCatalog  : %s", strCatalog);
      X("strSchema   : %s", strSchema);
      X("strTableName: %s", strTableName);
      X("Column Name : %s", strColumnName);
      X("Column Size : %i", ColumnSize);
      X("Data Type   : %i", SQLDataType);
      X("strTypeName : %s", strTypeName);
      X("Nullable    : %d", Nullable);
    }
  }
  return 0;
}

static int show_table_data(char* table_name) {
  reset_stmt();
  SQLHANDLE hstmt = link_info->ctx.hstmt;
  SQLCHAR sql[1024];
  SQLSMALLINT numberOfColumns;
  sprintf((char *)sql, "SELECT * FROM %s", table_name);
  CALL_SQLExecDirect(hstmt, sql, SQL_NTS);
  CALL_SQLNumResultCols(hstmt, &numberOfColumns);

  int row = 0;
  SQLCHAR columnData[256];
  SQLLEN indicator;
  while (CALL_SQLFetch(hstmt) == SQL_SUCCESS) {
    row++;
    for (int i = 1; i <= numberOfColumns; i++) {
      CALL_SQLGetData(hstmt, i, SQL_C_CHAR, columnData, sizeof(columnData), &indicator);
      if (indicator == SQL_NULL_DATA) {
        X("Row:%d Column %d: NULL", row, i);
      }
      else {
        X("Row:%d Column %d: %s", row, i, columnData);
      }
    }
  }
  return row;
}

static int fetch_scroll_test(char* table_name) {
  reset_stmt();
  SQLHANDLE hstmt = link_info->ctx.hstmt;

  SQLSMALLINT numberOfColumns;
  SQLULEN    numRowsFetched;         /* Number of rows fetched */

  SQLCHAR sql[1024];
  sprintf((char *)sql, "SELECT * FROM %s limit 110", table_name);

  CALL_SQLExecDirect(hstmt, sql, SQL_NTS);
  CALL_SQLNumResultCols(hstmt, &numberOfColumns);

#define ROWS 100
#define TS_LEN 128
  SQLULEN row = 0;
  SQLRETURN ret;

  ret = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_STATIC, SQL_IS_INTEGER);
  X("CALL_SQLSetStmtAttr SQL_ATTR_CURSOR_TYPE result=%d", ret);

  SQLCHAR        ts[ROWS][TS_LEN];       /* Record set */
  SQLLEN         orind[ROWS];            /* Len or status ind */

  SQLUSMALLINT   rowStatus[ROWS];        /* Status of each row */

  ret = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
    (SQLPOINTER)ROWS, 0);
  X("CALL_SQLSetStmtAttr SQL_ATTR_ROW_ARRAY_SIZE result=%d", ret);

  ret = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_STATUS_PTR,
    (SQLPOINTER)rowStatus, 0);
  X("CALL_SQLSetStmtAttr SQL_ATTR_ROW_STATUS_PTR result=%d", ret);

  ret = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_ROWS_FETCHED_PTR,
    (SQLPOINTER)&numRowsFetched, 0);
  X("CALL_SQLSetStmtAttr SQL_ATTR_ROWS_FETCHED_PTR result=%d", ret);

  SQLULEN maxRows = 90;
  CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER)maxRows, SQL_IS_UINTEGER);

  SQLBindCol(hstmt,   /* Statement handle */
    1,                /* Column number */
    SQL_C_CHAR,       /* Bind to a C string */
    ts,               /* The data to be fetched */
    TS_LEN,           /* Maximum length of the data */
    orind);           /* Status or length indicator */

  while ((ret = CALL_SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
    for (SQLUINTEGER i = 0; i < numRowsFetched; i++)
    {
      D("ts: %s", ts[i]);
    }
    row += numRowsFetched;
    X("numRowsFetched "SQLLEN_FORMAT" : "SQLLEN_FORMAT"", row, numRowsFetched);
  }

  if (ret == SQL_NO_DATA) {
    X("No more rows");
  }
  else if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
    SQLCHAR sqlState[6], message[256];
    SQLINTEGER nativeError;
    SQLSMALLINT textLength;
    SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlState, &nativeError, message, sizeof(message), &textLength);
    X("Error: SQLSTATE=%s, Native Error=%d, Message=%s\n", sqlState, nativeError, message);
    return -1;
  }

  X("Has got "SQLLEN_FORMAT" rows, exit...", row);
  return (int)row;
}

static int more_result_test(char* table_name) {
  reset_stmt();
  SQLHANDLE hstmt = link_info->ctx.hstmt;
  SQLULEN    numRowsFetched;
  SQLULEN    row = 0;
  SQLRETURN  ret;

  SQLCHAR sql[1024];
  sprintf((char *)sql, "SELECT ts FROM %s limit 12; SELECT ts, double_val FROM %s limit 25;", table_name, table_name);

  CALL_SQLExecDirect(hstmt, sql, SQL_NTS);

  ret = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_ROWS_FETCHED_PTR,
    (SQLPOINTER)&numRowsFetched, 0);
  X("CALL_SQLSetStmtAttr SQL_ATTR_ROWS_FETCHED_PTR result=%d", ret);

  ret = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
    (SQLPOINTER)10, 0);
  X("CALL_SQLSetStmtAttr SQL_ATTR_ROW_ARRAY_SIZE result=%d", ret);

  do {
    while ((ret = CALL_SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
      for (SQLUINTEGER i = 0; i < numRowsFetched; i++)
      {
        X("row: %d", i);
      }
      row += numRowsFetched;
      X("numRowsFetched "SQLLEN_FORMAT" : "SQLLEN_FORMAT"", row, numRowsFetched);
    }
    X("Has got "SQLLEN_FORMAT" rows", row);
  } while ((ret = SQLMoreResults(hstmt)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO);

  return 0;
}

static void tsleep(int ms) {
  #ifdef _WIN32
    Sleep(ms);
  #else
    usleep(ms * 1000);
  #endif
}

static int row_count_test() {
  reset_stmt();
  SQLCHAR sql[100 + count_10 * 5 * 30];
  sql[0] = '\0';

  time_t t1 = time(NULL) * 1000;
  for (int num = 0; num < 1; num++) {
    sql[0] = '\0';
    sprintf((char *)sql, "insert into t_table (ts, double_val, int_val) values ");
    char val[64];

    for (int i = 0; i < count_10 * 5; i++) {
      val[0] = '\0';
      sprintf(val, " ("SQLLEN_FORMAT", %d, 0)", time(NULL) * 1000, 100 + i);
      strcat((char *)sql, val);
      tsleep(300);
    }
    CHK1(exec_sql, (char *)sql, 0);

    SQLLEN numberOfrows;
    CALL_SQLRowCount(link_info->ctx.hstmt, &numberOfrows);
    X("insert into t_table count: "SQLLEN_FORMAT"", numberOfrows);
  }
  time_t t2 = time(NULL) * 1000;
  for (int num = 0; num < 1; num++) {
    sql[0] = '\0';
    sprintf((char *)sql, "delete from t_table where ts >= "SQLLEN_FORMAT" and ts <= "SQLLEN_FORMAT"", t1, (t2 + t1) / 2);

    CHK1(exec_sql, (char *)sql, 0);

    SQLLEN numberOfrows;
    CALL_SQLRowCount(link_info->ctx.hstmt, &numberOfrows);
    X("delete from t_table count: "SQLLEN_FORMAT"", numberOfrows);
  }

  return 0;
}

static int primary_key_test(SQLCHAR* table) {
  SQLRETURN ret;
  SQLHANDLE hstmt = link_info->ctx.hstmt;
  ret = SQLPrimaryKeys(hstmt, NULL, 0, NULL, 0, table, SQL_NTS);

  // Fetch and print the primary key information
  SQLCHAR tableCat[MAX_TABLE_NAME_LEN];
  SQLCHAR tableSchem[MAX_TABLE_NAME_LEN];
  SQLCHAR tableName[MAX_TABLE_NAME_LEN];
  SQLCHAR columnName[MAX_TABLE_NAME_LEN];
  SQLCHAR pkName[MAX_TABLE_NAME_LEN];
  SQLUSMALLINT keySEQ;
  SQLLEN valLen;
  while (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
    ret = SQLFetch(hstmt);
    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
      SQLGetData(hstmt, 1, SQL_C_CHAR, tableCat, MAX_TABLE_NAME_LEN, &valLen);
      SQLGetData(hstmt, 2, SQL_C_CHAR, tableSchem, MAX_TABLE_NAME_LEN, &valLen);
      SQLGetData(hstmt, 3, SQL_C_CHAR, tableName, MAX_TABLE_NAME_LEN, &valLen);
      SQLGetData(hstmt, 4, SQL_C_CHAR, columnName, MAX_TABLE_NAME_LEN, &valLen);
      SQLGetData(hstmt, 5, SQL_C_SHORT, &keySEQ, sizeof(keySEQ), NULL);
      SQLGetData(hstmt, 6, SQL_C_CHAR, pkName, MAX_TABLE_NAME_LEN, &valLen);
      X("tableCat: %s, tableSchem: %s, tableName: %s, Column: %s, keySEQ: %d, pkName: %s",
        tableCat, tableSchem, tableName, columnName, keySEQ, pkName);
    }
  }
  return 0;
}

static int get_type_info_test() {
  SQLRETURN sr = SQL_SUCCESS;
  CHK0(reset_stmt, 0);
  SQLHANDLE hstmt = link_info->ctx.hstmt;

  sr = CALL_SQLGetTypeInfo(hstmt, SQL_ALL_TYPES);
  CHKSTMTR(hstmt, sr);

  // Fetch and print the data type information
  SQLCHAR typeName[MAX_TABLE_NUMBER];
  SQLCHAR localTypeName[MAX_TABLE_NUMBER];

  SQLLEN typeNameLen = 0;
  SQLINTEGER columnSize = 0;
  SQLSMALLINT dataType = 0, decimalDigits = 0;
  while (sr == SQL_SUCCESS || sr == SQL_SUCCESS_WITH_INFO) {
    sr = SQLFetch(hstmt);
    if (sr == SQL_SUCCESS || sr == SQL_SUCCESS_WITH_INFO) {
      CALL_SQLGetData(hstmt, 1, SQL_C_CHAR, typeName, MAX_TABLE_NUMBER, &typeNameLen);
      CALL_SQLGetData(hstmt, 2, SQL_C_SHORT, &dataType, sizeof(dataType), NULL);
      CALL_SQLGetData(hstmt, 3, SQL_C_LONG, &columnSize, sizeof(columnSize), NULL);
      CALL_SQLGetData(hstmt, 13, SQL_C_CHAR, localTypeName, MAX_TABLE_NUMBER, &typeNameLen);
      //CALL_SQLGetData(hstmt, 19, SQL_C_SHORT, &decimalDigits, sizeof(decimalDigits), NULL);
      printf("Type: %s, DataType: %d, localTypeName:%s, ColumnSize: %d, DecimalDigits: %d\n", typeName, dataType, localTypeName, columnSize, decimalDigits);
    }
  }

  return 0;
}

static int printSetResult(SQLSMALLINT handleType, SQLHANDLE handle, const char* event, SQLRETURN sr) {
  if (sr < 0) {
    X("%s failed %d", event, sr);
  }
  else {
    X("%s success", event);
  }
  if (handleType == SQL_HANDLE_STMT) {
    CHKSTMTR(handle, sr);
  }
  else if (handleType == SQL_HANDLE_DBC) {
    CHKDBR(handle, sr);
  }
  return 0;
}

static int printGetResult(SQLSMALLINT handleType, SQLHANDLE handle, const char* event, SQLRETURN sr, SQLLEN value) {
  if (sr < 0) {
    X("%s failed %d", event, sr);
  }
  else {
    X("%s: "SQLLEN_FORMAT"", event, value);
  }
  if (handleType == SQL_HANDLE_STMT) {
    CHKSTMTR(handle, sr);
  }
  else if (handleType == SQL_HANDLE_DBC) {
    CHKDBR(handle, sr);
  }
  return 0;
}

static int set_stmt_attr_test() {
  SQLRETURN sr = SQL_SUCCESS;
  SQLHANDLE hstmt = link_info->ctx.hstmt;

  SQLCHAR sql[] = "SELECT ts FROM t_table";
  sr = SQLPrepare(hstmt, sql, SQL_NTS);
  CHKSTMTR(hstmt, sr);
  X("SQLPrepare result:%d", sr);

  sr = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_ASYNC_ENABLE, (SQLPOINTER)SQL_ASYNC_ENABLE_OFF, SQL_IS_INTEGER);
  printSetResult(SQL_HANDLE_STMT, hstmt, "CALL_SQLSetStmtAttr SQL_ATTR_ASYNC_ENABLE", sr);

  SQLLEN async_enable = 0;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_ASYNC_ENABLE, &async_enable, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_ASYNC_ENABLE", sr, async_enable);

  // sr = CALL_SQLExecDirect(hstmt, (SQLCHAR*)sql, SQL_NTS);
  // D("CALL_SQLExecDirect: %s result:%d", sql, sr);

  sr = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_FORWARD_ONLY, SQL_IS_INTEGER);
  printSetResult(SQL_HANDLE_STMT, hstmt, "CALL_SQLSetStmtAttr SQL_ATTR_CURSOR_TYPE", sr);

  SQLLEN cursor_type;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, &cursor_type, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_CURSOR_TYPE", sr, cursor_type);

  SQLLEN max_rows = 100;
  sr = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER)&max_rows, SQL_IS_INTEGER);
  printSetResult(SQL_HANDLE_STMT, hstmt, "CALL_SQLSetStmtAttr SQL_ATTR_MAX_ROWS", sr);

  max_rows = 0;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_MAX_ROWS, &max_rows, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_MAX_ROWS", sr, max_rows);

  SQLLEN max_length = 100;
  sr = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_MAX_LENGTH, (SQLPOINTER)&max_length, SQL_IS_INTEGER);
  printSetResult(SQL_HANDLE_STMT, hstmt, "CALL_SQLSetStmtAttr SQL_ATTR_MAX_LENGTH", sr);

  max_length = 0;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_MAX_LENGTH, &max_length, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_MAX_LENGTH", sr, max_length);

  SQLLEN query_timeout = 100;
  sr = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)&query_timeout, SQL_IS_INTEGER);
  printSetResult(SQL_HANDLE_STMT, hstmt, "CALL_SQLSetStmtAttr SQL_ATTR_QUERY_TIMEOUT", sr);

  query_timeout = 0;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_QUERY_TIMEOUT, &query_timeout, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_QUERY_TIMEOUT", sr, query_timeout);

  sr = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_NOSCAN, (SQLPOINTER)SQL_NOSCAN_ON, SQL_IS_INTEGER);
  printSetResult(SQL_HANDLE_STMT, hstmt, "CALL_SQLSetStmtAttr SQL_ATTR_NOSCAN", sr);

  SQLLEN noscan = 0;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_NOSCAN, &noscan, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_NOSCAN", sr, noscan);

  return 0;
}

static int get_stmt_attr_test() {
  SQLRETURN sr = SQL_SUCCESS;
  SQLHANDLE hstmt = link_info->ctx.hstmt;

  SQLCHAR sql[] = "SELECT * FROM t_table";
  sr = SQLPrepare(hstmt, sql, SQL_NTS);
  CHKSTMTR(hstmt, sr);
  X("SQLPrepare result:%d", sr);

  SQLLEN cursorType = 0;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, &cursorType, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_CURSOR_TYPE", sr, cursorType);

  SQLLEN row_number = 0;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_NUMBER, &row_number, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_ROW_NUMBER", sr, row_number);

  SQLLEN rowArarrySize = 0;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &rowArarrySize, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_ROW_ARRAY_SIZE", sr, rowArarrySize);

  SQLLEN time_out = 0;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_QUERY_TIMEOUT, &time_out, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_QUERY_TIMEOUT", sr, time_out);

  SQLLEN maxRows = 0;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_MAX_ROWS, &maxRows, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_MAX_ROWS", sr, maxRows);

  SQLLEN param_bind_type = 0;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_PARAM_BIND_TYPE, &param_bind_type, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_PARAM_BIND_TYPE", sr, param_bind_type);

  SQLLEN param_size = 0;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE, &param_size, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_PARAMSET_SIZE", sr, param_size);

  SQLLEN row_array_size = 0;
  sr = SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &row_array_size, 0, NULL);
  printGetResult(SQL_HANDLE_STMT, hstmt, "SQLGetStmtAttr SQL_ATTR_ROW_ARRAY_SIZE", sr, row_array_size);

  return 0;
}

static int num_param_t_table_test() {
  SQLRETURN sr = SQL_SUCCESS;
  reset_stmt();
  SQLHANDLE hstmt = link_info->ctx.hstmt;

  char* sql[] = {
    // "select ts from t_table;",
    // "select * from t_table where ts = ?;",
    "select * from t_table where ts = ? and double_val = ?;",
    "select * from t_table where ts = ? and double_val = ?;",
    "insert into t_table (ts, double_val, int_val) values (?, ?, ?);",
    "delete from t_table where ts = ? and double_val = ?;",
  };
  for (unsigned long i = 0; i < sizeof(sql) / sizeof(char*); i++) {
    X("prepare...  sql:%s", sql[i]);
    sr = CALL_SQLPrepare(hstmt, (SQLCHAR*)sql[i], SQL_NTS);
    if (FAILED(sr)) return -1;

    SQLSMALLINT ParameterCount = 0;
    sr = CALL_SQLNumParams(hstmt, &ParameterCount);
    if (FAILED(sr)) return -1;
    X("sql:%s  numParam:%d", sql[i], ParameterCount);

    for (int i = 0; i < ParameterCount; ++i) {
      SQLSMALLINT DataType = 0;
      SQLULEN     ParameterSize = 0;
      SQLSMALLINT DecimalDigits = 0;
      SQLSMALLINT Nullable = 0;
      sr = CALL_SQLDescribeParam(hstmt, i + 1, &DataType, &ParameterSize, &DecimalDigits, &Nullable);
      X("\t\tparam %d: datatype(%d) paramsize("SQLLEN_FORMAT") decimaldigit(%d) nullable(%d)", i, DataType, ParameterSize, DecimalDigits, Nullable);
      if (FAILED(sr)) return -1;
    }
    CALL_SQLCloseCursor(hstmt);
  }

  return 0;
}

static int get_records_counts(char* table_name) {
  reset_stmt();
  SQLHANDLE hstmt = link_info->ctx.hstmt;
  char sql[1024];
  SQLLEN numberOfrows;
  sprintf(sql, "SELECT * FROM %s", table_name);
  CALL_SQLExecDirect(hstmt, (SQLCHAR *)sql, SQL_NTS);
  CALL_SQLRowCount(hstmt, &numberOfrows);
  X("%s record count: "SQLLEN_FORMAT"", table_name, numberOfrows);

  return (int)numberOfrows;
}

static int show_columns2(char* table_name) {
  CHK0(reset_stmt, 0);
  SQLHANDLE hstmt = link_info->ctx.hstmt;
  CALL_SQLColumns(hstmt, NULL, 0, NULL, 0, (SQLCHAR*)table_name, SQL_NTS, NULL, 0);

  SQLCHAR columnName[256];
  SQLLEN columnNameLen;
  SQLCHAR dataType[256];
  SQLLEN dataTypeLen;
  SQLCHAR typeName[256];
  SQLLEN typeNameLen;
  SQLINTEGER columnSize;
  SQLSMALLINT decimalDigits;
  SQLSMALLINT nullable;
  while (CALL_SQLFetch(hstmt) == SQL_SUCCESS) {
    SQLGetData(hstmt, 4, SQL_C_CHAR, columnName, 256, &columnNameLen);
    SQLGetData(hstmt, 5, SQL_C_CHAR, dataType, 256, &dataTypeLen);
    SQLGetData(hstmt, 6, SQL_C_CHAR, typeName, 256, &typeNameLen);
    SQLGetData(hstmt, 7, SQL_C_LONG, &columnSize, 0, NULL);
    SQLGetData(hstmt, 9, SQL_C_SHORT, &decimalDigits, 0, NULL);
    SQLGetData(hstmt, 11, SQL_C_SHORT, &nullable, 0, NULL);

    X("Column Name: %s", columnName);
    X("Data Type: %s", dataType);
    X("Type Name: %s", typeName);
    X("Column Size: %d", columnSize);
    X("Decimal Digits: %d", decimalDigits);
    X("Nullable: %d", nullable);
  }
  return 0;
}

static int end_tran_test() {
  SQLRETURN sr = SQL_SUCCESS;
  SQLHANDLE hstmt = link_info->ctx.hstmt;

  sr = SQLGetTypeInfo(hstmt, SQL_ALL_TYPES);
  CHKSTMTR(hstmt, sr);

  sr = SQLEndTran(SQL_HANDLE_DBC, link_info->ctx.hconn, SQL_COMMIT);
  X("SQLEndTran result: %d", sr);

  return 0;
}

static int describe_col_test(char* table_name) {
  reset_stmt();
  SQLHANDLE hstmt = link_info->ctx.hstmt;
  SQLRETURN  ret;

  SQLCHAR sql[1024];
  sprintf((char *)sql, "SELECT * FROM %s limit 12; SELECT ts, double_val FROM %s limit 25;", table_name, table_name);

  CALL_SQLExecDirect(hstmt, sql, SQL_NTS);

  SQLSMALLINT columnCount;
  ret = CALL_SQLNumResultCols(hstmt, &columnCount);
  CHKSTMTR(hstmt, ret);

  SQLCHAR colName[256];
  SQLSMALLINT colNameLen;
  SQLSMALLINT dataType;
  SQLULEN colSize;
  SQLSMALLINT decimalDigits;
  SQLSMALLINT nullable;

  for (int i = 0; i < columnCount; ++i) {
    ret = SQLDescribeCol(hstmt, i + 1, colName, sizeof(colName), &colNameLen, &dataType, &colSize, &decimalDigits, &nullable);
    if (ret == SQL_SUCCESS) {
      printf("Column Name: %.*s\n", colNameLen, colName);
      printf("Data Type: %d\n", dataType);
      printf("Column Size: "SQLLEN_FORMAT"\n", colSize);
      printf("Decimal Digits: %d\n", decimalDigits);
      printf("Nullable: %d\n", nullable);
    }
    else {
      // Handle error
      printf("Failed to describe column.\n");
    }
    printf("\n");
  }

  return 0;
}

static int col_attribute_test(char* table_name) {
  reset_stmt();
  SQLHANDLE hstmt = link_info->ctx.hstmt;
  SQLRETURN  ret;

  SQLCHAR sql[1024];
  sprintf((char *)sql, "SELECT * FROM %s limit 12;", table_name);

  CALL_SQLExecDirect(hstmt, sql, SQL_NTS);

  SQLSMALLINT columnCount;
  ret = CALL_SQLNumResultCols(hstmt, &columnCount);
  CHKSTMTR(hstmt, ret);

  SQLCHAR valstr[256];
  SQLSMALLINT valLen;

  for (int i = 1; i <= columnCount; ++i) {
    valstr[0] = '\0';
    ret = SQLColAttribute(hstmt, i, SQL_DESC_NAME, valstr, sizeof(valstr), &valLen, NULL);
    if (ret == SQL_SUCCESS) {
      printf("SQL_DESC_NAME: %.*s\n", valLen, valstr);
    }
    else {
      printf("Failed to get column attribute.\n");
    }

    valstr[0] = '\0';
    ret = SQLColAttribute(hstmt, i, SQL_DESC_LOCAL_TYPE_NAME, valstr, sizeof(valstr), &valLen, NULL);
    if (ret == SQL_SUCCESS) {
      printf("SQL_DESC_LOCAL_TYPE_NAME: %.*s\n", valLen, valstr);
    }
    else {
      printf("Failed to get column attribute.\n");
    }

    valstr[0] = '\0';
    ret = SQLColAttribute(hstmt, i, SQL_DESC_LITERAL_PREFIX, valstr, sizeof(valstr), &valLen, NULL);
    if (ret == SQL_SUCCESS) {
      printf("SQL_DESC_LITERAL_PREFIX: %.*s\n", valLen, valstr);
    }
    else {
      printf("Failed to get column attribute.\n");
    }

    valstr[0] = '\0';
    ret = SQLColAttribute(hstmt, i, SQL_DESC_BASE_COLUMN_NAME, valstr, sizeof(valstr), &valLen, NULL);
    if (ret == SQL_SUCCESS) {
      printf("SQL_DESC_BASE_COLUMN_NAME: %.*s\n", valLen, valstr);
    }
    else {
      printf("Failed to get column attribute.\n");
    }

    valstr[0] = '\0';
    ret = SQLColAttribute(hstmt, i, SQL_DESC_BASE_TABLE_NAME, valstr, sizeof(valstr), &valLen, NULL);
    if (ret == SQL_SUCCESS) {
      printf("SQL_DESC_BASE_TABLE_NAME: %.*s\n", valLen, valstr);
    }
    else {
      printf("Failed to get column attribute.\n");
    }

    valstr[0] = '\0';
    ret = SQLColAttribute(hstmt, i, SQL_DESC_LABEL, valstr, sizeof(valstr), &valLen, NULL);
    if (ret == SQL_SUCCESS) {
      printf("SQL_DESC_LABEL: %.*s\n", valLen, valstr);
    }
    else {
      printf("Failed to get column attribute.\n");
    }

    valstr[0] = '\0';
    ret = SQLColAttribute(hstmt, i, SQL_DESC_CATALOG_NAME, valstr, sizeof(valstr), &valLen, NULL);
    if (ret == SQL_SUCCESS) {
      printf("SQL_DESC_CATALOG_NAME: %.*s\n", valLen, valstr);
    }
    else {
      printf("Failed to get column attribute.\n");
    }

    SQLLEN val = 0;
    ret = SQLColAttribute(hstmt, i, SQL_DESC_TYPE, NULL, SQL_IS_INTEGER, &valLen, &val);
    if (ret == SQL_SUCCESS) {
      printf("SQL_DESC_TYPE: "SQLLEN_FORMAT"\n", val);
    }
    else {
      printf("Failed to get column attribute.\n");
      CHKSTMTR(hstmt, ret);
    }

    val = 0;
    ret = SQLColAttribute(hstmt, i, SQL_DESC_COUNT, NULL, SQL_IS_INTEGER, &valLen, &val);
    if (ret == SQL_SUCCESS) {
      printf("SQL_DESC_COUNT: "SQLLEN_FORMAT"\n", val);
    }
    else {
      printf("Failed to get column attribute.\n");
      CHKSTMTR(hstmt, ret);
    }

    val = 0;
    ret = SQLColAttribute(hstmt, i, SQL_DESC_PRECISION, NULL, SQL_IS_INTEGER, &valLen, &val);
    if (ret == SQL_SUCCESS) {
      printf("SQL_DESC_PRECISION: "SQLLEN_FORMAT"\n", val);
    }
    else {
      printf("Failed to get column attribute.\n");
      CHKSTMTR(hstmt, ret);
    }

    val = 0;
    ret = SQLColAttribute(hstmt, i, SQL_DESC_SCALE, NULL, SQL_IS_INTEGER, &valLen, &val);
    if (ret == SQL_SUCCESS) {
      printf("SQL_DESC_SCALE: "SQLLEN_FORMAT"\n", val);
    }
    else {
      printf("Failed to get column attribute.\n");
      CHKSTMTR(hstmt, ret);
    }
    printf("\n");
  }

  return 0;
}

static int case_1(void) {
  char* tmp_db = "tmp_db";
  CHK0(create_sql_connect, 0);
  CHK1(drop_database, tmp_db, 0);
  CHK1(create_database, tmp_db, 0);
  CHK1(use_db, tmp_db, 0);

  char sql[1024];

  SQLCHAR table_names[MAX_TABLE_NUMBER][MAX_TABLE_NAME_LEN];
  sql[0] = '\0';
  strcpy(sql, "DROP STABLE IF EXISTS `metertemplate`");
  CHK1(exec_sql, sql, 0);

  sql[0] = '\0';
  strcpy(sql, "CREATE STABLE `metertemplate` (`ts` TIMESTAMP, `double_val` DOUBLE, `int_val` INT) \
    TAGS(`element_id` NCHAR(100), `location` NCHAR(100))");
  CHK1(exec_sql, sql, 0);
  int table_counts = get_tables(table_names);
  ASSERT_EQUAL(table_counts, 1);
  ASSERT_EQUALS((char *)table_names[0], "metertemplate");

  sql[0] = '\0';
  strcpy(sql, "CREATE TABLE tb1 using metertemplate TAGS ('00001', 'location_001')");
  CHK1(exec_sql, sql, 0);
  sql[0] = '\0';
  strcpy(sql, "CREATE TABLE tb2 using metertemplate TAGS ('00002', 'location_002')");
  CHK1(exec_sql, sql, 0);
  sql[0] = '\0';
  strcpy(sql, "CREATE TABLE tb3 using metertemplate TAGS ('00003', 'location_003')");
  CHK1(exec_sql, sql, 0);
  table_counts = get_tables(table_names);
  ASSERT_EQUAL(table_counts, 4);

  show_columns1("metertemplate");
  show_columns2("tb1");

  const int insert_count = 10;
  for (int i = 0; i < insert_count; i++) {
    sql[0] = '\0';
    sprintf(sql, "insert into tb1 (ts, double_val, int_val) values (now(), %d, 0)", 100 + i);
    CHK1(exec_sql, sql, 0);
  }
  for (int i = 0; i < insert_count; i++) {
    sql[0] = '\0';
    sprintf(sql, "insert into tb2 (ts, double_val, int_val) values (now(), %d, 0)", 100 + i);
    CHK1(exec_sql, sql, 0);
  }
  show_table_data("tb1");
  show_table_data("metertemplate");
  get_records_counts("tb1");

  // int st_counts = get_records_counts("metertemplate");
  // ASSERT_EQUAL(st_counts, 2* insert_count);

  CHK0(free_connect, 0);
  return 0;
}

static int case_2(void) {
  char* tmp_db = "tmp_db2";
  CHK0(create_sql_connect, 0);
  CHK1(drop_database, tmp_db, 0);
  CHK1(create_database, tmp_db, 0);
  CHK1(use_db, tmp_db, 0);

  char sql[1024];

  SQLCHAR table_names[MAX_TABLE_NUMBER][MAX_TABLE_NAME_LEN];
  sql[0] = '\0';
  strcpy(sql, "DROP TABLE IF EXISTS tbx");
  CHK1(exec_sql, sql, 0);

  sql[0] = '\0';
  strcpy(sql, "CREATE TABLE `tbx` (`ts` TIMESTAMP, `double_val` DOUBLE, `int_val` INT)");
  CHK1(exec_sql, sql, 0);
  int table_counts = get_tables(table_names);
  ASSERT_EQUAL(table_counts, 1);

  const int insert_count = 10;
  for (int i = 0; i < insert_count; i++) {
    sql[0] = '\0';
    sprintf(sql, "insert into tbx (ts, double_val, int_val) values (now(), %d, 0)", 100 + i);
    CHK1(exec_sql, sql, 0);
  }

  show_table_data("tbx");
  // int tab1_counts = get_records_counts("tbx");
  // ASSERT_EQUAL(tab1_counts, insert_count);

  return 0;
}

static int check_t_table() {
  if (is_sql_server_test()) return 0;
  CHK1(exec_sql, t_table_create, 0);
  return 0;
}

static int case_3(void) {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);
  CHK1(exec_sql, "drop table if EXISTS t_table;", 0);
  CHK0(check_t_table, 0);

  char sql[100 + count_1000 * 64];
  sql[0] = '\0';

  CHK0(reset_stmt, 0);

  double cost = 0;
  time_t current_time = time(NULL);

  for (int num = 0; num < count_1000; num++) {
    sql[0] = '\0';
    sprintf(sql, "insert into t_table (ts, double_val, int_val) values ");
    char val[64];

    for (int i = 0; i < count_1000; i++) {
      val[0] = '\0';
      sprintf(val, " ("SQLLEN_FORMAT", %d, 0)", current_time * 1000 + num * count_1000 + i, 100 + i);
      strcat(sql, val);
    }

    clock_t t1_1 = clock();
    int r = SQLExecDirect(link_info->ctx.hstmt, (SQLCHAR*)sql, SQL_NTS);
    if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) {
      CHKSTMTR(link_info->ctx.hstmt, r);
      return -1;
    }
    clock_t t1_2 = clock();
    double elapsed_time = (double)(t1_2 - t1_1) / CLOCKS_PER_SEC;
    D("SQLExecDirect, cost time : % f seconds", elapsed_time);
    cost += elapsed_time;
  }

  X("Write %d * %d rows data, cost time : % f seconds", count_1000, count_1000, cost);

  clock_t t2_1 = clock();
  int counts = show_table_data("t_table");
  clock_t t2_2 = clock();
  double elapsed_time = (double)(t2_2 - t2_1) / CLOCKS_PER_SEC;
  X("read %d rows data, cost time : % f seconds", counts, elapsed_time);

  CHK0(free_connect, 0);
  return 0;
}

#define GET_FLOAT_VAL(x) (*(float *)(x))
#define GET_DOUBLE_VAL(x) (*(double *)(x))

typedef uint16_t VarDataLenT;
#define varDataLen(v) ((VarDataLenT *)(v))[0]

#define VARSTR_HEADER_SIZE sizeof(VarDataLenT)
int printRow(int row_num, char* str, TAOS_ROW row, TAOS_FIELD* fields, int numFields) {
  int  len = 0;
  char split = ' ';

  for (int i = 0; i < numFields; ++i) {
    if (i > 0) {
      str[len++] = split;
    }

    if (row[i] == NULL) {
      len += sprintf(str + len, "%s", "NULL");
      continue;
    }

    switch (fields[i].type) {
    case TSDB_DATA_TYPE_TINYINT:
      len += sprintf(str + len, "%d", *((int8_t*)row[i]));
      break;

    case TSDB_DATA_TYPE_UTINYINT:
      len += sprintf(str + len, "%u", *((uint8_t*)row[i]));
      break;

    case TSDB_DATA_TYPE_SMALLINT:
      len += sprintf(str + len, "%d", *((int16_t*)row[i]));
      break;

    case TSDB_DATA_TYPE_USMALLINT:
      len += sprintf(str + len, "%u", *((uint16_t*)row[i]));
      break;

    case TSDB_DATA_TYPE_INT:
      len += sprintf(str + len, "%d", *((int32_t*)row[i]));
      break;

    case TSDB_DATA_TYPE_UINT:
      len += sprintf(str + len, "%u", *((uint32_t*)row[i]));
      break;

    case TSDB_DATA_TYPE_BIGINT:
      len += sprintf(str + len, "%" PRId64, *((int64_t*)row[i]));
      break;

    case TSDB_DATA_TYPE_UBIGINT:
      len += sprintf(str + len, "%" PRIu64, *((uint64_t*)row[i]));
      break;

    case TSDB_DATA_TYPE_FLOAT: {
      float fv = 0;
      fv = GET_FLOAT_VAL(row[i]);
      len += sprintf(str + len, "%f", fv);
    } break;

    case TSDB_DATA_TYPE_DOUBLE: {
      double dv = 0;
      dv = GET_DOUBLE_VAL(row[i]);
      len += sprintf(str + len, "%lf", dv);
    } break;

    case TSDB_DATA_TYPE_BINARY:
    case TSDB_DATA_TYPE_NCHAR:
    case TSDB_DATA_TYPE_GEOMETRY: {
      int32_t charLen = varDataLen((char*)row[i] - VARSTR_HEADER_SIZE);
      memcpy(str + len, row[i], charLen);
      len += charLen;
    } break;

    case TSDB_DATA_TYPE_TIMESTAMP:
      len += sprintf(str + len, "%" PRId64, *((int64_t*)row[i]));
      break;

    case TSDB_DATA_TYPE_BOOL:
      len += sprintf(str + len, "%d", *((int8_t*)row[i]));
    default:
      break;
    }
    D("Row:%d Column %d: %s", row_num, i, str);
  }

  return len;
}
static int print_result_taos(TAOS_RES* res) {
  int         numFields = taos_num_fields(res);
  TAOS_FIELD* fields = taos_fetch_fields(res);
  char        header[256] = { 0 };
  int len = 0;
  for (int i = 0; i < numFields; ++i) {
    len += sprintf(header + len, "%s ", fields[i].name);
  }

  TAOS_ROW row = NULL;
  int row_num = 0;
  while ((row = taos_fetch_row(res))) {
    char temp[256] = { 0 };
    printRow(row_num, temp, row, fields, numFields);
    ++row_num;
  }
  return row_num;
}

static int case_3_1(void) {
  TAOS* taos = taos_connect("192.168.1.98:6030", "root", "taosdata", NULL, 0);
  if (taos == NULL) {
    printf("failed to connect to server, reason:%s\n", "null taos" /*taos_errstr(taos)*/);
    exit(1);
  }
  TAOS_RES* res = taos_query(taos, "use meter;");
  if (taos_errno(res) != 0) {
    printf("failed to taos_query, reason:%s\n", taos_errstr(res));
  }

  char* drop_sql = "drop TABLE if EXISTS `t_table2`;";
  res = taos_query(taos, drop_sql);
  if (taos_errno(res) != 0) {
    printf("failed to taos_query, reason:%s\n", taos_errstr(res));
  }

  char* create_sql = "CREATE TABLE if not exists `t_table2` (`ts` TIMESTAMP, `double_val` DOUBLE, `int_val` INT)";
  res = taos_query(taos, create_sql);
  if (taos_errno(res) != 0) {
    printf("failed to taos_query, reason:%s\n", taos_errstr(res));
  }

  // TAOS_STMT* stmt = taos_stmt_init( taos);

  char sql[100 + count_1000 * 64];
  sql[0] = '\0';

  double cost = 0;
  time_t current_time = time(NULL);

  for (int num = 0; num < count_1000; num++) {
    sql[0] = '\0';
    sprintf(sql, "insert into t_table2 (ts, double_val, int_val) values ");
    char val[64];

    for (int i = 0; i < count_1000; i++) {
      val[0] = '\0';
      sprintf(val, " ("SQLLEN_FORMAT", %d, 0)", current_time * 1000 + num * count_1000 + i, 100 + i);
      strcat(sql, val);
    }
    clock_t t1_1 = clock();
    res = taos_query(taos, sql);
    if (taos_errno(res) != 0) {
      printf("failed to taos_query, reason:%s\n", taos_errstr(res));
    }
    clock_t t1_2 = clock();
    double elapsed_time = (double)(t1_2 - t1_1) / CLOCKS_PER_SEC;
    D("taos_query for write, cost time : % f seconds",  elapsed_time);
    cost += elapsed_time;
  }
  X("Write %d * %d rows data, cost time : % f seconds", count_1000, count_1000, cost);

  char* select_sql = "select * from `t_table2`;";

  clock_t t2_1 = clock();
  res = taos_query(taos, select_sql);
  if (taos_errno(res) != 0) {
    printf("failed to execute taos_query. error: %s\n", taos_errstr(res));
    exit(EXIT_FAILURE);
  }
  int read_count = print_result_taos(res);
  clock_t t2_2 = clock();
  double elapsed_time = (double)(t2_2 - t2_1) / CLOCKS_PER_SEC;
  X("read %d rows, cost time : % f seconds", read_count, elapsed_time);

  taos_close(taos);
  taos_cleanup();
  return 0;
}

static int case_4(void) {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);
  CHK0(check_t_table, 0);

  clock_t t2 = clock();
  int counts = fetch_scroll_test("t_table");
  if (counts < 0) return -1;

  clock_t t3 = clock();
  double elapsed_time = (double)(t3 - t2) / CLOCKS_PER_SEC;
  X("Read %d rows data, cost time: %f seconds", counts, elapsed_time);

  //CHK0(free_connect, 0);
  return 0;
}

static int case_5(void) {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);
  CHK0(check_t_table, 0);

  CHK1(more_result_test, "t_table", 0);

  CHK0(free_connect, 0);
  return 0;
}

static int case_6(void) {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);
  CHK0(check_t_table, 0);

  CHK0(row_count_test, 0);

  CHK0(free_connect, 0);
  return 0;
}

static int case_7(void) {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);
  CHK0(check_t_table, 0);

  CHK1(primary_key_test, (SQLCHAR *)"t_table", 0);

  CHK0(free_connect, 0);
  return 0;
}

static int case_8(void) {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);
  CHK0(check_t_table, 0);

  CHK0(num_param_t_table_test, 0);

  CHK0(free_connect, 0);
  return 0;
}

static int case_9(void) {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);

  CHK0(get_type_info_test, 0);

  CHK0(free_connect, 0);
  return 0;
}

static int case_9_1(void) {
  CHK0(create_sql_connect, 0);

  CHK0(get_type_info_test, 0);

  CHK0(free_connect, 0);
  return 0;
}

static int case_10(void) {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);
  CHK0(check_t_table, 0);

  CHK0(set_stmt_attr_test, 0);

  CHK0(free_connect, 0);
  return 0;
}

static int case_11(void) {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);
  CHK0(check_t_table, 0);

  CHK0(get_stmt_attr_test, 0);

  CHK0(free_connect, 0);
  return 0;
}

static int case_12() {
  SQLRETURN sr;

  sr = CALL_SQLAllocHandle(SQL_HANDLE_DBC, link_info->ctx.henv, &link_info->ctx.hconn);
  if (sr != SQL_SUCCESS && sr != SQL_SUCCESS_WITH_INFO) return -1;

  X("connect dsn:%s user:%s pwd:%s", link_info->dsn, link_info->user, link_info->pwd);

  // sr = CALL_SQLConnect(link_info->ctx.hconn, (SQLCHAR*)link_info->dsn, SQL_NTS, (SQLCHAR*)link_info->user, SQL_NTS, (SQLCHAR*)link_info->pwd, SQL_NTS);
  // if (sr != SQL_SUCCESS && sr != SQL_SUCCESS_WITH_INFO) return -1;
  // X("connect dsn:%s result:%d", link_info->dsn, sr);

  SQLHDBC hDbc = link_info->ctx.hconn;
  sr = SQLConnect(hDbc, (SQLCHAR*)link_info->dsn, SQL_NTS, (SQLCHAR *)"wrong_dsn", 0, NULL, 0);
  if (sr != SQL_SUCCESS) {
    // Handle connection error
    SQLCHAR sqlState[6] = { 0 };
    SQLINTEGER nativeError = 0;
    SQLCHAR messageText[SQL_MAX_MESSAGE_LENGTH] = { 0 };
    SQLSMALLINT textLength = 0;

    SQLGetDiagField(SQL_HANDLE_DBC, hDbc, 1, SQL_DIAG_SQLSTATE, sqlState, sizeof(sqlState), NULL);
    SQLGetDiagField(SQL_HANDLE_DBC, hDbc, 1, SQL_DIAG_NATIVE, &nativeError, sizeof(nativeError), NULL);
    SQLGetDiagField(SQL_HANDLE_DBC, hDbc, 1, SQL_DIAG_MESSAGE_TEXT, messageText, sizeof(messageText), &textLength);

    printf("Connection failed:\n");
    printf("SQLSTATE: %s\n", sqlState);
    printf("Native Error: %d\n", nativeError);
    printf("Message Text: %.*s\n", textLength, messageText);
  }
  CHK0(free_connect, 0);
  return 0;
}

static int case_13(void) {
  int r = 0;

  r = CALL_SQLAllocHandle(SQL_HANDLE_DBC, link_info->ctx.henv, &link_info->ctx.hconn);
  if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;
  SQLHANDLE hconn = link_info->ctx.hconn;

  X("connect dsn:%s user:%s pwd:%s", link_info->dsn, link_info->user, link_info->pwd);
  r = CALL_SQLConnect(hconn, (SQLCHAR*)link_info->dsn, SQL_NTS, (SQLCHAR*)link_info->user, SQL_NTS, (SQLCHAR*)link_info->pwd, SQL_NTS);
  if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;
  X("connect dsn:%s result:%d", link_info->dsn, r);

  r = CALL_SQLSetConnectAttr(hconn, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)30, SQL_IS_UINTEGER);
  X("CALL_SQLSetConnectAttr SQL_ATTR_LOGIN_TIMEOUT set 30, result:%d", r);

  SQLLEN login_time_out;
  r = SQLGetStmtAttr(hconn, SQL_ATTR_LOGIN_TIMEOUT, &login_time_out, 0, NULL);
  printGetResult(SQL_HANDLE_DBC, hconn, "SQLGetStmtAttr SQL_ATTR_LOGIN_TIMEOUT", r, login_time_out);

  r = CALL_SQLSetConnectAttr(hconn, SQL_ATTR_PACKET_SIZE, (SQLPOINTER)1048576, SQL_IS_UINTEGER);
  X("CALL_SQLSetConnectAttr SQL_ATTR_PACKET_SIZE set 1048576 byte, result:%d", r);

  SQLLEN packet_size;
  r = SQLGetStmtAttr(hconn, SQL_ATTR_PACKET_SIZE, &packet_size, 0, NULL);
  printGetResult(SQL_HANDLE_DBC, hconn, "SQLGetStmtAttr SQL_ATTR_PACKET_SIZE", r, packet_size);

  CHK0(free_connect, 0);
  return 0;
}

static int case_14(void) {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);

  CHK0(end_tran_test, 0);

  CHK0(free_connect, 0);
  return 0;
}

static int case_15(void) {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);
  CHK0(check_t_table, 0);

  CHK1(describe_col_test, "t_table", 0);
  CHK0(free_connect, 0);
  return 0;
}

static int case_16(void) {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);
  CHK0(check_t_table, 0);

  CHK1(col_attribute_test, "t_table", 0);
  CHK0(free_connect, 0);
  return 0;
}

static int case_17(void) {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);

  char sql[1024];
  sql[0] = '\0';
  strcpy(sql, "DROP STABLE IF EXISTS `stable1`");
  CHK1(exec_sql, sql, 0);

  sql[0] = '\0';
  strcpy(sql, "CREATE STABLE `stable1` (`ts` TIMESTAMP, `double_val` DOUBLE, `int_val` INT, `unint_val` INT UNSIGNED, `bigint_val` BIGINT, `unbigint_val` BIGINT UNSIGNED, `unsmallint_val` SMALLINT UNSIGNED, `tinyint_val` TINYINT, `untinyint_val` TINYINT UNSIGNED,                                             \
                                        `bool_val` BOOL, `nchar100_val` NCHAR(100), `float_val` FLOAT, `binary_val` BINARY(100), `smallint_val` SMALLINT, `vchar100_val` VARCHAR(100)) TAGS(`element_id` NCHAR(100), `location` NCHAR(100))");
  CHK1(exec_sql, sql, 0);

  sql[0] = '\0';
  strcpy(sql, "CREATE TABLE tytb1 using stable1 TAGS ('00001', 'location_001')");
  CHK1(exec_sql, sql, 0);
  sql[0] = '\0';
  strcpy(sql, "CREATE TABLE tytb2 using stable1 TAGS ('00002', 'location_002')");
  CHK1(exec_sql, sql, 0);
  sql[0] = '\0';
  strcpy(sql, "CREATE TABLE tytb3 using stable1 TAGS ('00003', 'location_003')");
  CHK1(exec_sql, sql, 0);

  X("\n stable1:column:");
  show_columns1("stable1");

  X("\n tytb1:column:");
  show_columns2("tytb1");

  const int insert_count = 10;
  for (int i = 0; i < insert_count; i++) {
    sql[0] = '\0';
    sprintf(sql, "insert into tytb1 (ts, double_val, int_val, unint_val, bigint_val, unbigint_val,      \
                                     unsmallint_val,  tinyint_val, untinyint_val, bool_val, nchar100_val, \
                                     float_val, binary_val, smallint_val, vchar100_val )         \
                                     values (now(), %f, %d, %u, %d, %u ,                            \
                                     %d, %d, %u, true, \'%s\', %f, \'%s\', %d, \'%s\' )",                            \
                                     100.0 + i, -i, i, -10000000-i, 10000000 + i, \
                                     i, -i, i, "test",  100.0 + i, "binary", i, "vchartest");
    CHK1(exec_sql, sql, 0);
    sql[0] = '\0';
    sprintf(sql, "insert into tytb2 (ts, double_val, int_val, unint_val, bigint_val, unbigint_val,      \
                                     unsmallint_val,  tinyint_val, untinyint_val, bool_val, nchar100_val, \
                                     float_val, binary_val, smallint_val, vchar100_val )         \
                                     values (now(), %f, %d, %u, %d, %u ,                            \
                                     %d, %d, %u, true, \'%s\', %f, \'%s\', %d, \'%s\' )", \
                                     100.0 + i, -i, i, -10000000 - i, 10000000 + i, \
                                     i, -i, i, "test", 100.0 + i, "binary", i, "vchartest");
    CHK1(exec_sql, sql, 0);
    sql[0] = '\0';
    sprintf(sql, "insert into tytb3 (ts, double_val, int_val, unint_val, bigint_val, unbigint_val,      \
                                     unsmallint_val,  tinyint_val, untinyint_val, bool_val, nchar100_val, \
                                     float_val, binary_val, smallint_val, vchar100_val )         \
                                     values (now(), %f, %d, %u, %d, %u ,                            \
                                     %d, %d, %u, true, \'%s\', %f, \'%s\', %d, \'%s\' )", \
                                     100.0 + i, -i, i, -10000000 - i, 10000000 + i, \
                                     i, -i, i, "test", 100.0 + i, "binary", i, "vchartest");
    CHK1(exec_sql, sql, 0);
    tsleep(2);
  }



  X("\n tytb1:data:");
  show_table_data("tytb1");

  X("\n stable1:data:");
  show_table_data("stable1");

  CHK0(free_connect, 0);
  return 0;
}

void checkErrorCode(TAOS_STMT* stmt, int code, const char* msg) {
  if (code != 0) {
    printf("%s. error: %s\n", msg, taos_stmt_errstr(stmt));
    taos_stmt_close(stmt);
    exit(EXIT_FAILURE);
  }
}
#define ARRAY_SIZE 10000
static double case_18_helper(int64_t current_10ms, int mode) {
  int r = 0;

  double elapsed_time = 0;
#define max_test_char_len 10
  int64_t ts_arr[ARRAY_SIZE] = { 0 };
  SQLLEN  ts_ind[ARRAY_SIZE] = { 0 };
  char    varchar_arr[ARRAY_SIZE][max_test_char_len];
  int32_t varchar_arr_len[ARRAY_SIZE] = { 0 };
  SQLLEN  varchar_ind[ARRAY_SIZE] = { 0 };
  char    nchar_arr[ARRAY_SIZE][max_test_char_len];
  int32_t nchar_arr_len[ARRAY_SIZE] = { 0 };
  SQLLEN  nchar_ind[ARRAY_SIZE] = { 0 };
  int64_t i64_arr[ARRAY_SIZE] = { 0 };
  SQLLEN  i64_ind[ARRAY_SIZE] = { 0 };

  const param_t params[4] = {
      {SQL_PARAM_INPUT,  SQL_C_SBIGINT,  SQL_TYPE_TIMESTAMP,  23,       3,          ts_arr,           0,   ts_ind},
      {SQL_PARAM_INPUT,  SQL_C_CHAR,     SQL_VARCHAR,         99,       0,          varchar_arr,      max_test_char_len, varchar_ind},
      {SQL_PARAM_INPUT,  SQL_C_CHAR,     SQL_WVARCHAR,        99,       0,          nchar_arr,        max_test_char_len, nchar_ind},
      {SQL_PARAM_INPUT,  SQL_C_SBIGINT,  SQL_BIGINT,          99,       0,          i64_arr,          max_test_char_len, i64_ind},
  };

  for (int i = 0; i < ARRAY_SIZE; ++i) {
    ts_arr[i] = current_10ms * 10000 + i;
    snprintf(varchar_arr[i], max_test_char_len, "abcd%d", i + 9);
    varchar_arr_len[i] = (int32_t)strlen(varchar_arr[i]);
    varchar_ind[i] = SQL_NTS;
    snprintf(nchar_arr[i], max_test_char_len, "bt%d", i);
    nchar_arr_len[i] = (int32_t)strlen(nchar_arr[i]);
    nchar_ind[i] = SQL_NTS;
    i64_arr[i] = 54321 + i;
  }

  if( mode == 1){
    reset_stmt();
    SQLHANDLE hstmt = link_info->ctx.hstmt;
    SQLULEN nr_paramset_size = ARRAY_SIZE;
    SQLUSMALLINT param_status_arr[ARRAY_SIZE] = { 0 };
    SQLULEN nr_params_processed = 0;
    memset(param_status_arr, 0, ARRAY_SIZE * sizeof(SQLUSMALLINT));

    char* buf = "insert into tx1 (ts,vname,wname,bi) values (?,?,?,?)";
    //X("SQL:%s", buf);


    r = SQLPrepare(hstmt, (SQLCHAR*)buf, SQL_NTS);
    D("SQLPrepare result:%d", r);
    CHKSTMTR(hstmt, r);
    if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;

    r = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_PARAM_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN, 0);
    D("CALL_SQLSetStmtAttr SQL_ATTR_PARAM_BIND_TYPE result:%d", r);

    r = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER)(uintptr_t)nr_paramset_size, 0);
    D("CALL_SQLSetStmtAttr SQL_ATTR_PARAMSET_SIZE result:%d", r);

    r = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_PARAM_STATUS_PTR, param_status_arr, 0);
    D("CALL_SQLSetStmtAttr SQL_ATTR_PARAM_STATUS_PTR result:%d", r);

    r = CALL_SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMS_PROCESSED_PTR, &nr_params_processed, 0);
    D("CALL_SQLSetStmtAttr SQL_ATTR_PARAMS_PROCESSED_PTR result:%d", r);
    clock_t t1_1 = clock();
    for (int j = 0; j < 4; ++j) {
      const param_t* param = params + j;
      r = SQLBindParameter(hstmt, (SQLUSMALLINT)j + 1,
        param->InputOutputType,
        param->ValueType,
        param->ParameterType,
        param->ColumnSize,
        param->DecimalDigits,
        param->ParameterValuePtr,
        param->BufferLength,
        param->StrLen_or_IndPtr);
      D("SQLBindParameter %dth column result:%d", (int)j, r);
      CHKSTMTR(hstmt, r);
    }

    r = CALL_SQLExecute(hstmt);
    clock_t t1_2 = clock();
    elapsed_time = (double)(t1_2 - t1_1) / CLOCKS_PER_SEC;

    D("CALL_SQLExecute result:%d.", r);
    CHKSTMTR(hstmt, r);
    if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) return -1;

  }

  if (mode == 2) {
    int32_t int64Len[ARRAY_SIZE] = { sizeof(int64_t) };
    char is_null[ARRAY_SIZE] = { 0 };
    TAOS_MULTI_BIND params[4];
    params[0].buffer_type = TSDB_DATA_TYPE_TIMESTAMP;
    params[0].buffer_length = sizeof(int64_t);
    params[0].buffer = ts_arr;
    params[0].length = int64Len;
    params[0].is_null = is_null;
    params[0].num = ARRAY_SIZE;

    params[1].buffer_type = TSDB_DATA_TYPE_VARCHAR;
    params[1].buffer_length = max_test_char_len;
    params[1].buffer = varchar_arr;
    params[1].length = varchar_arr_len;
    params[1].is_null = is_null;
    params[1].num = ARRAY_SIZE;

    params[2].buffer_type = TSDB_DATA_TYPE_NCHAR;
    params[2].buffer_length = max_test_char_len;
    params[2].buffer = nchar_arr;
    params[2].length = nchar_arr_len;
    params[2].is_null = is_null;
    params[2].num = ARRAY_SIZE;

    params[3].buffer_type = TSDB_DATA_TYPE_BIGINT;
    params[3].buffer_length = sizeof(int64_t);
    params[3].buffer = i64_arr;
    params[3].length = int64Len;
    params[3].is_null = is_null;
    params[3].num = ARRAY_SIZE;
    {
      TAOS * taos = taos_connect("192.168.1.98:6030", "root", "taosdata", NULL, 0);
      if (taos == NULL) {
        printf("failed to connect to server, reason:%s\n", "null taos" /*taos_errstr(taos)*/);
        exit(1);
      }
      TAOS_RES* res = taos_query(taos, "use meter;");
      if (taos_errno(res) != 0) {
        printf("failed to taos_query, reason:%s\n", taos_errstr(res));
      }

      TAOS_STMT* stmt = taos_stmt_init(taos);
      // prepare

      clock_t t1_1 = clock();
      const char* sql = "insert into tx2 (ts,vname,wname,bi) values (?,?,?,?)";
      int code = taos_stmt_prepare(stmt, sql, 0);
      checkErrorCode(stmt, code, "failed to execute taos_stmt_prepare");

      code = taos_stmt_bind_param_batch(stmt, params); // bind batch
      checkErrorCode(stmt, code, "failed to execute taos_stmt_bind_param_batch");
      code = taos_stmt_add_batch(stmt);  // add batch
      checkErrorCode(stmt, code, "failed to execute taos_stmt_add_batch");
      // execute
      code = taos_stmt_execute(stmt);
      checkErrorCode(stmt, code, "failed to execute taos_stmt_execute");

      clock_t t1_2 = clock();
      elapsed_time = (double)(t1_2 - t1_1) / CLOCKS_PER_SEC;
      int affectedRows = taos_stmt_affected_rows(stmt);
      D("successfully inserted %d rows", affectedRows);
    }
  }

  return elapsed_time;
}

static int case_18_2() {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);
  char sql[1024];
  sql[0] = '\0';
  strcpy(sql, "drop table if exists tx2;");
  CHK1(exec_sql, sql, 0);

  sql[0] = '\0';
  strcpy(sql, "create table if not exists tx2 (ts timestamp,vname varchar(10),wname nchar(10),bi bigint);");
  CHK1(exec_sql, sql, 0);
  
  clock_t t1 = clock();
  time_t currentTime = time(NULL);
  double elapsed_time = 0;
  for (int i = 0; i < count_1000; ++i) {
    elapsed_time += case_18_helper(currentTime * 100 + i, 2);
  }

  clock_t t2 = clock();
  elapsed_time = (double)(t2 - t1) / CLOCKS_PER_SEC;
  X("taosc write %d times, batch size %d : % f seconds", count_1000, ARRAY_SIZE, elapsed_time);
  return 0;
}

static int case_18_1() {
  CHK0(create_sql_connect, 0);
  CHK1(use_db, test_db, 0);
  char sql[1024];
  sql[0] = '\0';
  strcpy(sql, "drop table if exists tx1;");
  CHK1(exec_sql, sql, 0);
  sql[0] = '\0';
  strcpy(sql, "create table if not exists tx1 (ts timestamp,vname varchar(10),wname nchar(10),bi bigint);");
  CHK1(exec_sql, sql, 0);

  clock_t t1 = clock();
  time_t currentTime = time(NULL);
  double elapsed_time = 0;
  for (int i = 0; i < count_1000; ++i) {
    elapsed_time += case_18_helper(currentTime * 100 + i, 1);
  }
  clock_t t2 = clock();
  elapsed_time = (double)(t2 - t1) / CLOCKS_PER_SEC;
  X("taos-odbc write %d times, batch size %d : % f seconds", count_1000, ARRAY_SIZE, elapsed_time);
  return 0;
}

static const int default_supported = 1;
static const int default_unsupported = 2;
static bool isTestCase(int argc, char* argv[], const char* test_case, const int support) {
  if (argc == 0 && support == default_supported) return true;
  if (argc == 0 && support == default_unsupported) return false;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], test_case) == 0) return true;
  }
  return false;
}

static int example() {
  SQLHANDLE henv = SQL_NULL_HANDLE;
  SQLHANDLE hdbc = SQL_NULL_HANDLE;
  SQLHANDLE hstmt = SQL_NULL_HANDLE;

  SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
  SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
  SQLSetEnvAttr(henv, SQL_ATTR_CONNECTION_POOLING, (SQLPOINTER)SQL_CP_ONE_PER_DRIVER, 0);

  SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

  // create a connection to tdengine data source
  SQLCHAR OutConnectionString[1024] = {0};
  SQLSMALLINT StringLength2 = 0;
 
  char* conn_str = "DSN=TAOS_ODBC_DSN; server=127.0.0.1:6030; uid=root; pwd=taosdata; db=meter";
  SQLDriverConnect(hdbc,
                   NULL,
                   (SQLCHAR*)conn_str,
                   (SQLSMALLINT)strlen(conn_str),
                   OutConnectionString,
                   sizeof(OutConnectionString),
                   &StringLength2,
                   SQL_DRIVER_NOPROMPT);

  SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);

  char* create_stable = "CREATE TABLE `meters` (`ts` TIMESTAMP, `current` FLOAT, `voltage` INT, `phase` FLOAT) \
         TAGS (`groupid` INT, `location` BINARY(24))";
  SQLExecDirect(hstmt, (SQLCHAR*)create_stable, SQL_NTS);

  // create test table
  char* create_table_sql = "CREATE TABLE `d0` USING `meters` TAGS(0, 'California.LosAngles')";
  SQLExecDirect(hstmt, (SQLCHAR*)create_table_sql, SQL_NTS);
 
  // write data into test table
  char insert_sql[256];
  sprintf(insert_sql, "INSERT INTO `d0` values(now - 10s, 10, 116, 0.32)");
  SQLExecDirect(hstmt, (SQLCHAR*)insert_sql, SQL_NTS);
  SQLLEN numberOfrows;
  SQLRowCount(hstmt, &numberOfrows);
  printf("insert count: "SQLLEN_FORMAT"\n", numberOfrows);

  // reset cursor
  SQLCloseCursor(hstmt);

  // read data from table
  char select_sql[256];
  sprintf(select_sql, "select ts, current, voltage from d0");
  SQLExecDirect(hstmt, (SQLCHAR*)select_sql, SQL_NTS);

  int row = 0;
  SQLSMALLINT numberOfColumns;
  CALL_SQLNumResultCols(hstmt, &numberOfColumns);

  while (SQLFetch(hstmt) == SQL_SUCCESS) {
    row++;
    for (int i = 1; i <= numberOfColumns; i++) {
      SQLCHAR columnData[256];
      SQLLEN indicator;
      SQLGetData(hstmt, i, SQL_C_CHAR, columnData, sizeof(columnData), &indicator);
      if (indicator != SQL_NULL_DATA) {
        printf("Row:%d Column %d: %s \n", row, i, columnData);
      }
    }
  }

  // close handle
  SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
  SQLDisconnect(hdbc);
  SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, henv);

  return 0;
}

static int run(int argc, char* argv[]) {
  init();

  if (isTestCase(argc, argv, "example", default_supported)) CHK0(example, 0);
  if (isTestCase(argc, argv, "case_1", default_supported)) CHK0(case_1, 0);
  if (isTestCase(argc, argv, "case_2", default_supported)) CHK0(case_2, 0);
  if (isTestCase(argc, argv, "case_3", default_supported)) CHK0(case_3, 0);
  if (isTestCase(argc, argv, "case_4", default_supported)) CHK0(case_4, 0);
  if (isTestCase(argc, argv, "case_5", default_supported)) CHK0(case_5, 0);
  if (isTestCase(argc, argv, "case_6", default_supported)) CHK0(case_6, 0);
  if (isTestCase(argc, argv, "case_7", default_supported)) CHK0(case_7, 0);
  if (isTestCase(argc, argv, "case_8", default_supported)) CHK0(case_8, 0);
  if (isTestCase(argc, argv, "case_9", default_supported)) CHK0(case_9, 0);
  if (isTestCase(argc, argv, "case_9_1", default_supported)) CHK0(case_9_1, 0);
  if (isTestCase(argc, argv, "case_10", default_supported)) CHK0(case_10, 0);
  if (isTestCase(argc, argv, "case_11", default_supported)) CHK0(case_11, 0);
  if (isTestCase(argc, argv, "case_12", default_supported)) CHK0(case_12, 0);
  if (isTestCase(argc, argv, "case_13", default_supported)) CHK0(case_13, 0);
  if (isTestCase(argc, argv, "case_14", default_supported)) CHK0(case_14, 0);
  if (isTestCase(argc, argv, "case_15", default_supported)) CHK0(case_15, 0);
  if (isTestCase(argc, argv, "case_16", default_supported)) CHK0(case_16, 0);
  if (isTestCase(argc, argv, "case_17", default_supported)) CHK0(case_17, 0);

  if (isTestCase(argc, argv, "db_test", default_unsupported)) CHK0(db_test, 0);
    if (isTestCase(argc, argv, "case_3_1", default_unsupported)) CHK0(case_3_1, 0);
  if (isTestCase(argc, argv, "case_18_2", default_unsupported)) CHK0(case_18_2, 0);
  if (isTestCase(argc, argv, "case_18_1", default_unsupported)) CHK0(case_18_1, 0);

  X("The test finished successfully.");
  return 0;
}

static int cloud_test(int argc, char* argv[]) {
  CHK0(init_henv, 0);
  CHK0(create_driver_conn, 0);
  CHK0(show_tables, 0);
  if (isTestCase(argc, argv, "case_3", default_supported)) CHK0(case_3, 0);

  X("Cloud_test finished successfully.");
  return 0;
}

static int mysql_help_test(int argc, char* argv[]) {
  CHK0(init, 0);
  if (isTestCase(argc, argv, "case_2", default_supported)) CHK0(case_2, 0);
  if (isTestCase(argc, argv, "case_4", default_supported)) CHK0(case_4, 0);
  if (isTestCase(argc, argv, "case_9", default_supported)) CHK0(case_9, 0);
  if (isTestCase(argc, argv, "case_9_1", default_supported)) CHK0(case_9_1, 0);
  if (isTestCase(argc, argv, "case_10", default_supported)) CHK0(case_10, 0);
  if (isTestCase(argc, argv, "case_13", default_supported)) CHK0(case_13, 0);

  X("The test finished successfully.");
  return 0;
}

static int server_help_test(int argc, char* argv[]) {
  CHK0(init, 0);
  if (isTestCase(argc, argv, "db_test", default_supported)) CHK0(db_test, 0);
  if (isTestCase(argc, argv, "case_9", default_supported)) CHK0(case_9, 0);
  if (isTestCase(argc, argv, "case_9_1", default_supported)) CHK0(case_9_1, 0);
  if (isTestCase(argc, argv, "case_10", default_supported)) CHK0(case_10, 0);
  if (isTestCase(argc, argv, "case_13", default_supported)) CHK0(case_13, 0);

  X("SQLServer help test skip.");
  return 0;
}

int main(int argc, char* argv[])
{
  int r = 0;
  do {
    if (argc >= 2 && strcmp(argv[1], LINKMODENATIVE) == 0) {
      link_info = &_native_link;
      r = run(argc - 1, argv + 1);
    }
    else if (argc >= 2 && strcmp(argv[1], LINKMODEWS) == 0) {
      link_info = &_websocket_link;
      r = run(argc - 1, argv + 1);
    }
    else if (argc >= 2 && strcmp(argv[1], LINKMODECLOUD) == 0) {
      link_info = &_cloud_link;
      r = cloud_test(argc - 1, argv + 1);
    }
    else if (argc >= 2 && strcmp(argv[1], LINKMODEMYSQL) == 0) {
      link_info = &_mysql_link;
      r = mysql_help_test(argc - 1, argv + 1);
    }
    else if (argc >= 2 && strcmp(argv[1], LINKMODESQLSERVER) == 0) {
      link_info = &_sqlserver_link;
      r = server_help_test(argc - 1, argv + 1);
    }
    else {
      link_info = &_native_link;
      r = run(argc, argv);
      if (r != 0) break;
      link_info = &_websocket_link;
      r = run(argc, argv);
    }
  } while (0);

  fprintf(stderr, "==%s==\n", r ? "failure" : "success");
  return !!r;
}
