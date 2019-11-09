/*
** gettext wrapper
** See Agreement in LICENSE
** Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
*/

#define lgettext_c
#define LUA_CORE

#include "lprefix.h"

#include "lua.h"

#ifdef LUAEX_I18N
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>

#ifdef _WIN32
#include <windows.h>
#else
/*#include <locale.h>*/
#endif

#ifndef MAX_PATH
#define MAX_PATH 1024
#endif


static char *data = NULL;
static uint32_t ldata, ndata;
static uint32_t *ostring, *tstring;


LUA_API int lua_lopen (const char *locale) {
  lua_lclose();
  char buf[MAX_PATH];
  FILE *file;
  if (locale) {
    snprintf(buf, sizeof(buf), "../share/i18n/%s.mo", locale);
    file = fopen(buf, "rb");
  } else {
  #ifdef _WIN32
    char lbuf[8];
    char cbuf[12];
    LCID lcid = GetUserDefaultLCID();
    GetLocaleInfo(lcid, LOCALE_SISO639LANGNAME, lbuf, sizeof(lbuf));
    GetLocaleInfo(lcid, LOCALE_SISO3166CTRYNAME, cbuf, sizeof(cbuf));
    snprintf(buf, sizeof(buf), "../share/i18n/%s_%s.mo", lbuf, cbuf);
    file = fopen(buf, "rb");
    if (file == NULL) {
      snprintf(buf, sizeof(buf), "../share/i18n/%s.mo", lbuf);
      file = fopen(buf, "rb");
    }
  #else
    return 0;
  #endif
  }
  if (file) {
    uint32_t value;
    fread(&value, sizeof(uint32_t), 1, file);
    if (value == 0x950412de) { /* magic signature must be 0x950412de */
      fread(&value, sizeof(uint32_t), 1, file);
      if (value == 0) { /* format must be 0 */
        fread(&ndata, sizeof(uint32_t), 1, file);
        if (ndata > 0) { /* number of strings must be > 0 */
          uint32_t os, ts;
          fread(&os, sizeof(uint32_t), 1, file);  /* offset of table with original strings */
          fread(&ts, sizeof(uint32_t), 1, file);  /* offset of table with translation strings */
          if (os >= 28 && ts >= 28) {
            fseek(file, 0, SEEK_END);
            ldata = (uint32_t) ftell(file);
            data = (char *) malloc(ldata);
            if (data) {
              fseek(file, 0, SEEK_SET);
              fread(data, ldata, 1, file);
              fclose(file);
              ostring = (uint32_t *) &data[os]; /* pointer of table with original strings */
              tstring = (uint32_t *) &data[ts]; /* pointer of table with translation strings */
              return 1;
            }
          }
        }
      }
    }
    fclose(file);
  }
  return 0;
}


LUA_API void lua_lclose (void) {
  if (data) {
    free(data);
    data = NULL;
  }
}


LUA_API const char * lua_gettext (const char *s) {
  if (data && s) {
    uint32_t i, l = (uint32_t) strlen(s);
    uint32_t *ostr = ostring;
    uint32_t *tstr = tstring;
    for (i = 0; i < ndata; i++) {
      /* str[0] - length string, str[1] - offset string in data */
      if (ostr[0] == l && ostr[0] + ostr[1] < ldata) {
        if (memcmp(&data[ostr[1]], s, l) == 0) {
          if (tstr[0] + tstr[1] < ldata)
            return &data[tstr[1]];
          break;
        }
      }
      ostr += 2;
      tstr += 2;
    }
  }
  return s;
}


#endif
