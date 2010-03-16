/*
 * Copyright (C) 2000-2003  David Helder
 * Copyright (C) 2009       Vetta Labs Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "sglclient.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gnet.h>

#define SGL_USERNAME                 "vetta"
#define SGL_PASSWORD                 "vetta"

#define SGL_OPEN_SESSION_REQUEST     "#open_session %s %s#"
#define SGL_OPEN_SESSION_RESPONSE_OK "#ok open_session#"

#define SGL_EXECUTE_REQUEST          "#execute facemetrix#"
#define SGL_EXECUTE_RESPONSE_OK      "#ok execute face_metrix#"

#define SGL_LIST_USERS_REQUEST       "#list_users recognizerid %s#"
#define SGL_LIST_USERS_RESPONSE      "#list_users_response"

#define SGL_RECOGNIZE_REQUEST        "#recognize reqid %d userid nobody recognizerid %s timestamp %s detect %s photo %s#"
#define SGL_RECOGNIZE_RESPONSE       "#recognize_response reqid %d userid "
#define SGL_RECOGNIZE_RESPONSE_FACE_COOR_PARAM "face_coordinates"

#define SGL_STORE_REQUEST            "#store reqid %d userid %s sourceid %s timestamp %s photo %s#"
#define SGL_STORE_RESPONSE_OK        "#ok store reqid %d#"

#define SGL_UPDATE_REQUEST           "#update_model recognizerid %s sourceid %s#"
#define SGL_UPDATE_RESPONSE_OK       "#ok update_model#"

#define SGL_REFERENCE_IMAGE_REQUEST  "#reference_image userid %s#"
#define SGL_REFERENCE_IMAGE_RESPONSE "#reference_image_response photo "

#define SGL_TIMESTAMP_FORMAT         "%H:%M:%S-%d/%m/%Y"
#define SGL_TIMESTAMP_EXAMPLE        "HH:MM:SS-DD/MM/YYYY"

#define MAX_CONNECT_RETRIES          3

// sgl client private members
#define SGL_CLIENT_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE((object), SGL_CLIENT_TYPE, SglClientPrivate))
struct _SglClientPrivate {
    GTcpSocket *socket;
    GIOChannel *iochannel;
    gchar      *hostname;
    guint       port;
    guint       reqid;
};
static gpointer sgl_client_parent_class = NULL;

// static function declarations
static GIOError gnet_read_sgl_command_dup   (GIOChannel* channel, gchar** bufferp, gsize* bytes_readp);
static gchar*   build_timestamp             ();
static gboolean iochannel_handler           (GIOChannel *iochannel, GIOCondition condition, gpointer user_data);

static gboolean sgl_client_check_connection (SglClient *client);
static gboolean sgl_client_connect          (SglClient *client);
static gboolean sgl_client_rpc              (SglClient *client, const gchar *request, const gsize request_length,
                                             gchar **response, gsize *response_length, const gchar *expected_response_prefix);
static void     sgl_client_class_init       (gpointer klass, gpointer data);
static void     sgl_client_init             (GTypeInstance *instance, gpointer data);
static void     sgl_client_dispose          (GObject *instance);
static void     sgl_client_finalize         (GObject *instance);

// --------------------------------------------------------------------

gboolean
sgl_client_open(SglClient *client, const gchar *hostname, const guint port)
{
    SglClientPrivate *priv = SGL_CLIENT_GET_PRIVATE(client);

    // sanity checks
    g_return_val_if_fail(client   != NULL, FALSE);
    g_return_val_if_fail(hostname != NULL, FALSE);

    if (priv->hostname != NULL)
        g_free(priv->hostname);
    priv->hostname = g_strdup(hostname);
    priv->port     = port;

    if ((priv->socket != NULL) || (priv->iochannel != NULL)) {
        g_warning("opening sgl connection on already connected client");
        sgl_client_close(client);
    }

    // connect to sgl server
    return sgl_client_connect(client);
}

void
sgl_client_close(SglClient *client)
{
    SglClientPrivate *priv = SGL_CLIENT_GET_PRIVATE(client);

    if (priv->socket != NULL)
        gnet_tcp_socket_delete(priv->socket);

    priv->socket      = NULL;
    priv->iochannel   = NULL;
    priv->reqid       = 0;
}

gchar**
sgl_client_list_users(SglClient *client, const gchar *recognizerid)
{
    gchar *request, *response, **user_list;
    gsize  response_length;

    // sanity checks
    g_return_val_if_fail(client != NULL, FALSE);

    request = g_strdup_printf(SGL_LIST_USERS_REQUEST, recognizerid);
    if (sgl_client_rpc(client, SGL_LIST_USERS_REQUEST, strlen(SGL_LIST_USERS_REQUEST),
                       &response, &response_length, SGL_LIST_USERS_RESPONSE) == FALSE) {
        g_free(request);
        return NULL;
    }

    response[response_length - 1] = '\0';
    user_list = g_strsplit(response + strlen(SGL_LIST_USERS_RESPONSE) + 1, " ", -1);

    g_free(request);
    g_free(response);

    return user_list;
}

gchar*
sgl_client_recognize(SglClient *client, const gchar *recognizerid, const gboolean detect,
                     const gchar* data, const guint length, guint *x1, guint *y1, guint *x2, guint *y2)
{
    gchar *request, *response, *expected_response;
    gchar *base64_data, *timestamp, *id, *id_start, *id_end;
    gsize response_length;
    SglClientPrivate *priv = SGL_CLIENT_GET_PRIVATE(client);

    // sanity checks
    g_return_val_if_fail(client != NULL, FALSE);
    g_return_val_if_fail(data   != NULL, FALSE);

    // convert data to base64 format
    base64_data = g_base64_encode((const guchar*) data, length);

    // build request string
    timestamp = build_timestamp();
    request = g_strdup_printf(SGL_RECOGNIZE_REQUEST, ++priv->reqid, recognizerid, timestamp, detect ? "Y" : "N", base64_data);
    g_free(base64_data);
    g_free(timestamp);

    // build expected response prefix
    expected_response = g_strdup_printf(SGL_RECOGNIZE_RESPONSE, priv->reqid);

    if (sgl_client_rpc(client, request, strlen(request), &response, &response_length,
                       expected_response) == FALSE) {
        g_free(request);
        g_free(expected_response);
        return FALSE;
    }

    // extract userid from the response
    id_start = response + strlen(expected_response);
    id_end   = index(id_start, ' ');
    if (id_end == NULL) {
        id_end = &response[response_length - 1];
    } else {
        gchar **coords = g_strsplit(id_end + 1, " ", -1);
        if ((g_strv_length(coords) == 5) && (strcmp(coords[0], SGL_RECOGNIZE_RESPONSE_FACE_COOR_PARAM) == 0)) {
            if (x1 != NULL) *x1 = strtol(coords[1], NULL, 0);
            if (y1 != NULL) *y1 = strtol(coords[2], NULL, 0);
            if (x2 != NULL) *x2 = strtol(coords[3], NULL, 0);
            if (y2 != NULL) *y2 = strtol(coords[4], NULL, 0);
        } else g_warning("invalid 'face coordinates' request parameters: '%s'", id_end);
        g_strfreev(coords);
    }
    id = g_strndup(id_start, id_end - id_start);

    g_free(request);
    g_free(response);
    g_free(expected_response);

    return id;
}

gboolean
sgl_client_store(SglClient *client, const gchar *userid, const gchar *sourceid, const gchar* data, const gsize length)
{
    gchar *request, *expected_response, *base64_data, *timestamp;
    SglClientPrivate *priv = SGL_CLIENT_GET_PRIVATE(client);

    // sanity checks
    g_return_val_if_fail(client != NULL, FALSE);
    g_return_val_if_fail(data   != NULL, FALSE);

    // convert data to base64 format
    base64_data = g_base64_encode((const guchar*) data, length);

    // build request string
    timestamp = build_timestamp();
    request = g_strdup_printf(SGL_STORE_REQUEST, ++priv->reqid, userid, sourceid, timestamp, base64_data);
    g_free(base64_data);
    g_free(timestamp);

    // build expected response prefix
    expected_response = g_strdup_printf(SGL_STORE_RESPONSE_OK, priv->reqid);

    if (sgl_client_rpc(client, request, strlen(request), NULL, NULL, expected_response) == FALSE) {
        g_free(request);
        g_free(expected_response);
        return FALSE;
    }

    g_free(request);
    g_free(expected_response);

    return TRUE;
}

gboolean
sgl_client_update(SglClient *client, const gchar *recognizerid, const gchar *sourceid)
{
    gchar *request;

    // sanity checks
    g_return_val_if_fail(client != NULL, FALSE);

    request = g_strdup_printf(SGL_UPDATE_REQUEST, recognizerid, sourceid);

    if (sgl_client_rpc(client, request, strlen(request), NULL, NULL, SGL_UPDATE_RESPONSE_OK) == FALSE) {
        g_free(request);
        return FALSE;
    }

    g_free(request);

    return TRUE;
}

void
sgl_client_reference_image(SglClient *client, const gchar *id, gchar** data, gsize* length)
{
    gchar *request, *response, *data_start, *data_end, *base64_data;
    gsize response_length;

    // sanity checks
    g_return_if_fail(client != NULL);

    // build reference_image request string and send it
    request = g_strdup_printf(SGL_REFERENCE_IMAGE_REQUEST, id);

    if (sgl_client_rpc(client, request, strlen(request), &response, &response_length,
                       SGL_REFERENCE_IMAGE_RESPONSE) == FALSE) {
        g_free(request);
        return;
    }

    data_start    = response + strlen(SGL_REFERENCE_IMAGE_RESPONSE);
    data_end      = &response[response_length - 1];
    base64_data = g_strndup(data_start, data_end - data_start);

    // convert image data from base64 format
    *data = (gchar*) g_base64_decode(base64_data, length);

    g_free(request);
    g_free(response);
    g_free(base64_data);
}

// adapted from gnet_iochannel_readline_strdup
static GIOError
gnet_read_sgl_command_dup(GIOChannel* channel, gchar** bufferp, gsize* bytes_readp)
{
    gsize rc, n, length;
    gchar c, *ptr, *buf;
    GIOError error = G_IO_ERROR_NONE;

    g_return_val_if_fail(channel     != NULL, G_IO_ERROR_INVAL);
    g_return_val_if_fail(bytes_readp != NULL, G_IO_ERROR_INVAL);

    length = 100;
    buf = (gchar*) g_malloc(length);
    ptr = buf;
    n = 1;

    while (TRUE) {
        error = gnet_io_channel_readn(channel, &c, 1, &rc);
        if (error == G_IO_ERROR_NONE && rc == 1) {        // read 1 char
            *ptr++ = c;
            if ((c == '#') && (n > 1)) break;
        } else if ((error == G_IO_ERROR_NONE) && (rc == 0)) { // read EOF
            if (n == 1) {
                // no data read
                *bytes_readp = 0;
                *bufferp     = NULL;
                g_free(buf);
                return G_IO_ERROR_NONE;
            } else break;
        } else if (error == G_IO_ERROR_AGAIN) {
            continue;
        } else {
            g_free(buf);
            return error;
        }

        if (++n >= length) {
            length *= 2;
            buf = g_realloc(buf, length);
            ptr = buf + n - 1;
        }
    }

    *ptr = 0;
    *bufferp = buf;
    *bytes_readp = n;
    return error;
}

static gboolean
iochannel_handler(GIOChannel *iochannel, GIOCondition condition, gpointer user_data)
{
    SglClient *client = SGL_CLIENT(user_data);

    // sanity checks
    g_return_val_if_fail(iochannel != NULL, FALSE);
    g_return_val_if_fail(client    != NULL, FALSE);

    g_print("[iochannel_handler] condition: %d\n", condition);

    if (condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL)) {
        g_print("received err/hup/nval condition (%d)\n", condition);
        sgl_client_close(client);
        return FALSE;
    } else if (condition & G_IO_IN) {
        gchar buffer[1024];
        guint nbytes;
        GIOStatus iostatus;

        iostatus = g_io_channel_read_chars(iochannel, buffer, 0, &nbytes, NULL);
        if (iostatus == G_IO_STATUS_ERROR || G_IO_STATUS_EOF) {
            sgl_client_close(client);
            return FALSE;
        } else {
            g_warning("[iochannel_handler] read %d bytes (return code: %d)", nbytes, iostatus);
        }
    }
    return TRUE;
}

static gboolean
sgl_client_check_connection(SglClient *client)
{
    SglClientPrivate *priv = SGL_CLIENT_GET_PRIVATE(client);

    return ((priv->socket != NULL) && (priv->iochannel != NULL));
}

static gboolean
sgl_client_connect(SglClient *client)
{
    SglClientPrivate *priv = SGL_CLIENT_GET_PRIVATE(client);

    // sanity checks
    g_return_val_if_fail(priv            != NULL, FALSE);
    g_return_val_if_fail(priv->socket    == NULL, FALSE);
    g_return_val_if_fail(priv->iochannel == NULL, FALSE);
    g_return_val_if_fail(priv->hostname  != NULL, FALSE);
    g_return_val_if_fail(priv->port      != 0,    FALSE);

    // connect to sgl server
    priv->socket = gnet_tcp_socket_connect(priv->hostname, priv->port);
    if (priv->socket == NULL) {
        g_warning("unable to connect to sgl server at %s:%d", priv->hostname, priv->port);
        return FALSE;
    }

    // cache & check iochannel pointer
    priv->iochannel = gnet_tcp_socket_get_io_channel(priv->socket);
    g_assert(priv->iochannel != NULL);

    // add error watches
    g_io_add_watch(priv->iochannel, G_IO_ERR | G_IO_HUP | G_IO_NVAL,
                   iochannel_handler, client);

    // send open_session request
    if (sgl_client_rpc(client, SGL_OPEN_SESSION_REQUEST, strlen(SGL_OPEN_SESSION_REQUEST),
                       NULL, NULL, SGL_OPEN_SESSION_RESPONSE_OK) == FALSE) {
        return FALSE;
    }

    // then, send execute request
    if (sgl_client_rpc(client, SGL_EXECUTE_REQUEST, strlen(SGL_EXECUTE_REQUEST),
                       NULL, NULL, SGL_EXECUTE_RESPONSE_OK) == FALSE) {
        return FALSE;
    }

    return TRUE;
}

static gboolean
sgl_client_rpc(SglClient *client, const gchar *request, const gsize request_length,
               gchar **response, gsize *response_length, const gchar *expected_response_prefix)
{
    SglClientPrivate *priv;
    gsize             lresponse_length, bytes_written, retries;
    gchar            *lresponse;
    GIOError          error;

    // sanity checks
    g_return_val_if_fail(client != NULL, FALSE);

    priv = SGL_CLIENT_GET_PRIVATE(client);
    g_return_val_if_fail(priv != NULL, FALSE);

    retries = 0;
retry:
    // check connection and reconnect if necessary
    if ((sgl_client_check_connection(client) == FALSE) &&
        (sgl_client_connect(client) == FALSE)) {
        return FALSE;
    }

    error = gnet_io_channel_writen(priv->iochannel, (gpointer) request, request_length, &bytes_written);
    if ((error != G_IO_ERROR_NONE) || (request_length != bytes_written)) {
        g_warning("unable to send 'recognize' request: %d (%s)", error, g_strerror(errno));
        return FALSE;
    }

    // read and parse the response
    lresponse = NULL; lresponse_length = 0;
    error = gnet_read_sgl_command_dup(priv->iochannel, &lresponse, &lresponse_length);
    if (error != G_IO_ERROR_NONE) {
        g_warning("unable to get 'recognize' response: %d (%s)", error, g_strerror(errno));
        return FALSE;
    }

    if (lresponse == NULL) { // EOF => likelly disconnect
        if (retries < MAX_CONNECT_RETRIES) {
            sgl_client_close(client);
            retries++;
            goto retry;
        } else {
            g_warning("max retries reached; aborting");
            return FALSE;
        }
    }

    if ((g_str_has_prefix(lresponse, expected_response_prefix) == FALSE) ||
        (g_str_has_suffix(lresponse, "#") == FALSE)) {
        g_warning("unable to parse response: (%s)", lresponse);
        g_free(lresponse);
        return FALSE;
    }

    // set output parameters, if requested
    if ((response != NULL) && (response_length != NULL)) {
        *response        = lresponse;
        *response_length = lresponse_length;
    } else {
        g_free(lresponse);
    }

    return TRUE;
}

static gchar*
build_timestamp()
{
    time_t t;
    struct tm *tt;
    gchar *timestamp;  
    guint timestamp_length;

    // print timestamp
    t = time(NULL);
    tt = localtime(&t);
    timestamp_length = strlen(SGL_TIMESTAMP_EXAMPLE) + 1;

    timestamp = g_new(char, timestamp_length);
    if (strftime(timestamp, timestamp_length, SGL_TIMESTAMP_FORMAT, tt) == 0) {
        g_warning("unable to get timestamp");
        return g_strdup("");
    }
    return timestamp;
}

// -- gtype boilerplate --

GType
sgl_client_get_type()
{
    static GType type = 0;
    if (type == 0) {
        static const GTypeInfo info = {
            sizeof (SglClientClass),
            NULL,                   // base_init
            NULL,                   // base_finalize
            sgl_client_class_init,  // class_init
            NULL,                   // class_finalize
            NULL,                   // class_data
            sizeof (SglClient),
            0,                      // n_preallocs
            sgl_client_init,        // instance_init
            NULL
        };
        type = g_type_register_static(G_TYPE_OBJECT, "SglClientType", &info, 0);
    }
    return type;
}

static void
sgl_client_class_init(gpointer klass, gpointer data)
{
    g_type_class_add_private(klass, sizeof(SglClientPrivate));
    G_OBJECT_CLASS(klass)->dispose  = sgl_client_dispose;
    G_OBJECT_CLASS(klass)->finalize = sgl_client_finalize;
    sgl_client_parent_class = g_type_class_peek_parent(klass);
}

static void
sgl_client_init(GTypeInstance *instance, gpointer data)
{
    SglClientPrivate *priv = SGL_CLIENT(instance)->priv = SGL_CLIENT_GET_PRIVATE(instance);
    priv->socket    = NULL;
    priv->iochannel = NULL;
    priv->reqid     = 0;
}

static void
sgl_client_dispose(GObject *object)
{
    SglClient        *client = SGL_CLIENT(object);
    SglClientPrivate *priv   = SGL_CLIENT_GET_PRIVATE(client);

    if (priv->socket != NULL) {
        g_warning("automatically closing sgl session");
        sgl_client_close(client);
    }
    if (priv->hostname != NULL)
        g_free(priv->hostname);

    G_OBJECT_CLASS(sgl_client_parent_class)->dispose(object);
}

static void
sgl_client_finalize(GObject *object)
{
    G_OBJECT_CLASS(sgl_client_parent_class)->finalize(object);
}
