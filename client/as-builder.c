/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <appstream-glib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "as-cleanup.h"
#include "asb-context.h"
#include "asb-utils.h"

/**
 * as_builder_search_path:
 **/
static gboolean
as_builder_search_path (GPtrArray *array, const gchar *path, GError **error)
{
	const gchar *filename;
	_cleanup_dir_close_ GDir *dir = NULL;

	dir = g_dir_open (path, 0, error);
	if (dir == NULL)
		return FALSE;
	while ((filename = g_dir_read_name (dir)) != NULL) {
		_cleanup_free_ gchar *tmp = NULL;
		tmp = g_build_filename (path, filename, NULL);
		if (g_file_test (tmp, G_FILE_TEST_IS_DIR)) {
			if (!as_builder_search_path (array, tmp, error))
				return FALSE;
		} else {
			g_ptr_array_add (array, g_strdup (tmp));
		}
	}
	return TRUE;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	AsbContext *ctx = NULL;
	GOptionContext *option_context;
	const gchar *filename;
	gboolean add_cache_id = FALSE;
	gboolean extra_checks = FALSE;
	gboolean no_net = FALSE;
	gboolean ret;
	gboolean verbose = FALSE;
	gchar *tmp;
	gdouble api_version = 0.0f;
	gint max_threads = 4;
	gint rc;
	guint i;
	_cleanup_dir_close_ GDir *dir = NULL;
	_cleanup_error_free_ GError *error = NULL;
	_cleanup_free_ gchar *basename = NULL;
	_cleanup_free_ gchar *cache_dir = NULL;
	_cleanup_free_ gchar *extra_appdata = NULL;
	_cleanup_free_ gchar *extra_appstream = NULL;
	_cleanup_free_ gchar *extra_screenshots = NULL;
	_cleanup_free_ gchar *log_dir = NULL;
	_cleanup_free_ gchar *old_metadata = NULL;
	_cleanup_free_ gchar *output_dir = NULL;
	_cleanup_free_ gchar *packages_dir = NULL;
	_cleanup_free_ gchar *screenshot_dir = NULL;
	_cleanup_free_ gchar *screenshot_uri = NULL;
	_cleanup_free_ gchar *temp_dir = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *packages = NULL;
	_cleanup_timer_destroy_ GTimer *timer = NULL;
	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ "no-net", '\0', 0, G_OPTION_ARG_NONE, &no_net,
			/* TRANSLATORS: command line option */
			_("Do not use the network to download screenshots"), NULL },
		{ "extra-checks", '\0', 0, G_OPTION_ARG_NONE, &extra_checks,
			/* TRANSLATORS: command line option */
			_("Perform extra checks on the source metadata"), NULL },
		{ "add-cache-id", '\0', 0, G_OPTION_ARG_NONE, &add_cache_id,
			/* TRANSLATORS: command line option */
			_("Add a cache ID to each component"), NULL },
		{ "log-dir", '\0', 0, G_OPTION_ARG_FILENAME, &log_dir,
			/* TRANSLATORS: command line option */
			_("Set the logging directory [default: ./logs]"), "DIR" },
		{ "screenshot-dir", '\0', 0, G_OPTION_ARG_FILENAME, &screenshot_dir,
			/* TRANSLATORS: command line option */
			_("Set the screenshots directory [default: ./screenshots]"), "DIR" },
		{ "packages-dir", '\0', 0, G_OPTION_ARG_FILENAME, &packages_dir,
			/* TRANSLATORS: command line option */
			_("Set the packages directory [default: ./packages]"), "DIR" },
		{ "temp-dir", '\0', 0, G_OPTION_ARG_FILENAME, &temp_dir,
			/* TRANSLATORS: command line option */
			_("Set the temporary directory [default: ./tmp]"), "DIR" },
		{ "extra-appstream-dir", '\0', 0, G_OPTION_ARG_FILENAME, &extra_appstream,
			/* TRANSLATORS: command line option */
			_("Use extra appstream data [default: ./appstream-extra]"), "DIR" },
		{ "extra-appdata-dir", '\0', 0, G_OPTION_ARG_FILENAME, &extra_appdata,
			/* TRANSLATORS: command line option */
			_("Use extra appdata data [default: ./appdata-extra]"), "DIR" },
		{ "extra-screenshots-dir", '\0', 0, G_OPTION_ARG_FILENAME, &extra_screenshots,
			/* TRANSLATORS: command line option */
			_("Use extra screenshots data [default: ./screenshots-extra]"), "DIR" },
		{ "output-dir", '\0', 0, G_OPTION_ARG_FILENAME, &output_dir,
			/* TRANSLATORS: command line option */
			_("Set the output directory [default: .]"), "DIR" },
		{ "cache-dir", '\0', 0, G_OPTION_ARG_FILENAME, &output_dir,
			/* TRANSLATORS: command line option */
			_("Set the cache directory [default: ./cache]"), "DIR" },
		{ "basename", '\0', 0, G_OPTION_ARG_STRING, &basename,
			/* TRANSLATORS: command line option */
			_("Set the origin name [default: fedora-21]"), "NAME" },
		{ "max-threads", '\0', 0, G_OPTION_ARG_INT, &max_threads,
			/* TRANSLATORS: command line option */
			_("Set the number of threads [default: 4]"), "THREAD_COUNT" },
		{ "api-version", '\0', 0, G_OPTION_ARG_DOUBLE, &api_version,
			/* TRANSLATORS: command line option */
			_("Set the AppStream version [default: 0.4]"), "API_VERSION" },
		{ "screenshot-uri", '\0', 0, G_OPTION_ARG_STRING, &screenshot_uri,
			/* TRANSLATORS: command line option */
			_("Set the screenshot base URL [default: none]"), "URI" },
		{ "old-metadata", '\0', 0, G_OPTION_ARG_FILENAME, &old_metadata,
			/* TRANSLATORS: command line option */
			_("Set the old metadata location [default: none]"), "DIR" },
		{ NULL}
	};

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
	option_context = g_option_context_new (NULL);

	g_option_context_add_main_entries (option_context, options, NULL);
	ret = g_option_context_parse (option_context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: error message */
		g_print (_("Failed to parse arguments: %s\n"), error->message);
		goto out;
	}

	if (verbose)
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	if (extra_checks)
		g_setenv ("ASB_PERFORM_EXTRA_CHECKS", "1", TRUE);

#if !GLIB_CHECK_VERSION(2,40,0)
	if (max_threads > 1) {
		/* TRANSLATORS: debug message */
		g_debug ("O_CLOEXEC not available, using 1 core");
		max_threads = 1;
	}
#endif
	/* set defaults */
	if (api_version < 0.01)
		api_version = 0.41;
	if (packages_dir == NULL)
		packages_dir = g_strdup ("./packages");
	if (temp_dir == NULL)
		temp_dir = g_strdup ("./tmp");
	if (log_dir == NULL)
		log_dir = g_strdup ("./logs");
	if (screenshot_dir == NULL)
		screenshot_dir = g_strdup ("./screenshots");
	if (output_dir == NULL)
		output_dir = g_strdup (".");
	if (cache_dir == NULL)
		cache_dir = g_strdup ("./cache");
	if (basename == NULL)
		basename = g_strdup ("fedora-21");
	if (screenshot_uri == NULL)
		screenshot_uri = g_strdup ("http://alt.fedoraproject.org/pub/alt/screenshots/f21/");
	if (extra_appstream == NULL)
		extra_appstream = g_strdup ("./appstream-extra");
	if (extra_appdata == NULL)
		extra_appdata = g_strdup ("./appdata-extra");
	if (extra_screenshots == NULL)
		extra_screenshots = g_strdup ("./screenshots-extra");
	setlocale (LC_ALL, "");

	/* set up state */
	ret = asb_utils_ensure_exists_and_empty (temp_dir, &error);
	if (!ret) {
		/* TRANSLATORS: error message */
		g_warning (_("failed to create temp dir: %s"), error->message);
		goto out;
	}
	tmp = g_build_filename (temp_dir, "icons", NULL);
	if (old_metadata != NULL) {
		add_cache_id = TRUE;
		ret = g_file_test (tmp, G_FILE_TEST_EXISTS);
		if (!ret) {
			/* TRANSLATORS: error message */
			g_warning (_("%s has to exist to use old metadata"), tmp);
			goto out;
		}
	} else {
		ret = asb_utils_ensure_exists_and_empty (tmp, &error);
		if (!ret) {
			/* TRANSLATORS: error message */
			g_warning (_("failed to create icons dir: %s"), error->message);
			goto out;
		}
	}
	g_free (tmp);
	rc = g_mkdir_with_parents (log_dir, 0700);
	if (rc != 0) {
		/* TRANSLATORS: error message */
		g_warning (_("failed to create log dir"));
		goto out;
	}
	rc = g_mkdir_with_parents (output_dir, 0700);
	if (rc != 0) {
		/* TRANSLATORS: error message */
		g_warning (_("failed to create log dir"));
		goto out;
	}
	tmp = g_build_filename (screenshot_dir, "112x63", NULL);
	rc = g_mkdir_with_parents (tmp, 0700);
	g_free (tmp);
	if (rc != 0) {
		/* TRANSLATORS: error message */
		g_warning (_("failed to create screenshot cache dir"));
		goto out;
	}
	tmp = g_build_filename (screenshot_dir, "624x351", NULL);
	rc = g_mkdir_with_parents (tmp, 0700);
	g_free (tmp);
	if (rc != 0) {
		/* TRANSLATORS: error message */
		g_warning (_("failed to create screenshot cache dir"));
		goto out;
	}
	tmp = g_build_filename (screenshot_dir, "752x423", NULL);
	rc = g_mkdir_with_parents (tmp, 0700);
	g_free (tmp);
	if (rc != 0) {
		/* TRANSLATORS: error message */
		g_warning (_("failed to create screenshot cache dir"));
		goto out;
	}
	tmp = g_build_filename (screenshot_dir, "source", NULL);
	rc = g_mkdir_with_parents (tmp, 0700);
	g_free (tmp);
	if (rc != 0) {
		/* TRANSLATORS: error message */
		g_warning (_("failed to create screenshot cache dir"));
		goto out;
	}
	rc = g_mkdir_with_parents (cache_dir, 0700);
	if (rc != 0) {
		/* TRANSLATORS: error message */
		g_warning (_("failed to create cache dir"));
		goto out;
	}

	ctx = asb_context_new ();
	asb_context_set_no_net (ctx, no_net);
	asb_context_set_api_version (ctx, api_version);
	asb_context_set_add_cache_id (ctx, add_cache_id);
	asb_context_set_extra_checks (ctx, extra_checks);
	asb_context_set_old_metadata (ctx, old_metadata);
	asb_context_set_extra_appstream (ctx, extra_appstream);
	asb_context_set_extra_appdata (ctx, extra_appdata);
	asb_context_set_extra_screenshots (ctx, extra_screenshots);
	asb_context_set_screenshot_uri (ctx, screenshot_uri);
	asb_context_set_log_dir (ctx, log_dir);
	asb_context_set_screenshot_dir (ctx, screenshot_dir);
	asb_context_set_temp_dir (ctx, temp_dir);
	asb_context_set_output_dir (ctx, output_dir);
	asb_context_set_cache_dir (ctx, cache_dir);
	asb_context_set_basename (ctx, basename);
	asb_context_set_max_threads (ctx, max_threads);
	ret = asb_context_setup (ctx, &error);
	if (!ret) {
		/* TRANSLATORS: error message */
		g_warning (_("failed to set up context: %s"), error->message);
		goto out;
	}

	/* scan each package */
	packages = g_ptr_array_new_with_free_func (g_free);
	if (argc == 1) {
		if (!as_builder_search_path (packages, packages_dir, &error)) {
			/* TRANSLATORS: error message */
			g_warning (_("failed to open packages: %s"), error->message);
			goto out;
		}
	} else {
		for (i = 1; i < (guint) argc; i++)
			g_ptr_array_add (packages, g_strdup (argv[i]));
	}
	/* TRANSLATORS: information message */
	g_print (_("Scanning packages...\n"));
	timer = g_timer_new ();
	for (i = 0; i < packages->len; i++) {
		_cleanup_error_free_ GError *error_local = NULL;

		filename = g_ptr_array_index (packages, i);

		/* anything in the cache */
		if (asb_context_find_in_cache (ctx, filename)) {
			/* TRANSLATORS: debug message */
			g_debug (_("Skipping %s as found in old md cache"),
				 filename);
			continue;
		}

		/* add to list */
		if (!asb_context_add_filename (ctx, filename, &error_local)) {
			g_print ("Failed to add %s: %s\n",
				 filename, error_local->message);
			continue;
		}
		if (g_timer_elapsed (timer, NULL) > 3.f) {
			/* TRANSLATORS: information message */
			g_print (_("Parsed %i/%i files...\n"),
				 i, packages->len);
			g_timer_reset (timer);
		}
	}

	/* disable anything not newest */
	asb_context_disable_older_pkgs (ctx);

	/* process all packages in the pool */
	ret = asb_context_process (ctx, &error);
	if (!ret) {
		/* TRANSLATORS: error message */
		g_warning (_("failed to process context: %s"), error->message);
		goto out;
	}

	/* success */
	/* TRANSLATORS: information message */
	g_print (_("Done!\n"));
out:
	g_option_context_free (option_context);
	if (ctx != NULL)
		g_object_unref (ctx);
	return 0;
}