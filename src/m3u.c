/** \file   m3u.c
 * \brief   Extended M3U playlist handling
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 *
 * Simple API for reading and writing extended M3U playlist files, tailored
 * towards use in VSID.
 *
 * There is no formal specification of the file format, so information was
 * used from various places on the internet, among which:
 * https://en.wikipedia.org/wiki/M3U
 *
 * Only a few directives of the extended m3u format are supported.
 */

#include "vice.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "lib.h"
#include "log.h"
#include "util.h"

#include "m3u.h"

/** \brief  Array length helper
 *
 * \param   arr array
 */
#define ARRAY_LEN(arr)  (sizeof arr / sizeof arr[0])

/** \brief  Initial size of buffer for reading the playlist */
#define INITIAL_BUFSIZE 80

/** \brief  Token indicating comment or directive */
#define M3U_COMMENT '#'

/** \brief  M3U directive data */
typedef struct m3u_ext_s {
    m3u_ext_id_t id;    /**< numeric ID */
    const char *text;   /**< text in upper case */
    const char *desc;   /**< short description */
} m3u_ext_t;


/** \brief  Path to playlist file */
static const char *playlist_path = NULL;

/** \brief  File pointer for the parser */
static FILE *playlist_fp = NULL;

/** \brief  Current line number in the playlist */
static long playlist_linenum = 0;

/** \brief  Buffer used for reading lines from the playlist
 *
 * Dynamically allocated and resized when required, freed with m3u_close()
 */
static char *playlist_buf = NULL;

/** \brief  Size of the buffer */
static size_t playlist_bufsize = 0;

/** \brief  Callback for normal entries
 *
 * The first argument is the entry as a string, the second the length of the
 * entry excluding the terminating nul character.
 */
static bool (*entry_cb)(const char *, size_t) = NULL;

/** \brief  Callback for directives
 *
 * The first argument the directive type, the second argument is the data
 * following the entry and the third argument is the length of the data
 * excluding the terminating nul character.
 */
static bool (*directive_cb)(int, const char *, size_t) = NULL;


static const m3u_ext_t extensions[] = {
    { M3U_EXTM3U,   "EXTM3U",   "file header" },
    { M3U_EXTINF,   "EXTINF",   "track information" },
    { M3U_PLAYLIST, "PLAYLIST", "playlist title" },
    { M3U_EXTALB,   "EXTALB",   "album title" },
    { M3U_EXTART,   "EXTART",   "album artist" },
};


/** \brief  Parse string for m3u directive
 *
 * Parse string \a s for a valid directive (excluding the starting '#') and
 * return the directive ID, optionally setting \a endptr to the first
 * non-directive character (whitspace, colon or eol).
 *
 * \param[in]   s       string to parse
 * \param[out]  endptr  pointer to character after the directive in \a s
 *                      (optional, pass `NULL` to ignore)
 *
 * \return  directive ID or #M3U_ERROR (-1) when no valid directive found
 */
static m3u_ext_id_t get_directive_id(const char *s, const char **endptr)
{
    const char *p = s;
    size_t i = 0;

    /* locate either eol, whitespace or colon */
    while (*p != '\0' && *p != ':' && !isspace((int)*p)) {
        p++;
    }

    /* check valid directives */
    for (i = 0; i < ARRAY_LEN(extensions); i++) {
        if (util_strncasecmp(extensions[i].text, s, strlen(extensions[i].text)) == 0) {
            if (*p == ':') {
                p++;    /* skip past colon */
            }
            if (endptr != NULL) {
                *endptr = p;
            }
            return extensions[i].id;
        }
    }
    if (endptr != NULL) {
        *endptr = NULL;
    }
    return M3U_ERROR;
}

/** \brief  Strip trailing whitespace from buffer
 *
 * Strip trailing whitespace starting at \a pos - 1 in the buffer.
 *
 * \return  position in the string of the terminating nul character
 */
static ssize_t strip_trailing(ssize_t pos)
{
    pos--;
    while (pos >= 0 && isspace((int)(playlist_buf[pos]))) {
        pos--;
    }
    playlist_buf[pos + 1] = '\0';
    return pos + 1;
}

/** \brief  Read a line of text from the playlist
 *
 * \param[out]  len length of line read (including trailing whitespace)
 *
 *
 * \return  `false` on EOF
 */
static bool read_line(size_t *len)
{
    ssize_t pos = 0;

    while (true) {
        int ch = fgetc(playlist_fp);

        if (pos == playlist_bufsize) {
            playlist_bufsize *= 2;
            playlist_buf = lib_realloc(playlist_buf, playlist_bufsize);
        }

        if (ch == EOF) {
            /* EOF, terminate string in buffer and exit */
            playlist_buf[pos] = '\0';
            *len = (size_t)strip_trailing(pos);
            return false;
        }

        if (ch == 0x0a) {
            /* LF */
            playlist_buf[pos] = '\0';
            if (pos > 0 && playlist_buf[pos - 1] == 0x0d) {
                /* strip CR */
                playlist_buf[pos - 1] = '\0';
                pos--;
            }

            /* strip trailing whitespace */
            *len = (size_t)strip_trailing(pos);
            return true;
        }

        playlist_buf[pos++] = ch;
    }
}


/** \brief  Open m3u file for parsing
 *
 * Try to open \a path for reading and set up the parser.
 *
 * The \a entry_callback argument is required and will be called by m3u_parse()
 * whenever a normal entry is encountered in \a path. Its first argument is
 * the entry text and the second argument is the lenght of the entry excluding
 * the terminating nul character.
 *
 * The \a directive_callback argument is optional and can be used to handle
 * extended M3U directives. The first argument is the directive ID (\see
 * #m3u_ext_id_t), the second argument is the text following the directive
 * and the third argument is the length of the text excluding the terminating
 * nul character.
 *
 * To stop the parser the callbacks must return `false`.
 *
 * \param[in]   path                path to m3u file
 * \param[in]   entry_callback      callback for normal entries
 * \param[in]   directive_callback  callback for extended m3u directives
 *                                  (optional, pass `NULL` to ignore)
 *
 * \return  `true` on success
 */
bool m3u_open(const char *path,
              bool (*entry_callback)(const char *arg, size_t len),
              bool (*directive_callback)(m3u_ext_id_t id, const char *arg, size_t len))
{
    if (path == NULL || *path == '\0') {
        log_error(LOG_ERR, "m3u: path argument cannot be NULL or empty.");
        return false;
    }
    if (entry_callback == NULL) {
        log_error(LOG_ERR, "m3u: entry_callback entry cannot be NULL.");
        return false;
    }
    playlist_path = path;
    entry_cb = entry_callback;
    directive_cb = directive_callback;
    playlist_linenum = 1;

    /* try to open file for reading */
    playlist_fp = fopen(path, "rb");
    if (playlist_fp == NULL) {
        log_error(LOG_ERR,
                  "m3u: failed to open %s for reading: errno %d (%s)",
                  path, errno, strerror(errno));
        return false;
    }

    /* allocate buffer for reading file content */
    playlist_buf = lib_malloc(INITIAL_BUFSIZE);
    playlist_bufsize = INITIAL_BUFSIZE;
    return true;
}


/** \brief  Close m3u file and clean up resources
 *
 * Cleans up resources and resets internal state for subsequent parsing of
 * m3u files.
 */
void m3u_close(void)
{
    if (playlist_fp != NULL) {
        fclose(playlist_fp);
        playlist_fp = NULL;
    }
    if (playlist_buf != NULL) {
        lib_free(playlist_buf);
        playlist_buf = NULL;
    }
    playlist_path = NULL;
    playlist_linenum = 0;
}


/** \brief  Parse the playlist
 *
 * Parse playlist file triggering the callbacks registered with m3u_open().
 *
 * \return  `true` on success
 */
bool m3u_parse(void)
{
    size_t len;

    if (playlist_fp == NULL) {
        log_error(LOG_ERR, "m3u: no input file.");
        return false;
    }

    while (read_line(&len)) {
        const char *s;
#if 0
        printf("line[%3ld](%3zu) '%s'\n", playlist_linenum, len, playlist_buf);
#endif
        s = util_skip_whitespace(playlist_buf);
        if (*s == M3U_COMMENT) {
            const char *endptr;
            m3u_ext_id_t id;

            /* try to find valid directive */
            id = get_directive_id(s + 1, &endptr);
            if (id >= 0) {
                if (!directive_cb(id, endptr, 0)) {
                    /* error */
                    return false;
                }
            }
        } else if (*s != '\0') {
            if (!entry_cb(s, len - (s - playlist_buf))) {
                /* some error, stop */
                return false;
            }
        }

        playlist_linenum++;
    }
    return true;
}


/** \brief  Get current line number of playlist being processed
 *
 * \return  line number
 */
long m3u_linenum(void)
{
    return playlist_linenum;
}


/** \brief  Get directive string for ID
 *
 * \param[in]   id  directive ID
 *
 * \see #m3u_ext_id_t
 */
const char *m3u_directive_str(m3u_ext_id_t id)
{
    if (id < 0 || id >= (m3u_ext_id_t)ARRAY_LEN(extensions)) {
        return NULL;
    }
    return extensions[id].text;
}
