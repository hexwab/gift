/* gtkcellrendererprogress.c
 * Copyright (C) 2002 Naba Kumar <kh_naba@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include "gtkcellrendererprogress.h"

static void gtk_cell_renderer_progress_init       (GtkCellRendererProgress      *celltext);
static void gtk_cell_renderer_progress_class_init (GtkCellRendererProgressClass *class);
static void gtk_cell_renderer_progress_finalize   (GObject                  *object);

static void gtk_cell_renderer_progress_get_property(GObject                  *object,
						  guint                     param_id,
						  GValue                   *value,
						  GParamSpec               *pspec);
static void gtk_cell_renderer_progress_set_property(GObject                  *object,
						  guint                     param_id,
						  const GValue             *value,
						  GParamSpec               *pspec);
static void gtk_cell_renderer_progress_get_size    (GtkCellRenderer          *cell,
					       GtkWidget                *widget,
					       GdkRectangle             *cell_area,
					       gint                     *x_offset,
					       gint                     *y_offset,
					       gint                     *width,
					       gint                     *height);
static void gtk_cell_renderer_progress_render      (GtkCellRenderer          *cell,
					       GdkWindow                *window,
					       GtkWidget                *widget,
					       GdkRectangle             *background_area,
					       GdkRectangle             *cell_area,
					       GdkRectangle             *expose_area,
					       guint                     flags);

enum {
  PROP_0,
  PROP_VALUE,
  PROP_LOW_VALUE,
  PROP_HIGH_VALUE,
  PROP_BACKGROUND_COLOR,
  PROP_BORDER_COLOR,
  PROP_LOW_COLOR,
  PROP_MED_COLOR,
  PROP_HIGH_COLOR,
  PROP_EMBED_TEXT,
  PROP_TEXT_COLOR
};

struct _GtkCellRendererProgressPriv {
    gfloat   value;
    gfloat   low_value;
    gfloat   high_value;
	GdkColor background_color;
    GdkColor border_color;
    GdkColor low_color;
    GdkColor med_color;
    GdkColor high_color;
    GdkColor text_color;
};

static gpointer parent_class;

/* Tilman: moved from util.c */
gchar* util_gdk_color_to_string(GdkColor* color)
{
    return g_strdup_printf ("#%04x%04x%04x", color->red, color->green, color->blue);
}

GtkType
gtk_cell_renderer_progress_get_type (void)
{
  static GtkType cell_progress_type = 0;

  if (!cell_progress_type)
    {
      static const GTypeInfo cell_progress_info =
      {
        sizeof (GtkCellRendererProgressClass),
        NULL,		/* base_init */
        NULL,		/* base_finalize */
        (GClassInitFunc) gtk_cell_renderer_progress_class_init,
        NULL,		/* class_finalize */
        NULL,		/* class_data */
        sizeof (GtkCellRendererProgress),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_cell_renderer_progress_init,
      };

      cell_progress_type = g_type_register_static (GTK_TYPE_CELL_RENDERER,
                                               "GtkCellRendererProgress",
                                               &cell_progress_info, 0);
    }

  return cell_progress_type;
}

static void
gtk_cell_renderer_progress_init (GtkCellRendererProgress *cellprogress)
{
    /* (unused) GtkCellRendererProgressPriv *priv; */
    
    cellprogress->priv = (GtkCellRendererProgressPriv*)g_new0(GtkCellRendererProgressPriv, 1);
    cellprogress->priv->low_value = 10;
    cellprogress->priv->high_value = 90;
    
#define SET_DEFAULT_RGB(color, r, g, b) \
    (color).red = (r);\
    (color).green = (g);\
    (color).blue = (b);\
    (color).pixel = 8;
    
    cellprogress->priv->value = 0;
    SET_DEFAULT_RGB(cellprogress->priv->background_color, 65535, 65535, 65535);
    SET_DEFAULT_RGB(cellprogress->priv->border_color, 0, 0, 0);
    SET_DEFAULT_RGB(cellprogress->priv->low_color, 0, 0, 32535);
    SET_DEFAULT_RGB(cellprogress->priv->med_color, 0, 32535, 0);
    SET_DEFAULT_RGB(cellprogress->priv->high_color, 32535, 0, 0);
    SET_DEFAULT_RGB(cellprogress->priv->text_color, 0, 0, 0);
}

static void
gtk_cell_renderer_progress_class_init (GtkCellRendererProgressClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  parent_class = g_type_class_peek_parent (class);
  
  object_class->finalize = gtk_cell_renderer_progress_finalize;
  
  object_class->get_property = gtk_cell_renderer_progress_get_property;
  object_class->set_property = gtk_cell_renderer_progress_set_property;

  cell_class->get_size = gtk_cell_renderer_progress_get_size;
  cell_class->render = gtk_cell_renderer_progress_render;
  
  g_object_class_install_property (object_class,
                                   PROP_VALUE,
                                   g_param_spec_float ("value",
                                                        "Value",
                                                        "Value of the progress bar.",
                                                        0, 100, 0,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_LOW_VALUE,
                                   g_param_spec_float ("low-value",
                                                        "Low Value",
                                                        "Low Value cut off of the progress bar.",
                                                        0, 100, 10,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_HIGH_VALUE,
                                   g_param_spec_float ("high-value",
                                                        "High Value",
                                                        "High Value cut off of the progress bar.",
                                                        0, 100, 90,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND_COLOR,
                                   g_param_spec_string ("background-color",
                                                        ("Background Color"),
                                                        ("Background color of the progress bar."),
                                                        (const char*)NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_BORDER_COLOR,
                                   g_param_spec_string ("border-color",
                                                        ("Border Color"),
                                                        ("Border color of the progress bar."),
                                                        (const char*)NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_LOW_COLOR,
                                   g_param_spec_string ("low-color",
                                                        ("Low Color"),
                                                        ("Low color of the progress bar."),
                                                        (const char*)NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_MED_COLOR,
                                   g_param_spec_string ("med-color",
                                                        ("Medium Color"),
                                                        ("Medium color of the progress bar."),
                                                        (const char*)NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_HIGH_COLOR,
                                   g_param_spec_string ("high-color",
                                                        ("High Color"),
                                                        ("High color of the progress bar."),
                                                        (const char*)NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_TEXT_COLOR,
                                   g_param_spec_string ("text-color",
                                                        ("Text Color"),
                                                        ("Text color of the progress bar."),
                                                        (const char*)NULL,
                                                        G_PARAM_READWRITE));
 
}

static void
gtk_cell_renderer_progress_get_property (GObject        *object,
                                         guint           param_id,
                                         GValue         *value,
                                         GParamSpec     *pspec)
{
  GtkCellRendererProgress *cellprogress = GTK_CELL_RENDERER_PROGRESS (object);

#define RETURN_COLOR_STRING(gdkcolor) \
      {\
        gchar* color_str;\
        color_str = util_gdk_color_to_string(&(gdkcolor));\
        g_value_set_string (value, color_str);\
        g_free(color_str);\
      }

  switch (param_id)
    {
    case PROP_VALUE:
      g_value_set_float (value, cellprogress->priv->value);
      break;
    case PROP_LOW_VALUE:
      g_value_set_float (value, cellprogress->priv->low_value);
      break;
    case PROP_HIGH_VALUE:
      g_value_set_float (value, cellprogress->priv->high_value);
      break;
    case PROP_BACKGROUND_COLOR:
      RETURN_COLOR_STRING(cellprogress->priv->background_color);
      break;
    case PROP_BORDER_COLOR:
      RETURN_COLOR_STRING(cellprogress->priv->border_color);
      break;
    case PROP_LOW_COLOR:
      RETURN_COLOR_STRING(cellprogress->priv->low_color);
      break;
    case PROP_MED_COLOR:
      RETURN_COLOR_STRING(cellprogress->priv->med_color);
      break;
    case PROP_HIGH_COLOR:
      RETURN_COLOR_STRING(cellprogress->priv->high_color);
      break;
    case PROP_TEXT_COLOR:
      RETURN_COLOR_STRING(cellprogress->priv->text_color);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_cell_renderer_progress_set_property (GObject      *object,
				     guint         param_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
  G_CONST_RETURN gchar* color_str;
  GtkCellRendererProgress *cellprogress = 
        GTK_CELL_RENDERER_PROGRESS (object);

  switch (param_id)
    {
    case PROP_VALUE:
      cellprogress->priv->value = g_value_get_float (value);
      break;
    case PROP_LOW_VALUE:
      cellprogress->priv->low_value = g_value_get_float (value);
      break;
    case PROP_HIGH_VALUE:
      cellprogress->priv->high_value = g_value_get_float (value);
      break;
    case PROP_BACKGROUND_COLOR:
      color_str = g_value_get_string (value);
      if (color_str) {
          gdk_color_parse(color_str, &cellprogress->priv->background_color);
      }
      break;
    case PROP_BORDER_COLOR:
      color_str = g_value_get_string (value);
      if (color_str) {
          gdk_color_parse(color_str, &cellprogress->priv->border_color);
      }
      break;
    case PROP_LOW_COLOR:
      color_str = g_value_get_string (value);
      if (color_str) {
          gdk_color_parse(color_str, &cellprogress->priv->low_color);
      }
      break;
    case PROP_MED_COLOR:
      color_str = g_value_get_string (value);
      if (color_str) {
          gdk_color_parse(color_str, &cellprogress->priv->med_color);
      }
      break;
    case PROP_HIGH_COLOR:
      color_str = g_value_get_string (value);
      if (color_str) {
          gdk_color_parse(color_str, &cellprogress->priv->high_color);
      }
      break;
    case PROP_TEXT_COLOR:
      color_str = g_value_get_string (value);
      if (color_str) {
          gdk_color_parse(color_str, &cellprogress->priv->text_color);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
	g_object_notify (object, "value");
}

static void
gtk_cell_renderer_progress_get_size (GtkCellRenderer *cell,
				 GtkWidget       *widget,
				 GdkRectangle    *cell_area,
				 gint            *x_offset,
				 gint            *y_offset,
				 gint            *width,
				 gint            *height)
{
  /* (unused) GtkCellRendererProgress *cellprogress = (GtkCellRendererProgress *) cell; */

  if (width)
    *width = 8;

  if (height)
    *height = 8;
}

GtkCellRenderer*
gtk_cell_renderer_progress_new (void)
{
  return GTK_CELL_RENDERER (g_object_new (gtk_cell_renderer_progress_get_type (), NULL));
}

static void
gtk_cell_renderer_progress_render (GtkCellRenderer    *cell,
			       GdkWindow          *window,
			       GtkWidget          *widget,
			       GdkRectangle       *background_area,
			       GdkRectangle       *cell_area,
			       GdkRectangle       *expose_area,
			       guint               flags)
{
  GtkCellRendererProgress *cellprogress = (GtkCellRendererProgress *) cell;
  GtkStateType state;
  /* (unused) gint x_offset;
   * (unused) gint y_offset;
   */
  GdkGC *gc;
  gint draw_width; 
  PangoFontDescription *font;
  PangoContext *context;
  PangoLayout *layout;
  gchar *text;
  int w,h;

  if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED)
    {
      if (GTK_WIDGET_HAS_FOCUS (widget))
        state = GTK_STATE_SELECTED;
      else
        state = GTK_STATE_ACTIVE;
    }
  else
    {
      if (GTK_WIDGET_STATE (widget) == GTK_STATE_INSENSITIVE)
        state = GTK_STATE_INSENSITIVE;
      else
        state = GTK_STATE_NORMAL;
    }

  gc = gdk_gc_new (window);
 
  gdk_gc_set_rgb_fg_color (gc, &cellprogress->priv->background_color);
  gdk_draw_rectangle (window,
                      gc,
                      TRUE,
                      cell_area->x,
                      cell_area->y,
                      cell_area->width,
                      cell_area->height);
  gdk_gc_set_rgb_fg_color (gc, &cellprogress->priv->border_color);
  
  gdk_draw_rectangle (window,
                      gc,
                      FALSE,
                      cell_area->x,
                      cell_area->y,
                      cell_area->width-2,
                      cell_area->height-1);
  if (cellprogress->priv->value < cellprogress->priv->low_value)
      gdk_gc_set_rgb_fg_color (gc, &cellprogress->priv->low_color);
  else if (cellprogress->priv->value > cellprogress->priv->high_value)
      gdk_gc_set_rgb_fg_color (gc, &cellprogress->priv->high_color);
  else
      gdk_gc_set_rgb_fg_color (gc, &cellprogress->priv->med_color);
  
  draw_width = ((cellprogress->priv->value)*(cell_area->width-4))/100.0;
  gdk_draw_rectangle (window,
                      gc,
                      TRUE,
                      cell_area->x+2,
                      cell_area->y+2,
                      draw_width,
                      cell_area->height-4);
  

  

  font =  pango_font_description_from_string ("8");
  context = gtk_widget_create_pango_context(widget);
  layout = pango_layout_new(context);
  
  pango_layout_set_font_description (layout, font);
  text = g_strdup_printf("%.1f%%",cellprogress->priv->value);
  pango_layout_set_text(layout, text, -1);
  
  gdk_gc_set_rgb_fg_color (gc, &cellprogress->priv->text_color);
  
  pango_layout_get_pixel_size(layout, &w, &h);
  w = (cell_area->width - w)/2;
  h = (cell_area->height - h)/2;
  gdk_draw_layout(window, gc, cell_area->x + w , cell_area->y + h, layout);
  
  g_free(text);
  g_object_unref(layout);
  g_object_unref(context);
  
  g_object_unref (G_OBJECT (gc));
}

static void
gtk_cell_renderer_progress_finalize (GObject *object)
{
  GtkCellRendererProgress *cellprogress = GTK_CELL_RENDERER_PROGRESS (object);
  g_free(cellprogress->priv);
  
  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}
