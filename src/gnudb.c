/*
 * This file is part of cyanrip.
 *
 * cyanrip is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * cyanrip is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with cyanrip; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "gnudb.h"
#include "cyanrip_log.h"

#include <curl/curl.h>
#include <libavutil/bprint.h>

/* GNUdb requires every application name to be pre-registered with them
 * (see gnudb.org/howtognudb.php) before any request works - XLD's "XLD"
 * identity is already approved, ours isn't yet. */
#define GNUDB_APPNAME "cyanrip"

static size_t receive_text(void *buffer, size_t size, size_t nb, void *opaque)
{
    av_bprint_append_data(opaque, buffer, size * nb);
    return size * nb;
}

/* Sends a single CDDB protocol command to ctx->settings.gnudb_host and
 * returns the raw response text (caller frees with av_free()), or NULL on
 * a network/HTTP error. */
static char *gnudb_request(cyanrip_ctx *ctx, const char *cmd)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return NULL;

    char hello[256];
    snprintf(hello, sizeof(hello), "%s localhost %s %s",
             ctx->settings.gnudb_email, GNUDB_APPNAME, PROJECT_VERSION_STRING);

    char *cmd_esc = curl_easy_escape(curl, cmd, 0);
    char *hello_esc = curl_easy_escape(curl, hello, 0);

    /* Plain HTTP: personalized (per-registration) gnudb hosts have been
     * observed sitting behind a shared frontend whose TLS certificate
     * doesn't cover their hostname; HTTP works uniformly for both that and
     * the default shared host. */
    AVBPrint url;
    av_bprint_init(&url, 0, AV_BPRINT_SIZE_AUTOMATIC);
    av_bprintf(&url, "http://%s/~cddb/cddb.cgi?cmd=%s&hello=%s&proto=6",
               ctx->settings.gnudb_host, cmd_esc, hello_esc);

    curl_free(cmd_esc);
    curl_free(hello_esc);

    AVBPrint resp;
    av_bprint_init(&resp, 0, AV_BPRINT_SIZE_AUTOMATIC);

    char user_agent[256];
    snprintf(user_agent, sizeof(user_agent), "cyanrip/%s ( https://github.com/cyanreg/cyanrip )",
             PROJECT_VERSION_STRING);

    char errbuf[CURL_ERROR_SIZE] = { 0 };
    curl_easy_setopt(curl, CURLOPT_URL, url.str);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive_text);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    av_bprint_finalize(&url, NULL);

    if (res != CURLE_OK) {
        cyanrip_log(ctx, 0, "GNUdb request failed: %s\n",
                    errbuf[0] ? errbuf : curl_easy_strerror(res));
        av_bprint_finalize(&resp, NULL);
        return NULL;
    }

    char *str;
    av_bprint_finalize(&resp, &str);
    return str;
}

/* Sends "cddb query ...", and on a match, writes its category and disc ID
 * (needed for a follow-up "cddb read") into the given buffers. Returns 0 if
 * there's no usable match. */
static int gnudb_query(cyanrip_ctx *ctx, const char *discid, int nb_tracks,
                        const uint32_t *offsets, uint32_t total_secs,
                        char *categ_out, size_t categ_size,
                        char *discid_out, size_t discid_size)
{
    AVBPrint cmd;
    av_bprint_init(&cmd, 0, AV_BPRINT_SIZE_AUTOMATIC);
    av_bprintf(&cmd, "cddb query %s %i", discid, nb_tracks);
    for (int i = 0; i < nb_tracks; i++)
        av_bprintf(&cmd, " %u", offsets[i]);
    av_bprintf(&cmd, " %u", total_secs);

    char *resp = gnudb_request(ctx, cmd.str);
    av_bprint_finalize(&cmd, NULL);
    if (!resp)
        return 0;

    int ret = 0;
    char *save1, *save2;
    char *line = av_strtok(resp, "\r\n", &save1);
    char *categ = NULL, *match_id = NULL;

    if (line) {
        /* Keep the untouched status line around, in case we need to log it
         * below - av_strtok() below mutates it in place. */
        char status_line[256];
        av_strlcpy(status_line, line, sizeof(status_line));

        char *code_str = av_strtok(line, " ", &save2);
        int code = code_str ? atoi(code_str) : 0;

        if (code == 200) {
            /* Single exact match, on the same line as the status code */
            categ = av_strtok(NULL, " ", &save2);
            match_id = av_strtok(NULL, " ", &save2);
        } else if (code == 210 || code == 211) {
            /* Message line, then match lines until a lone "." - just take
             * the first one */
            char *entry = av_strtok(NULL, "\r\n", &save1);
            if (entry && strcmp(entry, ".")) {
                char *save3;
                categ = av_strtok(entry, " ", &save3);
                match_id = av_strtok(NULL, " ", &save3);
            }
        }

        if (!categ || !match_id)
            cyanrip_log(ctx, 0, "GNUdb: %s\n", status_line);
    }

    if (categ && match_id) {
        av_strlcpy(categ_out, categ, categ_size);
        av_strlcpy(discid_out, match_id, discid_size);
        ret = 1;
    }

    av_free(resp);
    return ret;
}

/* Sends "cddb read <categ> <discid>" and returns the entry's fields
 * (DTITLE, DYEAR, DGENRE, TTITLE0...) as a raw AVDictionary, or NULL. */
static AVDictionary *gnudb_read(cyanrip_ctx *ctx, const char *categ, const char *discid)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "cddb read %s %s", categ, discid);

    char *resp = gnudb_request(ctx, cmd);
    if (!resp)
        return NULL;

    char *save;
    char *line = av_strtok(resp, "\r\n", &save);
    if (!line || atoi(line) != 210) {
        if (line)
            cyanrip_log(ctx, 0, "GNUdb: %s\n", line);
        av_free(resp);
        return NULL;
    }

    AVDictionary *fields = NULL;
    while ((line = av_strtok(NULL, "\r\n", &save))) {
        if (!strcmp(line, "."))
            break;
        if (line[0] == '#' || line[0] == '\0')
            continue;

        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = '\0';
        if (eq[1])
            av_dict_set(&fields, line, eq + 1, 0);
    }

    av_free(resp);
    return fields;
}

static void gnudb_set(AVDictionary **dict, const char *key, const char *val)
{
    if (val && val[0])
        av_dict_set(dict, key, val, 0);
}

/* Splits "Artist / Title" into the two parts and sets them under
 * artist_key/title_key - the same convention libcddb (and so XLD) uses for
 * its cddb_disc_get_artist()/cddb_disc_get_title() split. Falls back to
 * setting just title_key if there's no " / " separator. */
static void gnudb_set_split(AVDictionary **dict, const char *artist_key,
                             const char *title_key, const char *val)
{
    if (!val || !val[0])
        return;

    const char *sep = strstr(val, " / ");
    if (!sep) {
        gnudb_set(dict, title_key, val);
        return;
    }

    char *artist = av_strndup(val, sep - val);
    gnudb_set(dict, artist_key, artist);
    av_free(artist);
    gnudb_set(dict, title_key, sep + 3);
}

void crip_fill_gnudb(cyanrip_ctx *ctx)
{
    if (!ctx->settings.gnudb_email || !ctx->settings.gnudb_host)
        return;

    const char *discid = dict_get(ctx->meta, "cddb");
    if (!discid)
        return;

    char discid_lc[9];
    av_strlcpy(discid_lc, discid, sizeof(discid_lc));
    for (char *p = discid_lc; *p; p++)
        *p = av_tolower(*p);

    /* The TOC the CDDB ID was computed from */
    if (!ctx->id_last_track)
        return;

    int nb_tracks = ctx->id_last_track - ctx->id_first_track + 1;
    uint32_t offsets[99];
    for (int i = 0; i < nb_tracks; i++)
        offsets[i] = ctx->id_offsets[ctx->id_first_track + i];

    uint32_t total_secs = ctx->id_offsets[0]/75 - offsets[0]/75;

    char categ[32], match_id[16];
    if (!gnudb_query(ctx, discid_lc, nb_tracks, offsets, total_secs,
                      categ, sizeof(categ), match_id, sizeof(match_id)))
        return; /* gnudb_query() already logged why */

    AVDictionary *fields = gnudb_read(ctx, categ, match_id);
    if (!fields)
        return; /* gnudb_read() already logged why */

    /* Disc-wide fields. Kept raw in ctx->gnudb, separate from ctx->meta, so
     * it survives ctx->meta later being overwritten by MusicBrainz or
     * manual (-a/-t) metadata - a MusicBrainz release seed URL can still be
     * built from it afterwards. */
    gnudb_set_split(&ctx->gnudb, "album_artist", "album", dict_get(fields, "DTITLE"));
    gnudb_set(&ctx->gnudb, "date", dict_get(fields, "DYEAR"));
    gnudb_set(&ctx->gnudb, "genre", dict_get(fields, "DGENRE"));

    /* Used as fallback tags for the rip itself */
    av_dict_copy(&ctx->meta, ctx->gnudb, 0);

    int nb_track_titles = 0;
    for (int i = 0; i < nb_tracks; i++) {
        char key[16];
        snprintf(key, sizeof(key), "TTITLE%i", i);

        cyanrip_track *t = &ctx->tracks[i];
        gnudb_set_split(&t->gnudb, "artist", "title", dict_get(fields, key));
        if (dict_get(t->gnudb, "title"))
            nb_track_titles++;

        av_dict_copy(&t->meta, t->gnudb, 0);
    }

    cyanrip_log(ctx, 0, "Found GNUdb entry: \"%s\" - \"%s\" (%i/%i track title(s))\n",
                dict_get(ctx->gnudb, "album") ? dict_get(ctx->gnudb, "album") : "?",
                dict_get(ctx->gnudb, "album_artist") ? dict_get(ctx->gnudb, "album_artist") : "?",
                nb_track_titles, nb_tracks);

    av_dict_free(&fields);
}
