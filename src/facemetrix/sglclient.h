#ifndef SGL_CLIENT_H
#define SGL_CLIENT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define SGL_CLIENT_TYPE         (sgl_client_get_type ())
#define SGL_CLIENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), SGL_CLIENT_TYPE, SglClient))
#define SGL_CLIENT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), SGL_CLIENT_TYPE, SglClientClass))
#define SGL_IS_CLIENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), SGL_CLIENT_TYPE))
#define SGL_IS_CLIENT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), SGL_CLIENT_TYPE))
#define SGL_CLIENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), SGL_CLIENT_TYPE, SglClientClass))

typedef struct _SglClient        SglClient;
typedef struct _SglClientPrivate SglClientPrivate;
typedef struct _SglClientClass   SglClientClass;

struct _SglClient
{
    GObject parent;

    /*< private >*/
    SglClientPrivate *priv;
};

struct _SglClientClass
{
    GObjectClass parent_class;
};

GType    sgl_client_get_type(void);

gboolean sgl_client_open      (SglClient   *client,
                               const gchar *hostname,
                               const guint  port);

void     sgl_client_close     (SglClient   *client);

gchar*   sgl_client_recognize (SglClient   *client,
                               const gchar *data,
                               const guint  length);

G_END_DECLS

#endif
