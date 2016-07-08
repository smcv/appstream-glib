/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:as-suggest
 * @short_description: Object representing a single suggest used in a screenshot.
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * Screenshot may have multiple versions of an suggest in different resolutions
 * or aspect ratios. This object allows access to the location and size of a
 * single suggest.
 *
 * See also: #AsScreenshot
 */

#include "config.h"

#include "as-suggest-private.h"
#include "as-node-private.h"
#include "as-utils-private.h"

typedef struct
{
	AsSuggestKind		 kind;
	GPtrArray		*ids;	/* utf8 */
} AsSuggestPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsSuggest, as_suggest, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_suggest_get_instance_private (o))

static void
as_suggest_finalize (GObject *object)
{
	AsSuggest *suggest = AS_SUGGEST (object);
	AsSuggestPrivate *priv = GET_PRIVATE (suggest);

	g_ptr_array_unref (priv->ids);

	G_OBJECT_CLASS (as_suggest_parent_class)->finalize (object);
}

static void
as_suggest_init (AsSuggest *suggest)
{
}

static void
as_suggest_class_init (AsSuggestClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_suggest_finalize;
}


/**
 * as_suggest_kind_from_string:
 * @kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a #AsSuggestKind, or %AS_SUGGEST_KIND_UNKNOWN for unknown.
 *
 * Since: 0.5.18
 **/
AsSuggestKind
as_suggest_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "source") == 0)
		return AS_SUGGEST_KIND_SOURCE;
	return AS_SUGGEST_KIND_UNKNOWN;
}

/**
 * as_suggest_kind_to_string:
 * @kind: the #AsSuggestKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.5.18
 **/
const gchar *
as_suggest_kind_to_string (AsSuggestKind kind)
{
	if (kind == AS_SUGGEST_KIND_SOURCE)
		return "source";
	return NULL;
}

/**
 * as_suggest_get_ids:
 * @suggest: a #AsSuggest instance.
 *
 * Gets the suggest ids if set.
 *
 * Returns: (transfer none): the #GPtrArray, or %NULL
 *
 * Since: 0.5.18
 **/
GPtrArray *
as_suggest_get_ids (AsSuggest *suggest)
{
	AsSuggestPrivate *priv = GET_PRIVATE (suggest);
	return priv->ids;
}

/**
 * as_suggest_set_kind:
 * @suggest: a #AsSuggest instance.
 * @kind: the #AsSuggestKind, e.g. %AS_SUGGEST_KIND_THUMBNAIL.
 *
 * Sets the suggest kind.
 *
 * Since: 0.5.18
 **/
void
as_suggest_set_kind (AsSuggest *suggest, AsSuggestKind kind)
{
	AsSuggestPrivate *priv = GET_PRIVATE (suggest);
	priv->kind = kind;
}

/**
 * as_suggest_add_id:
 * @suggest: a #AsSuggest instance.
 * @ids: the #GPtrArray, or %NULL
 *
 * Add a the suggest application ID.
 *
 * Since: 0.1.6
 **/
void
as_suggest_add_id (AsSuggest *suggest, const gchar *id)
{
	AsSuggestPrivate *priv = GET_PRIVATE (suggest);
	g_ptr_array_add (priv->ids, g_strdup (id));
}

/**
 * as_suggest_node_insert: (skip)
 * @suggest: a #AsSuggest instance.
 * @parent: the parent #GNode to use..
 * @ctx: the #AsNodeContext
 *
 * Inserts the suggest into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode
 *
 * Since: 0.5.18
 **/
GNode *
as_suggest_node_insert (AsSuggest *suggest, GNode *parent, AsNodeContext *ctx)
{
	AsSuggestPrivate *priv = GET_PRIVATE (suggest);
	GNode *n;

	n = as_node_insert (parent, "suggest", priv->url,
			    AS_NODE_INSERT_FLAG_NONE,
			    "type", as_suggest_kind_to_string (priv->kind),
			    NULL);
	return n;
}

/**
 * as_suggest_node_parse:
 * @suggest: a #AsSuggest instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.18
 **/
gboolean
as_suggest_node_parse (AsSuggest *suggest, GNode *node,
		       AsNodeContext *ctx, GError **error)
{
	AsSuggestPrivate *priv = GET_PRIVATE (suggest);
	const gchar *tmp;
	gchar *taken;

	tmp = as_node_get_attribute (node, "type");
	if (tmp == NULL)
		as_suggest_set_kind (suggest, AS_SUGGEST_KIND_SOURCE);
	else
		as_suggest_set_kind (suggest, as_suggest_kind_from_string (tmp));
	taken = as_node_take_data (node);
	if (taken != NULL) {
		g_free (priv->url);
		priv->url = taken;
	}
	return TRUE;
}

/**
 * as_suggest_node_parse_dep11:
 * @suggest: a #AsSuggest instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DEP-11 node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.18
 **/
gboolean
as_suggest_node_parse_dep11 (AsSuggest *im, GNode *node,
			   AsNodeContext *ctx, GError **error)
{
	GNode *n;
	const gchar *tmp;

	for (n = node->children; n != NULL; n = n->next) {
		tmp = as_yaml_node_get_key (n);
		if (g_strcmp0 (tmp, "height") == 0)
			as_suggest_set_height (im, as_yaml_node_get_value_as_int (n));
		else if (g_strcmp0 (tmp, "width") == 0)
			as_suggest_set_width (im, as_yaml_node_get_value_as_int (n));
		else if (g_strcmp0 (tmp, "url") == 0) {
			const gchar *media_base_url = as_node_context_get_media_base_url (ctx);
			if (media_base_url != NULL) {
				g_autofree gchar *url = NULL;
				url = g_build_path ("/", media_base_url, as_yaml_node_get_value (n), NULL);
				as_suggest_set_url (im, url);
			} else {
				as_suggest_set_url (im, as_yaml_node_get_value (n));
			}
		}
	}
	return TRUE;
}

/**
 * as_suggest_new:
 *
 * Creates a new #AsSuggest.
 *
 * Returns: (transfer full): a #AsSuggest
 *
 * Since: 0.5.18
 **/
AsSuggest *
as_suggest_new (void)
{
	AsSuggest *suggest;
	suggest = g_object_new (AS_TYPE_SUGGEST, NULL);
	return AS_SUGGEST (suggest);
}
 
