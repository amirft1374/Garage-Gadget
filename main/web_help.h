#pragma once

/*
 * web_help.h — The in-device documentation shown on the Help tab.
 *
 * The content is generated from the project's Markdown files by
 * tools/gen_help.py, so there is exactly one source of truth: edit the .md
 * files, re-run the script, rebuild.
 *
 * Help is served without authentication, deliberately. Someone who has
 * forgotten the admin password, or whose device has dropped into its rescue
 * portal, needs the recovery instructions precisely when they cannot sign
 * in — and none of this content is specific to the unit.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *category;   /* grouping shown in the Help sidebar   */
    const char *title;      /* page title, also the list entry      */
    const char *html;       /* rendered body, flash-resident        */
} help_topic_t;

extern const help_topic_t k_help_topics[];
extern const int k_help_topic_count;

/* Every topic concatenated as one Markdown document, served for download.
 * Markdown rather than the rendered HTML because the usual reason to want
 * the whole manual as a file is to paste it into an AI assistant, and that
 * reads Markdown far better than a wall of tags. */
extern const char k_help_markdown[];

#ifdef __cplusplus
}
#endif
