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

#include <gnet.h>
#include <string.h>
#include <errno.h>

#define SGL_USERNAME                 "vetta"
#define SGL_PASSWORD                 "vetta"

#define SGL_OPEN_SESSION_REQUEST     "#open_session %s %s#"
#define SGL_EXECUTE_REQUEST          "#execute facemetrix#"
#define SGL_RECOGNIZE_REQUEST        "#recognize reqid %d userid gama timestamp %s store N photo %s#"
#define SGL_OPEN_SESSION_RESPONSE_OK "#ok open_session#"
#define SGL_EXECUTE_RESPONSE_OK      "#ok execute face_metrix#"
#define SGL_RECOGNIZE_RESPONSE_OK    "#recognize_response reqid %d userid "

#define SGL_TIMESTAMP_FORMAT         "%H:%M:%S-%d/%m/%Y"

// sgl client private members
#define SGL_CLIENT_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE((object), SGL_CLIENT_TYPE, SglClientPrivate))
struct _SglClientPrivate {
    GTcpSocket* socket;
    GIOChannel* iochannel;
    guint       reqid;
};
static gpointer sgl_client_parent_class = NULL;

// static function declarations
static GIOError gnet_read_sgl_command_dup (GIOChannel* channel, gchar** bufferp, gsize* bytes_readp);
static void sgl_client_class_init         (gpointer klass, gpointer data);
static void sgl_client_init               (GTypeInstance *instance, gpointer data);
static void sgl_client_dispose            (GObject *instance);
static void sgl_client_finalize           (GObject *instance);

// --------------------------------------------------------------------

gboolean
sgl_client_open(SglClient *client, const gchar *hostname, const guint port)
{
    gchar *request, *response;
    gsize request_length, response_length, bytes_written;
    GIOError error = G_IO_ERROR_NONE;
    SglClientPrivate *priv = SGL_CLIENT_GET_PRIVATE(client);

    // connect to sgl server
    priv->socket = gnet_tcp_socket_connect(hostname, port);
    if (priv->socket == NULL) {
        g_warning("unable to connect to sgl server at %s:%d", hostname, port);
        return FALSE;
    }

    // cache & check iochannel pointer
    priv->iochannel = gnet_tcp_socket_get_io_channel(priv->socket);
    g_assert(priv->iochannel != NULL);

    // build open_session request string and send it
    request = g_strdup_printf(SGL_OPEN_SESSION_REQUEST, SGL_USERNAME, SGL_PASSWORD);
    request_length = strlen(request);
    error = gnet_io_channel_writen(priv->iochannel, request, request_length, &bytes_written);
    if ((error != G_IO_ERROR_NONE) || (request_length != bytes_written)) {
        g_warning("unable to send 'open_session' request: %d (%s)", error, g_strerror(errno));
        g_free(request);
        return FALSE;
    }
    g_free(request);

    // read and parse open_session response
    error = gnet_read_sgl_command_dup(priv->iochannel, &response, &response_length);
    if (error != G_IO_ERROR_NONE) {
        g_warning("unable to get 'open_session' response: %d (%s)", error, g_strerror(errno));
        g_free(response);
        return FALSE;
    }
    if (strcmp(response, SGL_OPEN_SESSION_RESPONSE_OK) != 0) {
        g_warning("unable to parse 'open_session' response: (%s)", response);
        g_free(response);
        return FALSE;
    }
    g_free(response);

    // build execute request string and send it
    request = g_strdup_printf(SGL_EXECUTE_REQUEST);
    request_length = strlen(request);
    error = gnet_io_channel_writen(priv->iochannel, request, request_length, &bytes_written);
    if ((error != G_IO_ERROR_NONE) || (request_length != bytes_written)) {
        g_warning("unable to send 'execute' request: %d (%s)", error, g_strerror(errno));
        g_free(request);
        return FALSE;
    }
    g_free(request);

    // read and parse execute response
    error = gnet_read_sgl_command_dup(priv->iochannel, &response, &response_length);
    if (error != G_IO_ERROR_NONE) {
        g_warning("unable to get 'execute' response: %d (%s)", error, g_strerror(errno));
        g_free(response);
        return FALSE;
    }
    if (strcmp(response, SGL_EXECUTE_RESPONSE_OK) != 0) {
        g_warning("unable to parse 'execute' response: (%s)", response);
        g_free(response);
        return FALSE;
    }
    g_free(response);

    return TRUE;
}

void
sgl_client_close(SglClient *client)
{
    SglClientPrivate *priv = SGL_CLIENT_GET_PRIVATE(client);

    gnet_tcp_socket_delete(priv->socket);
    priv->socket    = NULL;
    priv->iochannel = NULL;
}

gchar*
sgl_client_recognize(SglClient *client, const gchar* data, const guint length)
{
    gchar *request, *response, *expected_recognize_response;
    gchar *base64_data, *id, *id_start, *id_end;
    gsize request_length, response_length, bytes_written;
    GIOError error = G_IO_ERROR_NONE;
    SglClientPrivate *priv = SGL_CLIENT_GET_PRIVATE(client);

    time_t t;
    struct tm *tt;
    gchar  timestamp[128];

    // sanity checks
    g_return_val_if_fail(client          != NULL, FALSE);
    g_return_val_if_fail(priv->socket    != NULL, FALSE);
    g_return_val_if_fail(priv->iochannel != NULL, FALSE);
    g_return_val_if_fail(data            != NULL, FALSE);

    // print timestamp
    t = time(NULL);
    tt = localtime(&t);
    if (strftime(timestamp, sizeof(timestamp), SGL_TIMESTAMP_FORMAT, tt) == 0) {
        g_warning("unable to get timestamp");
        return NULL;
    }

    // convert data to base64 format
    base64_data = g_base64_encode((const guchar*) data, length);

    // build request string and send it
    request = g_strdup_printf(SGL_RECOGNIZE_REQUEST, ++priv->reqid, timestamp, base64_data);
    request_length = strlen(request);
    g_free(base64_data);
    error = gnet_io_channel_writen(priv->iochannel, request, request_length, &bytes_written);
    if ((error != G_IO_ERROR_NONE) || (request_length != bytes_written)) {
        g_warning("unable to send 'recognize' request: %d (%s)", error, g_strerror(errno));
        g_free(request);
        return FALSE;
    }
    g_free(request);

    // read and parse the response
    error = gnet_read_sgl_command_dup(priv->iochannel, &response, &response_length);
    if (error != G_IO_ERROR_NONE) {
        g_warning("unable to get 'recognize' response: %d (%s)", error, g_strerror(errno));
        return FALSE;
    }
    expected_recognize_response = g_strdup_printf(SGL_RECOGNIZE_RESPONSE_OK, priv->reqid);
    if ((g_str_has_prefix(response, expected_recognize_response) == FALSE) ||
        (g_str_has_suffix(response, "#") == FALSE)) {
        g_warning("unable to parse 'recognize' response: (%s)", response);
        g_free(expected_recognize_response);
        g_free(response);
        return FALSE;
    }

    // extract userid from the response
    id_start = response + strlen(expected_recognize_response);
    id_end   = &response[response_length - 1];
    id = g_strndup(id_start, id_end - id_start);

    g_free(expected_recognize_response);
    g_free(response);

    return id;
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
    priv->reqid     = 0;
    priv->socket    = NULL;
    priv->iochannel = NULL;
}

static void
sgl_client_dispose(GObject *object)
{
    SglClient *client = SGL_CLIENT(object);

    if (client->priv->socket != NULL) {
        g_warning("automatically closing sgl session");
        sgl_client_close(client);
    }

    G_OBJECT_CLASS(sgl_client_parent_class)->dispose(object);
}

static void
sgl_client_finalize(GObject *object)
{
    G_OBJECT_CLASS(sgl_client_parent_class)->finalize(object);
}
