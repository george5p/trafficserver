/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

/***************************************************************************
 LogFormat.cc


 ***************************************************************************/

#include "ink_config.h"
#include "ink_unused.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "INK_MD5.h"

#include "Error.h"
#include "SimpleTokenizer.h"

#include "LogUtils.h"
#include "LogFile.h"
#include "LogField.h"
#include "LogFilter.h"
#include "LogFormat.h"
#include "LogHost.h"
#include "LogBuffer.h"
#include "LogObject.h"
#include "LogConfig.h"
#include "Log.h"

// class variables
//
bool
  LogFormat::m_tagging_on = false;

// predefined formats
//
const char *const
  LogFormat::squid_format =
  "%<cqtq> %<ttms> %<chi> %<crc>/%<pssc> %<psql> %<cqhm> %<cquc> " "%<caun> %<phr>/%<pqsn> %<psct> %<xid>";

const char *const
  LogFormat::common_format = "%<chi> - %<caun> [%<cqtn>] \"%<cqtx>\" %<pssc> %<pscl>";

const char *const
  LogFormat::extended_format =
  "%<chi> - %<caun> [%<cqtn>] \"%<cqtx>\" %<pssc> %<pscl> "
  "%<sssc> %<sscl> %<cqbl> %<pqbl> %<cqhl> %<pshl> %<pqhl> " "%<sshl> %<tts>";

const char *const
  LogFormat::extended2_format =
  "%<chi> - %<caun> [%<cqtn>] \"%<cqtx>\" %<pssc> %<pscl> "
  "%<sssc> %<sscl> %<cqbl> %<pqbl> %<cqhl> %<pshl> %<pqhl> " "%<sshl> %<tts> %<phr> %<cfsc> %<pfsc> %<crc>";

/*-------------------------------------------------------------------------
  LogFormat::setup
  -------------------------------------------------------------------------*/

void
LogFormat::setup(const char *name, const char *format_str, unsigned interval_sec)
{
  if (name == NULL && format_str == NULL) {
    Note("No name or field symbols for this format");
    m_valid = false;
  } else {
    const char *tag = " %<phn>";
    const size_t m_format_str_size = strlen(format_str) + (m_tagging_on ? strlen(tag) : 0) + 1;
    m_format_str = (char *) xmalloc(m_format_str_size);
    ink_strncpy(m_format_str, format_str, m_format_str_size);
    if (m_tagging_on) {
      Note("Log tagging enabled, adding %%<phn> field at the end of " "format %s", name);
      strncat(m_format_str, tag, m_format_str_size - strlen(m_format_str) - 1);
    };

    char *printf_str = NULL;
    char *fieldlist_str = NULL;
    int nfields = parse_format_string(m_format_str, &printf_str,
                                      &fieldlist_str);
    if (nfields > (m_tagging_on ? 1 : 0)) {
      init_variables(name, fieldlist_str, printf_str, interval_sec);
    } else {
      Note("Format %s encountered an error parsing the symbol string "
           "\"%s\", symbol string contains no fields", ((name) ? name : "no-name"), format_str);
      m_valid = false;
    }

    xfree(fieldlist_str);
    xfree(printf_str);
  }
}

/*-------------------------------------------------------------------------
  LogFormat::id_from_name
  -------------------------------------------------------------------------*/

int32 LogFormat::id_from_name(const char *name)
{
  int32
    id = 0;
  if (name) {
    INK_MD5
      name_md5;
    name_md5.encodeBuffer(name, (int)::strlen(name));
#if (HOST_OS == linux)
    /* Mask most signficant bit so that return value of this function
     * is not sign extended to be a negative number.
     * This problem is only known to occur on Linux which
     * is a 32-bit OS.
     */
    id = (int32) name_md5.fold() & 0x7fffffff;
#else
    id = (int32) name_md5.fold();
#endif
  }
  return id;
}

/*-------------------------------------------------------------------------
  LogFormat::init_variables
  -------------------------------------------------------------------------*/

void
LogFormat::init_variables(const char *name, const char *fieldlist_str, const char *printf_str, unsigned interval_sec)
{

  m_field_count = parse_symbol_string(fieldlist_str, &m_field_list, &m_aggregate);

  if (m_field_count == 0) {
    m_valid = false;
  } else if (m_aggregate && !interval_sec) {
    Note("Format for aggregate operators but no interval " "was specified");
    m_valid = false;
  } else {
    if (m_aggregate) {
      m_agg_marshal_space = (char *) xmalloc(m_field_count * MIN_ALIGN);
    }

    if (m_name_str) {
      xfree(m_name_str);
      m_name_str = NULL;
      m_name_id = 0;
    }
    if (name) {
      m_name_str = xstrdup(name);
      m_name_id = id_from_name(m_name_str);
    }

    if (m_fieldlist_str) {
      xfree(m_fieldlist_str);
      m_fieldlist_str = NULL;
      m_fieldlist_id = 0;
    }
    if (fieldlist_str) {
      m_fieldlist_str = xstrdup(fieldlist_str);
      m_fieldlist_id = id_from_name(m_fieldlist_str);
    }

    m_printf_str = xstrdup(printf_str);
    m_interval_sec = interval_sec;
    m_interval_next = LogUtils::timestamp();

    m_valid = true;
  }
}


/*-------------------------------------------------------------------------
  LogFormat::LogFormat

  This constructor builds a LogFormat object for one of the pre-defined
  format types.  Only the type is needed since we know everything else we
  need to about the pre-defined formats.
  -------------------------------------------------------------------------*/

LogFormat::LogFormat(LogFormatType type)
:m_interval_sec(0)
  , m_interval_next(0)
  , m_agg_marshal_space(NULL)
  , m_valid(false)
  , m_name_str(NULL)
  , m_name_id(0)
  , m_fieldlist_str(NULL)
  , m_fieldlist_id(0)
  , m_field_count(0)
  , m_printf_str(NULL)
  , m_aggregate(false)
  , m_format_str(NULL)
{
  switch (type) {
  case SQUID_LOG:
    setup("squid", (char *) squid_format);
    break;
  case COMMON_LOG:
    setup("common", (char *) common_format);
    break;
  case EXTENDED_LOG:
    setup("extended", (char *) extended_format);
    break;
  case EXTENDED2_LOG:
    setup("extended2", (char *) extended2_format);
    break;
    // For text logs, there is no format string; we'll simply log the
    // entire entry as a string without any field substitutions.  To
    // indicate this, the format_str will be NULL
    //
  case TEXT_LOG:
    m_name_str = xstrdup("text");
    m_valid = true;
    break;
  default:
    Note("Invalid log format type %d", type);
    m_valid = false;
  }
  m_format_type = type;
}

/*-------------------------------------------------------------------------
  LogFormat::LogFormat

  This is the general ctor that builds a LogFormat object from the data
  provided.  In this case, the "fields" character string is a printf-style
  string where the field symbols are represented within the string in the
  form %<symbol>.
  -------------------------------------------------------------------------*/

LogFormat::LogFormat(const char *name, const char *format_str, unsigned interval_sec)
  :
m_interval_sec(0)
  ,
m_interval_next(0)
  ,
m_agg_marshal_space(NULL)
  ,
m_valid(false)
  ,
m_name_str(NULL)
  ,
m_name_id(0)
  ,
m_fieldlist_str(NULL)
  ,
m_fieldlist_id(0)
  ,
m_field_count(0)
  ,
m_printf_str(NULL)
  ,
m_aggregate(false)
  ,
m_format_str(NULL)
{
  setup(name, format_str, interval_sec);
  m_format_type = CUSTOM_LOG;
}

//-----------------------------------------------------------------------------
// This constructor is used only in Log::match_logobject
//
// It is awkward because it does not take a format_str but a fieldlist_str
// and a printf_str. These should be derived from a format_str but a
// LogBufferHeader does not store the format_str, if it did, we could probably
// delete this.
//
LogFormat::LogFormat(const char *name, const char *fieldlist_str, const char *printf_str, unsigned interval_sec)
  :
m_interval_sec(0)
  ,
m_interval_next(0)
  ,
m_agg_marshal_space(NULL)
  ,
m_valid(false)
  ,
m_name_str(NULL)
  ,
m_name_id(0)
  ,
m_fieldlist_str(NULL)
  ,
m_fieldlist_id(0)
  ,
m_field_count(0)
  ,
m_printf_str(NULL)
  ,
m_aggregate(false)
  ,
m_format_str(NULL)
{
  init_variables(name, fieldlist_str, printf_str, interval_sec);
  m_format_type = CUSTOM_LOG;
}

/*-------------------------------------------------------------------------
  LogFormat::LogFormat

  This is the copy ctor, needed for copying lists of Format objects.
  -------------------------------------------------------------------------*/

LogFormat::LogFormat(const LogFormat & rhs)
  :
m_interval_sec(0)
  ,
m_interval_next(0)
  ,
m_agg_marshal_space(NULL)
  ,
m_valid(rhs.m_valid)
  ,
m_name_str(NULL)
  ,
m_name_id(0)
  ,
m_fieldlist_str(NULL)
  ,
m_fieldlist_id(0)
  ,
m_field_count(0)
  ,
m_printf_str(NULL)
  ,
m_aggregate(false)
  ,
m_format_str(NULL)
  ,
m_format_type(rhs.m_format_type)
{
  if (m_valid) {
    if (m_format_type == TEXT_LOG) {
      m_name_str = xstrdup(rhs.m_name_str);
    } else {
      m_format_str = rhs.m_format_str ? xstrdup(rhs.m_format_str) : 0;
      init_variables(rhs.m_name_str, rhs.m_fieldlist_str, rhs.m_printf_str, rhs.m_interval_sec);
    }
  }
}

/*-------------------------------------------------------------------------
  LogFormat::~LogFormat
  -------------------------------------------------------------------------*/

LogFormat::~LogFormat()
{
  xfree(m_name_str);
  xfree(m_fieldlist_str);
  xfree(m_printf_str);
  xfree(m_agg_marshal_space);
  xfree(m_format_str);
  m_valid = false;
}

#ifndef TS_MICRO
/*-------------------------------------------------------------------------
  LogFormat::format_from_specification

  This routine is obsolete as of 3.1, but will be kept around to preserve
  the old log config file option.

  This (static) function examines the given log format specification string
  and builds a new LogFormat object if the format specification is valid.
  On success, a pointer to a LogFormat object allocated from the heap (with
  new) will be returned.  On error, NULL is returned.
  -------------------------------------------------------------------------*/

LogFormat *
LogFormat::format_from_specification(char *spec, char **file_name, char **file_header, LogFileFormat * file_type)
{
  LogFormat *format;
  char *token;
  int format_id;
  char *format_name, *format_str;

  ink_assert(file_name != NULL);
  ink_assert(file_header != NULL);
  ink_assert(file_type != NULL);

  SimpleTokenizer tok(spec, ':');

  //
  // Divide the specification string into tokens using the ':' as a
  // field separator.  There are currently eight (8) tokens that comprise
  // a format specification.  Verify each of the token values and if
  // everything looks ok, then build the LogFormat object.
  //
  // First should be the "format" keyword that says this is a format spec.
  //
  token = tok.getNext();
  if (token == NULL) {
    Debug("log2-format", "token expected");
    return NULL;
  }
  if (strcasecmp(token, "format") == 0) {
    Debug("log2-format", "this is a format");
  } else {
    Debug("log2-format", "should be 'format'");
    return NULL;
  }

  //
  // Next should be the word "enabled" or "disabled", which indicates
  // whether we should care about this format or not.
  //
  token = tok.getNext();
  if (token == NULL) {
    Debug("log2-format", "token expected");
    return NULL;
  }
  if (!strcasecmp(token, "disabled")) {
    Debug("log2-format", "format not enabled, skipping ...");
    return NULL;
  } else if (!strcasecmp(token, "enabled")) {
    Debug("log2-format", "enabled format");
  } else {
    Debug("log2-format", "should be 'enabled' or 'disabled', not %s", token);
    return NULL;
  }

  //
  // Next should be the numeric format identifier
  //
  token = tok.getNext();
  if (token == NULL) {
    Debug("log2-format", "token expected");
    return NULL;
  }
  format_id = atoi(token);
  // NOW UNUSED !!!

  //
  // Next should be the format name
  //
  token = tok.getNext();
  if (token == NULL) {
    Debug("log2-format", "token expected");
    return NULL;
  }
  format_name = token;

  //
  // Next should be the printf-stlye format symbol string
  //
  token = tok.getNext();
  if (token == NULL) {
    Debug("log2-format", "token expected");
    return NULL;
  }
  format_str = token;

  //
  // Next should be the file name for the log
  //
  token = tok.getNext();
  if (token == NULL) {
    Debug("log2-format", "token expected");
    return NULL;
  }
  *file_name = xstrdup(token);

  //
  // Next should be the file type, either "ASCII" or "BINARY"
  //
  token = tok.getNext();
  if (token == NULL) {
    Debug("log2-format", "token expected");
    return NULL;
  }
  if (!strcasecmp(token, "ASCII")) {
    *file_type = ASCII_LOG;
  } else if (!strcasecmp(token, "BINARY")) {
    *file_type = BINARY_LOG;
  } else {
    Debug("log2-format", "%s is not a valid file format (ASCII or BINARY)", token);
    return NULL;
  }

  //
  // the rest should be the file header
  //
  token = tok.getRest();
  if (token == NULL) {
    Debug("log2-format", "token expected");
    return NULL;
  }
  // set header to NULL if "none" was specified (a NULL header means
  // "write no header" to the rest of the logging system)
  //
  *file_header = strcmp(token, "none") == 0 ? NULL : xstrdup(token);

  Debug("log2-format", "custom:%d:%s:%s:%s:%d:%s", format_id, format_name, format_str, *file_name, *file_type, token);

  format = NEW(new LogFormat(format_name, format_str));
  ink_assert(format != NULL);
  if (!format->valid()) {
    delete format;
    return NULL;
  }

  return format;
}
#endif // TS_MICRO

/*-------------------------------------------------------------------------
  LogFormat::parse_symbol_string

  This function does the work of parsing a comma-separated symbol list and
  adding them to the LogFieldList that is provided.  The total number of
  fields added to the list is returned.
  -------------------------------------------------------------------------*/

int
LogFormat::parse_symbol_string(const char *symbol_string, LogFieldList *field_list, bool *contains_aggregates)
{
  char *sym_str;
  int field_count = 0;
  LogField *f;
  char *symbol, *name, *sym;
  LogField::Container container;
  LogField::Aggregate aggregate;

  if (symbol_string == NULL)
    return 0;
  ink_assert(field_list != NULL);
  ink_assert(contains_aggregates != NULL);

  *contains_aggregates = false; // we'll change if it does

  //
  // strtok will mangle the input string; we'll make a copy for that.
  //
  sym_str = xstrdup(symbol_string);
  symbol = strtok(sym_str, ",");

  while (symbol != NULL) {
    //
    // See if there is an aggregate operator, which will contain "()"
    //
    char *begin_paren = strchr(symbol, '(');
    if (begin_paren) {
      char *end_paren = strchr(symbol, ')');
      if (end_paren) {
        Debug("log2-agg", "Aggregate symbol: %s", symbol);
        *begin_paren = '\0';
        *end_paren = '\0';
        name = begin_paren + 1;
        sym = symbol;
        Debug("log2-agg", "Aggregate = %s, field = %s", sym, name);
        aggregate = LogField::valid_aggregate_name(sym);
        if (aggregate == LogField::NO_AGGREGATE) {
          Note("Invalid aggregate specification: %s", sym);
        } else {
          if (aggregate == LogField::eCOUNT && strcmp(name, "*") == 0) {
            f = Log::global_field_list.find_by_symbol("psql");
          } else {
            f = Log::global_field_list.find_by_symbol(name);
          }
          if (!f) {
            Note("Invalid field symbol %s used in aggregate " "operation", name);
          } else if (f->type() != LogField::sINT) {
            Note("Only single integer field types may be aggregated");
          } else {
            LogField *new_f = NEW(new LogField(*f));
            new_f->set_aggregate_op(aggregate);
            field_list->add(new_f, false);
            field_count++;
            *contains_aggregates = true;
            Debug("log2-agg", "Aggregate field %s(%s) added", sym, name);
          }
        }
      } else {
        Note("Invalid aggregate field specification: no trailing " "')' in %s", symbol);
      }
    }
    //
    // Now check for a container field, which starts with '{'
    //
    else if (*symbol == '{') {
      Debug("log2-format", "Container symbol: %s", symbol);
      f = NULL;
      char *name_end = strchr(symbol, '}');
      if (name_end != NULL) {
        name = symbol + 1;
        *name_end = 0;          // changes '}' to '\0'
        sym = name_end + 1;     // start of container symbol
        Debug("log2-format", "Name = %s, symbol = %s", name, sym);
        container = LogField::valid_container_name(sym);
        if (container == LogField::NO_CONTAINER) {
          Note("Invalid container specification: %s", sym);
        } else {
          f = NEW(new LogField(name, container));
          ink_assert(f != NULL);
          field_list->add(f, false);
          field_count++;
          Debug("log2-format", "Container field {%s}%s added", name, sym);
        }
      } else {
        Note("Invalid container field specification: no trailing " "'}' in %s", symbol);
      }
    }
    //
    // treat this like a regular field symbol
    //
    else {
      Debug("log2-format", "Regular field symbol: %s", symbol);
      f = Log::global_field_list.find_by_symbol(symbol);
      if (f != NULL) {
        field_list->add(f);
        field_count++;
        Debug("log2-format", "Regular field %s added", symbol);
      } else {
        Note("The log format symbol %s was not found in the " "list of known symbols.", symbol);
      }
    }

    //
    // Get the next symbol
    //
    symbol = strtok(NULL, ",");
  }

  xfree(sym_str);
  return field_count;
}

/*-------------------------------------------------------------------------
  LogFormat::parse_format_string

  This function will parse a custom log format string, which is a
  combination of printf characters and logging field names, separating this
  combined format string into a normal printf string and a fieldlist.  The
  number of logging fields parsed will be returned.  The two strings
  returned are allocated with xmalloc, and should be released by the
  caller.  The function returns -1 on error.

  For 3.1, I've added the ability to log summary information using
  aggregate operators SUM, COUNT, AVG, ...
  -------------------------------------------------------------------------*/

int
LogFormat::parse_format_string(const char *format_str, char **printf_str, char **fields_str)
{
  ink_assert(printf_str != NULL);
  ink_assert(fields_str != NULL);

  if (format_str == NULL) {
    *printf_str = *fields_str = NULL;
    return 0;
  }
  //
  // Since the given format string is a combination of the printf-string
  // and the field symbols, when we break it up into these two components
  // each is guaranteed to be smaller (or the same size) as the format
  // string.
  //
  unsigned len = (unsigned)::strlen(format_str);
  *printf_str = (char *) xmalloc(len + 1);
  *fields_str = (char *) xmalloc(len + 1);
  ink_assert(*printf_str != NULL && *fields_str != NULL);

  unsigned printf_pos = 0;
  unsigned fields_pos = 0;
  unsigned field_count = 0;
  unsigned field_len;
  unsigned start, stop;

  for (start = 0; start < len; start++) {
    //
    // Look for logging fields: %<field>
    //
    if ((format_str[start] == '%') && (start + 1 < len) && (format_str[start + 1] == '<')) {
      //
      // this is a field symbol designation; look for the
      // trailing '>'.
      //
      if (fields_pos > 0) {
        (*fields_str)[fields_pos++] = ',';
      }
      for (stop = start + 2; stop < len; stop++) {
        if (format_str[stop] == '>') {
          break;
        }
      }
      if (format_str[stop] == '>') {
        //
        // We found the termination for this field spec;
        // copy the field symbol to the symbol string and place a
        // LOG_FIELD_MARKER in the printf string.
        //
        field_len = stop - start - 2;
        memcpy(&(*fields_str)[fields_pos], &format_str[start + 2], field_len);
        fields_pos += field_len;
        (*printf_str)[printf_pos++] = LOG_FIELD_MARKER;
        ++field_count;
      } else {
        //
        // This was not a logging field spec after all, so copy it
        // over to the printf string as is.
        //
        memcpy(&(*printf_str)[printf_pos], &format_str[start], stop - start + 1);
        printf_pos += stop - start + 1;
      }
      start = stop;
    } else {
      //
      // This was not the start of a logging field spec, so simply
      // put this char into the printf_str.
      //
      (*printf_str)[printf_pos++] = format_str[start];
    }
  }

  //
  // Ok, now NULL terminate the strings and return the number of fields
  // actually found.
  //
  (*fields_str)[fields_pos] = '\0';
  (*printf_str)[printf_pos] = '\0';

  Debug("log2-format", "LogFormat::parse_format_string: field_count=%d, \"%s\", \"%s\"",
        field_count, *fields_str, *printf_str);
  return field_count;
}

/*-------------------------------------------------------------------------
  LogFormat::display

  Print out some info about this object.
  -------------------------------------------------------------------------*/

void
LogFormat::display(FILE * fd)
{
  static const char *types[] = {
    "SQUID_LOG",
    "COMMON_LOG",
    "EXTENDED_LOG",
    "EXTENDED2_LOG",
    "CUSTOM_LOG",
    "TEXT_LOG"
  };

  fprintf(fd, "--------------------------------------------------------\n");
  fprintf(fd, "Format : %s (%s) (%p), %d fields.\n", m_name_str, types[m_format_type], this, m_field_count);
  if (m_fieldlist_str) {
    fprintf(fd, "Symbols: %s\n", m_fieldlist_str);
    fprintf(fd, "Fields :\n");
    m_field_list.display(fd);
  } else {
    fprintf(fd, "Fields : None\n");
  }
  fprintf(fd, "--------------------------------------------------------\n");
}

void
LogFormat::displayAsXML(FILE * fd)
{
  if (valid()) {
    fprintf(fd,
            "<LogFormat>\n"
            "  <Name     = \"%s\"/>\n"
            "  <Format   = \"%s\"/>\n"
            "  <Interval = \"%ld\"/>\n" "</LogFormat>\n", m_name_str, m_format_str, m_interval_sec);
  } else {
    fprintf(fd, "INVALID FORMAT\n");
  }
}

/*-------------------------------------------------------------------------
  LogFormatList
  -------------------------------------------------------------------------*/

LogFormatList::LogFormatList()
{
}

LogFormatList::~LogFormatList()
{
  clear();
}

void
LogFormatList::clear()
{
  LogFormat *f;
  while ((f = m_format_list.dequeue())) {
    delete f;
  }
}

void
LogFormatList::add(LogFormat * format, bool copy)
{
  ink_assert(format != NULL);

  if (copy) {
    m_format_list.enqueue(NEW(new LogFormat(*format)));
  } else {
    m_format_list.enqueue(format);
  }
}

LogFormat *
LogFormatList::find_by_name(const char *name) const
{
  for (LogFormat * f = first(); f; f = next(f)) {
    if (!strcmp(f->name(), name)) {
      return f;
    }
  }
  return NULL;
}

LogFormat *
LogFormatList::find_by_type(LogFormatType type, int32 id) const
{
  for (LogFormat * f = first(); f; f = next(f)) {
    if ((f->type() == type) && (f->name_id() == id)) {
      return f;
    }
  }
  return NULL;
}

unsigned
LogFormatList::count()
{
  unsigned cnt = 0;
  for (LogFormat * f = first(); f; f = next(f)) {
    cnt++;
  }
  return cnt;
}

void
LogFormatList::display(FILE * fd)
{
  for (LogFormat * f = first(); f; f = next(f)) {
    f->display(fd);
  }
}
