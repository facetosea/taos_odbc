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

#include "conn.h"
#include "env.h"
#include "errs.h"
#include "helpers.h"
#include "parser.h"
#include "utils.h"

#include <string.h>

static int test_case1(void)
{
  env_t *env = env_create();

  conn_t *conn = conn_create(env);
  conn_unref(conn);

  env_unref(env);

  parser_param_t param = {0};
  // param.debug_flex = 1;
  // param.debug_bison = 1;

  const char *connection_strs[] = {
    " driver = {    ax bcd ; ;   }; dsn=server; uid=xxx; pwd=yyy",
    // https://www.connectionstrings.com/dsn/
    "DSN=myDsn;Uid=myUsername;Pwd=;",
    "FILEDSN=c:\\myDsnFile.dsn;Uid=myUsername;Pwd=;",
    "Driver={any odbc driver's name};OdbcKey1=someValue;OdbcKey2=someValue;",
    // http://lunar.lyris.com/help/Content/sample_odbc_connection_str.html
    "Driver={SQL Server};Server=lmtest;Database=lmdb;Uid=sa;Pwd=pass",
    "DSN=DSN_Name;Server=lmtest;Uid=lmuser;Pwd=pass",
    "Driver={MySQL ODBC 3.51 driver};server=lmtest;database=lmdb;uid=mysqluser;pwd=pass;",
    "Driver={MySQL ODBC 3.51 driver};server=lmtest;database=lmdb;uid=mysqluser;pwd=pass;",
    "Driver={MySQL ODBC 3.51 driver};server=lm.test;database=lmdb;uid=mysqluser;pwd=pass;",
    "Driver={MySQL ODBC 3.51 driver};server=lmtest:378;database=lmdb;uid=mysqluser;pwd=pass;",
    "Driver={MySQL ODBC 3.51 driver};server=lmtest:378378378378;database=lmdb;uid=mysqluser;pwd=pass;",
  };
  for (size_t i=0; i<sizeof(connection_strs)/sizeof(connection_strs[0]); ++i) {
    const char *s = connection_strs[i];
    int r = parser_parse(s, strlen(s), &param);
    parser_param_release(&param);
    if (r) return -1;
  }

  return 0;
}

static int _wildcard_match(const char *ex, const char *str)
{
  int r;
  wildex_t *wild = NULL;

  r = wildcomp(&wild, ex);
  if (r) {
    E("failed to compile wildcard `%s`", ex);
    return -1;
  }

  r = wildexec(wild, str);
  if (r) E("`%s` does not match by `%s`", str, ex);

  wildfree(wild);

  return r ? -1 : 0;
}

static int test_case2(void)
{
  int r = 0;

  r = _wildcard_match("hello", "hello");
  if (r) return -1;

  r = _wildcard_match("%", "hello");
  if (r) return -1;

  r = _wildcard_match("%%%%%%%%%%%%%%%%", "hello");
  if (r) return -1;

  r = _wildcard_match("_", "h");
  if (r) return -1;

  r = _wildcard_match("_____", "hello");
  if (r) return -1;

  r = _wildcard_match("_%lo", "hello");
  if (r) return -1;

  r = _wildcard_match("_%l%o", "hello");
  if (r) return -1;

  r = _wildcard_match("_%el%o", "hello");
  if (r) return -1;

  r = _wildcard_match("_%ll%o", "hello");
  if (r) return -1;

  return 0;
}

int main(void)
{
  if (0) {
    const char *path = "/Users/xxh/foo/bar";
    char buf[PATH_MAX+1];
    char *p = tod_dirname(path, buf, sizeof(buf));
    D("\n%s[%p]\n%s[%p]\n%s[%p]\n===", path, path, buf, buf, p, p);
    return 1;
  }

  int r = 0;

  r = test_case1();
  if (r) return 1;

  r = test_case2();
  if (r) return 1;

  return 0;
}

