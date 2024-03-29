/** \file   src/lib/stil.c
 * \brief   Sid Tune Information List handling
 *
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

/*
 *  HVSClib - a library to work with High Voltage SID Collection files
 *  Copyright (C) 2018-2022  Bas Wassink <b.wassink@ziggo.nl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.*
 */

#undef HVSC_DEBUG

#ifndef HVSC_STANDALONE
# include "vice.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>

#ifndef HVSC_STANDALONE
# include "log.h"
#endif
#include "hvsc.h"
#include "hvsc_defs.h"
#include "base.h"

#include "stil.h"



/*
 * Forward declarations
 */

static void                 stil_field_init(hvsc_stil_field_t *field);
static hvsc_stil_field_t *  stil_field_new(int type,
                                           const char *text,
                                           size_t      tlen,
                                           long        ts_from,
                                           long        ts_to,
                                           const char *album,
                                           size_t      alen);
static void                 stil_field_free(hvsc_stil_field_t *field);
static hvsc_stil_field_t *  stil_field_dup(const hvsc_stil_field_t *field);

static void                 stil_block_init(hvsc_stil_block_t *block);
static hvsc_stil_block_t *  stil_block_new(void);
static void                 stil_block_free(hvsc_stil_block_t *block);
static void                 stil_block_add_field(hvsc_stil_block_t *block,
                                                 hvsc_stil_field_t *field);
static hvsc_stil_block_t *  stil_block_dup(const hvsc_stil_block_t *block);

static int                  stil_parse_timestamp(char                   *s,
                                                 hvsc_stil_timestamp_t  *ts,
                                                 char                  **endptr);


/** \brief  Parse tune number from string \a s
 *
 * The string \a s is expected to be in the form "(#N)" where N is a decimal
 * string.
 *
 * \param[in]   s   string
 *
 * \return  tune number or -1 when not found
 */
static int stil_parse_tune_number(const char *s)
{
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == '(' && *(s + 1) == '#') {
        char *endptr;
        long  result;

        result = strtol(s + 2, &endptr, 10);
        if (*endptr == ')') {
            return (int)result;
        }
    }
    return -1;
}

/** \brief  Parse a STIL timestamp
 *
 * A STIL timestamp is either '([H]H:MM[.f]{1,3)' or
 * '([H]H:MM[.f]{1,3}-[H]H:MM[.f]{1,3})'.
 *
 * In the first case, the 'to' member of \a ts is set to -1 to indicate only a
 * single timestamp was found, not a range.
 *
 * \param[in]   s       string to parse
 * \param[out]  ts      timestamp object to store result
 * \param[out]  endptr  object to store pointer to first non-parsed character
 *
 * \return  bool
 */
static int stil_parse_timestamp(char                   *s,
                                hvsc_stil_timestamp_t  *ts,
                                char                  **endptr)
{
    char *p;
    long  result;

    /* get first/only entry */
    result = hvsc_parse_simple_timestamp(s, &p);
    if (result < 0) {
        *endptr = p;
        return 0;
    }
    ts->from = result;

    /* do we have a range? */
    if (*p != '-') {
        /* nope, single entry */
        ts->to  = -1;
        *endptr = p;
        return 1;
    }

    /* get second entry */
    result = hvsc_parse_simple_timestamp(p + 1, endptr);
    if (result < 0) {
        return 0;
    }
    ts->to = result;
    return 1;
}

/** \brief  Parse Album sub field
 *
 * \param[in]   s   string to parse for Album sub field
 * \param[in]   len length of \a s
 *
 * \return  heap-allocated Album string or `NULL` on failure
 */
static char *stil_parse_album(const char *s, size_t len)
{
    /* put p at the char before ']' */
    const char *p   = s + len - 2;
    const char *end = s + len - 2;
    char       *album = NULL;

    while (p >= s && *p != '[') {
        p--;
    }
    if (*p == '[') {
        /* found it */
        album = hvsc_strndup(p + 1, (size_t)(end - p));
    }
    return album;
}


/*
 * STIL field functions
 */

/** \brief  Initialize \a field for use
 *
 * \param[in,out]   field   STIL field object
 */
static void stil_field_init(hvsc_stil_field_t *field)
{
    field->type           = HVSC_FIELD_INVALID;
    field->text           = NULL;
    field->timestamp.from = -1;
    field->timestamp.to   = -1;
    field->album          = NULL;
}

/** \brief  Allocate a new STIL field object
 *
 * The copy of \a text will be nul-terminated.
 *
 * \param[in]   type    field type
 * \param[in]   text    field text
 * \param[in]   tlen    number of bytes to copy from \a text
 * \param[in]   ts_from timestamp 'from' member
 * \param[in]   ts_to   timestamp 'to' member
 * \param[in]   album   cover info
 * \param[in]   alen    number of bytes to copy from \a album
 *
 * \return  new STIL field object or `NULL` on failure
 */
static hvsc_stil_field_t *stil_field_new(int         type,
                                         const char *text,
                                         size_t      tlen,
                                         long        ts_from,
                                         long        ts_to,
                                         const char *album,
                                         size_t      alen)
{
    hvsc_stil_field_t *field = hvsc_malloc(sizeof *field);

    stil_field_init(field);
    field->type           = type;
    field->timestamp.from = ts_from;
    field->timestamp.to   = ts_to;
    field->text = hvsc_strndup(text, tlen);
    if (album != NULL && *album != '\0') {
        field->album = hvsc_strndup(album, alen);
    }
    return field;
}

/** \brief  Free memory used by member of \a field and \a field itself
 *
 * \param[in,out]   field   STIL field object
 */
static void stil_field_free(hvsc_stil_field_t *field)
{
    if (field->text != NULL) {
        hvsc_free(field->text);
    }
    if (field->album != NULL) {
        hvsc_free(field->album);
    }
    hvsc_free(field);
}


/*
 * STIL block functions
 */

/** \brief  Initialize STIL block
 *
 * Initializes all members to 0/NULL.
 *
 * \param[in,out]   block   STIL block
 */
static void stil_block_init(hvsc_stil_block_t *block)
{
    block->tune        = 0;
    block->fields      = NULL;
    block->fields_max  = 0;
    block->fields_used = 0;
}

/** \brief  Allocate and intialize a new STIL block
 *
 * \return  new STIL block or `NULL` on failure
 */
static hvsc_stil_block_t *stil_block_new(void)
{
    hvsc_stil_block_t *block;
    size_t             i;

    block = hvsc_malloc(sizeof *block);
    stil_block_init(block);

    block->fields = hvsc_malloc(HVSC_STIL_BLOCK_FIELDS_INIT * sizeof *(block->fields));
    block->fields_max = HVSC_STIL_BLOCK_FIELDS_INIT;
    for (i = 0; i < HVSC_STIL_BLOCK_FIELDS_INIT; i++) {
        block->fields[i] = NULL;
    }
    return block;
}

/** \brief  Make a deep copy of \a field
 *
 * \param[in]   field   STIL field
 *
 * \return  copy of \a field
 */
static hvsc_stil_field_t *stil_field_dup(const hvsc_stil_field_t *field)
{
    return stil_field_new(field->type,
                          field->text,
                          strlen(field->text),
                          field->timestamp.from,
                          field->timestamp.to,
                          field->album,
                          field->album != NULL ? strlen(field->album) : 0);
}

/** \brief  Make a deep copy of \a block
 *
 * \param[in]   block   STIL block
 *
 * \return  copy of \a block
 */
static hvsc_stil_block_t *stil_block_dup(const hvsc_stil_block_t *block)
{
    hvsc_stil_block_t *copy;
    size_t             i;

    copy = hvsc_malloc(sizeof *copy);
    stil_block_init(copy);

    copy->tune        = block->tune;
    copy->fields_max  = block->fields_max;
    copy->fields_used = block->fields_used;
    copy->fields      = hvsc_malloc(block->fields_max * sizeof *(copy->fields));
    for (i = 0; i < copy->fields_used; i++) {
        copy->fields[i] = stil_field_dup(block->fields[i]);
    }
    return copy;
}

/** \brief  Free STIL block and its members
 *
 * \param[in,out]   block   STIL block
 */
static void stil_block_free(hvsc_stil_block_t *block)
{
    size_t i;

    for (i = 0; i < block->fields_used; i++) {
        stil_field_free(block->fields[i]);
    }
    hvsc_free(block->fields);
    hvsc_free(block);
}

/** \brief  Add STIL \a field to STIL \a block
 *
 * \param[in,out]   block   STIL block
 * \param[in]       field   STIL field
 */
static void stil_block_add_field(hvsc_stil_block_t *block,
                                 hvsc_stil_field_t *field)
{
    hvsc_dbg("max = %" PRI_SIZE_T ", used = %" PRI_SIZE_T "\n",
             block->fields_max, block->fields_used);
    /* do we need to resize the array? */
    if (block->fields_max == block->fields_used) {
        /* yep */
        block->fields_max *= 2;
        block->fields = hvsc_realloc(block->fields,
                                     block->fields_max * sizeof *(block->fields));
    }
    block->fields[block->fields_used++] = field;
}

/** \brief  Initialize STIL \a handle for use
 *
 * \param[in,out]   handle  STIL handle
 */
static void stil_init_handle(hvsc_stil_t *handle)
{
    hvsc_text_file_init_handle(&(handle->stil));
    handle->psid_path     = NULL;
    handle->entry_buffer  = NULL;
    handle->entry_bufmax  = 0;
    handle->entry_bufused = 0;
    handle->sid_comment   = NULL;
    handle->blocks        = NULL;
    handle->blocks_max    = 0;
    handle->blocks_used   = 0;
}

/** \brief  Allocate initial 'blocks' array
 *
 * All block pointers are initialized to `NULL`
 *
 * \param[in,out]   handle  STIL handle
 */
static void stil_handle_init_blocks(hvsc_stil_t *handle)
{
    size_t i;

    handle->blocks = hvsc_malloc(HVSC_HANDLE_BLOCKS_INIT * sizeof *(handle->blocks));
    for (i = 0; i < HVSC_HANDLE_BLOCKS_INIT; i++) {
        handle->blocks[i] = NULL;
    }
    handle->blocks_used = 0;
    handle->blocks_max  = HVSC_HANDLE_BLOCKS_INIT;
}

/** \brief  Free STIL blocks (entries + array)
 *
 * \param[in,out]   handle  STIL handle
 */
static void stil_handle_free_blocks(hvsc_stil_t *handle)
{
    if (handle->blocks != NULL) {
        size_t i;

        for (i = 0; i < handle->blocks_used; i++) {
            stil_block_free(handle->blocks[i]);
        }
        hvsc_free(handle->blocks);
        handle->blocks = NULL;
    }
}

/** \brief  Add STIL \a block to STIL \a handle
 *
 * \param[in,out]   handle  STIL handle
 * \param[in]       block   STIL block
 */
static void stil_handle_add_block(hvsc_stil_t *handle, hvsc_stil_block_t *block)
{
    /* do we need to resize the array? */
    if (handle->blocks_max == handle->blocks_used) {
        /* yep */
        handle->blocks_max *= 2;
        handle->blocks = hvsc_realloc(handle->blocks,
                                      handle->blocks_max * sizeof *(handle->blocks));
    }
    /* make a copy */
    handle->blocks[handle->blocks_used++] = stil_block_dup(block);
}


/** \brief  Open STIL and look for PSID file \a psid
 *
 * \param[in]   psid    path to PSID file
 * \param[out]  handle  STIL handle
 *
 * \return  bool
 */
bool hvsc_stil_open(const char *psid, hvsc_stil_t *handle)
{
    const char *line;

    stil_init_handle(handle);
    handle->entry_buffer = hvsc_malloc(HVSC_STIL_BUFFER_INIT *
                                       sizeof *(handle->entry_buffer));
    handle->entry_bufmax  = HVSC_STIL_BUFFER_INIT;
    handle->entry_bufused = 0;

#ifndef HVSC_STANDALONE
    log_message(LOG_DEFAULT, "VSID: Opening '%s'.", hvsc_stil_path);
#endif
    if (!hvsc_text_file_open(hvsc_stil_path, &(handle->stil))) {
#ifndef HVSC_STANDALONE
        log_warning(LOG_DEFAULT, "VSID: Failed to open STIL.");
#endif
        hvsc_stil_close(handle);
        return false;
    }

    /* get MD5 sum for the given file */
    memset(handle->md5sum, 0, sizeof handle->md5sum);
    if (hvsc_md5_digest(psid, handle->md5sum)) {
        hvsc_dbg("got MD5 sum: %s\n", handle->md5sum);
    } else {
        hvsc_dbg("failed to get MD5 sum for file '%s'\n", psid);
    }

    /* make copy of psid path, ripping off the HVSC root directory */
    if (hvsc_path_is_hvsc(psid)) {
        hvsc_dbg("path %s is in HVSC base directory\n", psid);
        handle->psid_path = hvsc_path_strip_root(psid);
    } else {
        /* not a HVSC base path, look up HVSC base-relative path in SLDB */
        char *path;

        hvsc_dbg("path %s is NOT in HVSC base directory or base directory not set\n",
                 psid);
        path = hvsc_sldb_get_path_for_md5(handle->md5sum);
        if (path != NULL) {
            hvsc_dbg("path for md5 hash %s is %s\n", handle->md5sum, path);
            handle->psid_path = path;
        } else {
            hvsc_dbg("couldn't find path for md5 hash %s\n", handle->md5sum);
            handle->psid_path = hvsc_strdup(psid);
        }
    }
#ifdef WINDOWS_COMPILE
    /* fix directory separators */
    hvsc_path_fix_separators(handle->psid_path);
#endif
    hvsc_dbg("stripped path is '%s'\n", handle->psid_path);

    /* find the entry */
    while (true) {
        line = hvsc_text_file_read(&(handle->stil));
        if (line == NULL) {
            if (feof(handle->stil.fp)) {
                /* EOF, so simply not found */
                hvsc_errno = HVSC_ERR_NOT_FOUND;
#ifndef HVSC_STANDALONE
                log_message(LOG_DEFAULT, "VSID: No STIL entry found.");
#endif
            }
            hvsc_stil_close(handle);
            /* I/O error is already set */
            return false;
        }

        if (strcmp(line, handle->psid_path) == 0) {
#ifndef HVSC_STANDALONE
            log_message(LOG_DEFAULT,
                    "VSID: Found '%s' at line %ld.", line, handle->stil.lineno);
#endif
            return true;
        }
    }
}


bool hvsc_stil_open_md5(const char *digest, hvsc_stil_t *handle)
{
    char *path;

    stil_init_handle(handle);
    handle->entry_bufmax  = HVSC_STIL_BUFFER_INIT;
    handle->entry_bufused = 0;
    handle->entry_buffer  = hvsc_malloc(handle->entry_bufmax *
                                        sizeof *(handle->entry_buffer));
    memcpy(handle->md5sum, digest, sizeof handle->md5sum);

    if (!hvsc_text_file_open(hvsc_stil_path, &(handle->stil))) {
        hvsc_dbg("failed to open STIL.");
        return false;
    }

    /* we need to look up the HVSC-relative path for the md5 digest in the SLDB
     * in order to find the STIL entry by filename (yeah...) */
    hvsc_dbg("looking up filename in SLDB for digest %s\n", digest);
    path = hvsc_sldb_get_path_for_md5(handle->md5sum);
    if (path != NULL) {
        hvsc_dbg("OK: found %s.", path);
        handle->psid_path = path;
    } else {
        hvsc_dbg("FAIL.\n");
        hvsc_stil_close(handle);
        return false;
    }

    /* look up entry */
    while (true) {
        const char *line = hvsc_text_file_read(&(handle->stil));

        if (line == NULL) {
            if (feof(handle->stil.fp)) {
                /* EOF, OK */
                hvsc_errno = HVSC_ERR_NOT_FOUND;
#ifndef HVSC_STANDALONE
                log_message(LOG_DEFAULT, "VSID: No STIL entry found.");
#endif
            }
            hvsc_stil_close(handle);
            return false;
        }

        if (strcmp(line, handle->psid_path) == 0) {
#ifndef HVSC_STANDALONE
            log_message(LOG_DEFAULT,
                    "VSID: Found '%s' at line %ld.", line, handle->stil.lineno);
#endif
            return true;
        }
    }
}


/** \brief  Clean up memory and close file handle(s) used by \a handle
 *
 * Doesn't free \a handle itself.
 *
 * \param[in,out]   handle  STIL handle
 */
void hvsc_stil_close(hvsc_stil_t *handle)
{
    hvsc_text_file_close(&(handle->stil));
    hvsc_free(handle->psid_path);

    if (handle->entry_buffer != NULL) {
        size_t i;
        for (i = 0; i < handle->entry_bufused; i++){
            hvsc_free(handle->entry_buffer[i]);
        }
        hvsc_free(handle->entry_buffer);
    }

    if (handle->sid_comment != NULL) {
        hvsc_free(handle->sid_comment);
    }
    if (handle->blocks != NULL) {
        stil_handle_free_blocks(handle);
    }
}


/** \brief  Add a \a line of STIL entry text to \a handle
 *
 * Add a line from the STIL to \a handle, for proper parsing later.
 *
 * \param[in,out]   handle  STIL handle
 * \param[in]       line    line of text
 *
 * \return  bool
 */
static void hvsc_stil_entry_add_line(hvsc_stil_t *handle, const char *line)
{
    if (handle->entry_bufmax == handle->entry_bufused) {
        handle->entry_bufmax *= 2;
        hvsc_dbg("resizing line buffer to %" PRI_SIZE_T " entries\n",
                  handle->entry_bufmax);
        handle->entry_buffer = hvsc_realloc(handle->entry_buffer,
                handle->entry_bufmax * sizeof *(handle->entry_buffer));
    }
    handle->entry_buffer[handle->entry_bufused++] = hvsc_strdup(line);
}


/** \brief  Read current STIL entry
 *
 * Reads all text lines in of the current STIL entry.
 *
 * \param[in,out]   handle  STIL handle
 *
 * \return  bool
 */
bool hvsc_stil_read_entry(hvsc_stil_t *handle)
{
    const char *line;

    while (true) {
        line = hvsc_text_file_read(&(handle->stil));
        if (line == NULL) {
            /* EOF ? */
            if (feof(handle->stil.fp)) {
                /* EOF, so end of entry */
                return true;
            }
            /* I/O error is already set */
            return false;
        }

        /* check for end of entry */
        if (hvsc_string_is_empty(line)) {
            hvsc_dbg("got empty line -> end-of-entry\n");
            return true;
        }

        hvsc_dbg("line %ld: '%s'\n", handle->stil.lineno, line);
        hvsc_stil_entry_add_line(handle, line);
    }
}


/** \brief  Helper function: dump the lines of the current STIL entry on stdout
 *
 * \param[in]   handle  STIL handle
 */
void hvsc_stil_dump_entry(hvsc_stil_t *handle)
{
    size_t i;

    for (i = 0; i < handle->entry_bufused; i++) {
        printf("%s\n", handle->entry_buffer[i]);
    }
}



/*
 * Functions to parse the STIL entry text into a structured representation
 */

/** \brief  Parse the STIL entry text for a comment
 *
 * Parses a comment from the lines of text in the parser's stil entry. The
 * comment is expected to start with 'COMMENT:' on the first line and each
 * subsequent line is expected to start with 9 spaces, per STIL.faq.
 *
 * \param[in]   state   parser state
 *
 * \return  comment
 */
static char *stil_parse_comment(hvsc_stil_parser_state_t *state)
{
    char       *comment;
    size_t      len;    /* len per line, excluding '\0' */
    size_t      total;  /* total line of comment, excluding '\0' */
    const char *line = state->handle->entry_buffer[state->lineno];

    /* first line is 'COMMENT: <text>' */
    comment = hvsc_strdup(line + 9);
    total   = strlen(line) - 9;
    state->lineno++;

    while (state->lineno < state->handle->entry_bufused) {
        line = state->handle->entry_buffer[state->lineno];
        len = strlen(line);
        /* check for nine spaces */
        if (strncmp("         ", line, 9) != 0) {
            return comment;
        }
        /* realloc to add new line */
        comment = hvsc_realloc(comment, total + len - 8 + 1);
        /* add line to comment, adding a space from the nine spaces indent to
         * get a proper separating space in the final comment text */
        memcpy(comment + total, line + 8, len - 8 + 1);
        total += (len - 8);
        state->lineno++;
    }
    return comment;
}

/** \brief  Initialize parser
 *
 * Initializes parser state, stores a pointer to handle in the object to easier
 * pass around data.
 *
 * \param[in,out]   parser  STIL parser state
 * \param[in]       handle  STIL handle
 */
static void stil_parser_init(hvsc_stil_parser_state_t *parser,
                             hvsc_stil_t              *handle)
{
    parser->handle    = handle;
    parser->tune      = 0;
    parser->lineno    = 0;
    parser->field     = NULL;
    parser->ts.from   = -1;
    parser->ts.to     = -1;
    parser->linelen   = 0;
    parser->album     = NULL;
    parser->album_len = 0;

    /* add block for tune #1 */
    parser->block = stil_block_new();
}

/** \brief  Free memory used by the parser's members
 *
 * Frees memory used by the members of \a parser, but not parser itself. The
 * STIL handle stored in \a parser also isn't freed, that is done by
 * hvsc_stil_close()
 *
 * \param[in,out]   parser  STIL parser state
 */
static void stil_parser_free(hvsc_stil_parser_state_t *parser)
{
    if (parser->block != NULL) {
        stil_block_free(parser->block);
    }
    if (parser->album != NULL) {
        hvsc_free(parser->album);
        parser->album = NULL;
    }
}


/** \brief  Parse textual content of \a handle into a structured representation
 *
 * \param[in,out]   handle  STIL entry handle
 *
 * \todo    Refactor, this function is too long and complex
 */
void hvsc_stil_parse_entry(hvsc_stil_t *handle)
{
    hvsc_stil_parser_state_t state;

    /* init parser state */
    stil_parser_init(&state, handle);

    /* allocate array for STIL blocks */
    stil_handle_init_blocks(handle);

    while (state.lineno < state.handle->entry_bufused) {
        char *comment;
        int   type;
        int   num;
        char *t;
        char *line = handle->entry_buffer[state.lineno];

        state.ts.from = -1;
        state.ts.to   = -1;
        state.album   = NULL;

        /* to avoid unitialized warning later on (it isn't uinitialized) */
        comment = NULL;

        hvsc_dbg("parsing:\n%s\n", line);
        state.linelen = strlen(line);

        /* tune number? */
        num = stil_parse_tune_number(line);
        if (num > 0) {
            hvsc_dbg("Got tune mumber %d\n", num);
            state.tune = num;

            /*
             * store block and alloc new one (if tune > 1, otherwise we already
             * have a block)
             */
            if (state.tune > 1) {
                stil_handle_add_block(state.handle, state.block);
                stil_block_free(state.block);
                state.block       = stil_block_new();
                state.block->tune = num;
            }

        } else {
            /* must be a field */
            type = hvsc_get_field_type(line);
            hvsc_dbg("Got field type %d\n", type);
#if 0
            line += 9;
            state.linelen -= 9;
#endif

            switch (type) {
                /* COMMENT: field */
                case HVSC_FIELD_COMMENT:
                    comment = stil_parse_comment(&state);
                    if (state.tune == 0) {
                        /* SID-wide comment */
                        state.handle->sid_comment = comment;
                    } else {
                        /* normal per-tune comment */
                        line          = comment;
                        state.linelen = strlen(comment);
                    }
                    /* comment parsing 'ate' the first non-comment line, so
                     * adjust parser state */
                    state.lineno--;
                    break;

                /* TITLE: field */
                case HVSC_FIELD_TITLE:
                    line          += 9;
                    state.linelen -= 9;
                    /* check for timestamp */
                    /* find closing ')' at end of line */
                    if (state.linelen > 6 && line[state.linelen - 1] == ')') {
                        hvsc_dbg("possible TIMESTAMP\n");

                        /* find opening '(' */
                        t = line + state.linelen - 1;
                        while (t >= line && *t != '(') {
                            t--;
                        }
                        if (t == line) {
                            /* nope */
                            hvsc_dbg("no closing '(') found, ignoring\n");
                        } else {
                            char *endptr;

                            if (!stil_parse_timestamp(t + 1, &(state.ts), &endptr)) {
                                /*
                                 * Some lines contain strings like "(lyrics)"
                                 * or "(music)", so don't trigger a parser
                                 * error, just ignore
                                 */
                                hvsc_dbg("invalid TIMESTAMP, ignoring\n");
                            } else {
                                hvsc_dbg("got TIMESTAMP: %ld-%ld\n",
                                        state.ts.from, state.ts.to);
                                /*
                                 * Adjust line: strip timestamp text, this
                                 * assumes a single space between the timestamp
                                 * '(' starting char and the rest of the text.
                                 * So perhaps do a rtrim() on the line?
                                 */
                                state.linelen = (size_t)(t - line) - 1;
                            }
                        }
                    }

                    /* check for 'Album' field: [from ...] */
                    if (line[state.linelen - 1] == ']') {
                        char *album;

                        hvsc_dbg("found possible album\n");
                        album = stil_parse_album(line, state.linelen);
                        if (album != NULL) {
                            hvsc_dbg("got album: '%s'\n", album);
                            state.album     = album;
                            state.album_len = strlen(album);

                            /* adjust line len (+3 for '[', ']' and space */
                            state.linelen -= (state.album_len + 3);
                        }
                    }
                    break;

                /* Other fields without special meaning/sub fields */
                default:
                    /* don't copy the first nine chars (field ident + space) */
                    line          += 9;
                    state.linelen -= 9;
                    break;
            }

            /*
             * Add line to block
             */

            /* fix the tune number */
            if (state.tune == 0 && type != HVSC_FIELD_COMMENT) {
                state.tune        = 1;
                state.block->tune = 1;
            }

            if (state.tune > 0) {
                hvsc_dbg("Adding '%s'\n", line);
                state.field = stil_field_new(type,
                                             line, state.linelen,
                                             state.ts.from, state.ts.to,
                                             state.album, state.album_len);
                stil_block_add_field(state.block, state.field);

                /* if the line was a comment, free the comment */
                if (type == HVSC_FIELD_COMMENT) {
                    hvsc_free(comment);
                    comment = NULL;
                }
                /* free album, if present */
                if (state.album != NULL) {
                    hvsc_free(state.album);
                    state.album = NULL;
                }
            } else {
                /* got all the SID-wide stuff, now add the rest to per-tune
                 * STIL blocks */
                state.tune        = 1;
                state.block->tune = 1;
            }
        }
        state.lineno++;
    }

    /* add last block */
    stil_handle_add_block(state.handle, state.block);

    stil_parser_free(&state);
}


/** \brief  Temp: dump parsed entries
 *
 * \param[in]   handle  STIL handle
 */
void hvsc_stil_dump(hvsc_stil_t *handle)
{
    size_t t;   /* tune index, not its number */
    size_t f;   /* field index, not its type */

    printf("\n\n{File: %s}\n", handle->psid_path);
    if (handle->sid_comment != NULL) {
        printf("\n{SID-wide comment}\n%s\n", handle->sid_comment);
    }

    printf("\n{Per-tune info}\n\n");
    for (t = 0; t < handle->blocks_used; t++) {
        hvsc_stil_block_t *block = handle->blocks[t];
        printf("  {#%d}\n", block->tune);
        for (f = 0; f < block->fields_used; f++) {
            printf("    %s %s\n",
                    hvsc_get_field_display(block->fields[f]->type),
                    block->fields[f]->text);
            /* do we have a valid timestamp ? */
            if (block->fields[f]->timestamp.from >= 0) {
                long from = block->fields[f]->timestamp.from;
                long to   = block->fields[f]->timestamp.to;

                if (to < 0) {
                    printf("      {timestamp} %ld:%02ld\n",
                           from / 60, from % 60);
                } else {
                    printf("      {timestamp} %ld:%02ld-%ld:%02ld\n",
                           from / 60, from % 60, to / 60, to % 60);
                }
            }
            /* do we have an album? */
            if (block->fields[f]->album != NULL) {
                printf("           {album} %s\n", block->fields[f]->album);
            }
        }
        putchar('\n');
    }
}


/** \brief  Get a STIL entry for \a tune
 *
 * Gets a STIL entry for \a tune from \a handle and store it in \a entry.
 * Please note that both hvsc_read_entry() and hvsc_parse_entry() need to have
 * been successfully called in order for this function to succeed.
 * The data in \a entry should not be manipulated or freed.
 *
 * \param[in]   handle  STIL handle
 * \param[out]  entry   STIL tune entry
 * \param[in]   tune    tune number (1-256)
 *
 * \return  bool
 */
bool hvsc_stil_get_tune_entry(const hvsc_stil_t     *handle,
                             hvsc_stil_tune_entry_t *entry,
                             int                     tune)
{
    size_t n = 0;

    if (handle->blocks == NULL) {
        hvsc_errno = HVSC_ERR_INVALID;
        return false;
    }

    for (n = 0; n < handle->blocks_used; n++) {
        if (handle->blocks[n]->tune == tune) {
            const hvsc_stil_block_t *block = handle->blocks[n];

            hvsc_dbg("Got entry for tune #%d\n", tune);
            entry->tune        = block->tune;
            entry->fields      = block->fields;
            entry->field_count = block->fields_used;
            return true;
        }
    }
    hvsc_dbg("Could not find entry for tune #%d\n", tune);
    hvsc_errno = HVSC_ERR_NOT_FOUND;
    return false;
}


/** \brief  Dump STIL tune \a entry on stdout
 *
 * This functions requires hvsc_stil_get_tune_entry() to be called to populate
 * the \a entry beforehand.
 *
 * \param[in]   entry   STIL tune entry
 */
void hvsc_stil_dump_tune_entry(const hvsc_stil_tune_entry_t *entry)
{
    size_t f;

    for (f = 0; f < entry->field_count; f++) {
        const hvsc_stil_field_t *field = entry->fields[f];

        /* print field identifier and content */
        printf("%s %s\n",
                hvsc_get_field_display(field->type),
                field->text);

        /* check for timestamp sub field */
        if (field->timestamp.from >= 0) {
            hvsc_stil_timestamp_t ts = field->timestamp;
            if (ts.to < 0) {
                printf("  {timestamp} %ld:%02ld\n",
                       ts.from / 60, ts.from % 60);
            } else {
                printf("  {timestamp} %ld:%02ld-%ld:%02ld\n",
                       ts.from / 60, ts.from % 60, ts.to / 60, ts.to % 60);
            }
        }

        /* check for album sub field */
        if (field->album != NULL) {
            printf("       {album} %s\n", field->album);
        }
    }
}


/** \brief  Retrieve full STIL info on \a path
 *
 * Get full STIL info PSID file \a path.
 *
 * \param[out]  stil    STIL handle
 * \param[in]   path    absolute path to PSID file
 *
 * \return  true if STIL info found and parsed
 */
bool hvsc_stil_get(hvsc_stil_t *stil, const char *path)
{
    /* find STIL.txt entry */
    if (!hvsc_stil_open(path, stil)) {
        return false;
    }

    /* read all text lines related to the entry */
    if (!hvsc_stil_read_entry(stil)) {
        hvsc_stil_close(stil);
        return false;
    }

    /* parse text */
    hvsc_stil_parse_entry(stil);
    return true;
}


bool hvsc_stil_get_md5(hvsc_stil_t *stil, const char *digest)
{
    if (!hvsc_stil_open_md5(digest, stil)) {
        return false;
    }

    if (!hvsc_stil_read_entry(stil)) {
        hvsc_stil_close(stil);
        return false;
    }

    hvsc_stil_parse_entry(stil);
    return true;
}
