#ifndef __SP_VERBS_H__
#define __SP_VERBS_H__

/*
 * Frontend to actions
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * This code is in public domain
 */

#include "helper/action.h"
#include "view.h"

enum {
	/* Header */
	SP_VERB_INVALID,
	SP_VERB_NONE,
	/* File */
	SP_VERB_FILE_NEW,
	SP_VERB_FILE_OPEN,
	SP_VERB_FILE_SAVE,
	SP_VERB_FILE_SAVE_AS,
	SP_VERB_FILE_PRINT,
	SP_VERB_FILE_PRINT_DIRECT,
	SP_VERB_FILE_PRINT_PREVIEW,
	SP_VERB_FILE_IMPORT,
	SP_VERB_FILE_EXPORT,
	SP_VERB_FILE_NEXT_DESKTOP,
	SP_VERB_FILE_PREV_DESKTOP,
	SP_VERB_FILE_QUIT,
	/* Edit */
	SP_VERB_EDIT_UNDO,
	SP_VERB_EDIT_REDO,
	SP_VERB_EDIT_CUT,
	SP_VERB_EDIT_COPY,
	SP_VERB_EDIT_PASTE,
	SP_VERB_EDIT_PASTE_STYLE,
	SP_VERB_EDIT_DELETE,
	SP_VERB_EDIT_DUPLICATE,
	SP_VERB_EDIT_CLEAR_ALL,
	SP_VERB_EDIT_SELECT_ALL,
	/* Selection */
	SP_VERB_SELECTION_TO_FRONT,
	SP_VERB_SELECTION_TO_BACK,
	SP_VERB_SELECTION_RAISE,
	SP_VERB_SELECTION_LOWER,
	SP_VERB_SELECTION_GROUP,
	SP_VERB_SELECTION_UNGROUP,
	SP_VERB_SELECTION_UNION,
	SP_VERB_SELECTION_INTERSECT,
	SP_VERB_SELECTION_DIFF,
	SP_VERB_SELECTION_SYMDIFF,
	SP_VERB_SELECTION_OFFSET,
	SP_VERB_SELECTION_INSET,
	SP_VERB_SELECTION_DYNAMIC_OFFSET,
	SP_VERB_SELECTION_LINKED_OFFSET,
	SP_VERB_SELECTION_OUTLINE,
	SP_VERB_SELECTION_SIMPLIFY,
	SP_VERB_SELECTION_COMBINE,
	SP_VERB_SELECTION_BREAK_APART,
	/* Object */
	SP_VERB_OBJECT_ROTATE_90,
	SP_VERB_OBJECT_FLATTEN,
	SP_VERB_OBJECT_TO_CURVE,
	SP_VERB_OBJECT_FLIP_HORIZONTAL,
	SP_VERB_OBJECT_FLIP_VERTICAL,
	/* Event contexts */
	SP_VERB_CONTEXT_SELECT,
	SP_VERB_CONTEXT_NODE,
	SP_VERB_CONTEXT_RECT,
	SP_VERB_CONTEXT_ARC,
	SP_VERB_CONTEXT_STAR,
	SP_VERB_CONTEXT_SPIRAL,
	SP_VERB_CONTEXT_PENCIL,
	SP_VERB_CONTEXT_PEN,
	SP_VERB_CONTEXT_CALLIGRAPHIC,
	SP_VERB_CONTEXT_TEXT,
	SP_VERB_CONTEXT_ZOOM,
	SP_VERB_CONTEXT_DROPPER,
	/* Zooming and desktop settings */
	SP_VERB_ZOOM_IN,
	SP_VERB_ZOOM_OUT,
	SP_VERB_TOGGLE_RULERS,
	SP_VERB_TOGGLE_SCROLLBARS,
	SP_VERB_TOGGLE_GRID,
	SP_VERB_TOGGLE_GUIDES,
	SP_VERB_ZOOM_NEXT,
	SP_VERB_ZOOM_PREV,
	SP_VERB_ZOOM_1_1,
	SP_VERB_ZOOM_1_2,
	SP_VERB_ZOOM_2_1,
	SP_VERB_ZOOM_PAGE,
	SP_VERB_ZOOM_PAGE_WIDTH,
	SP_VERB_ZOOM_DRAWING,
	SP_VERB_ZOOM_SELECTION,
	SP_VERB_FULLSCREEN,
	/* Dialogs */
	SP_VERB_DIALOG_DISPLAY,
	SP_VERB_DIALOG_NAMEDVIEW,
	SP_VERB_DIALOG_TOOL_OPTIONS,
	SP_VERB_DIALOG_FILL_STROKE,
	SP_VERB_DIALOG_TRANSFORM,
	SP_VERB_DIALOG_ALIGN_DISTRIBUTE,
	SP_VERB_DIALOG_TEXT,
	SP_VERB_DIALOG_XML_EDITOR,
	SP_VERB_DIALOG_TOGGLE,
	SP_VERB_DIALOG_ITEM,
	/* Footer */
	SP_VERB_LAST
};

typedef int sp_verb_t;

class SPVerbActionFactory {
public:
	virtual SPAction *make_action(sp_verb_t verb, SPView *view)=0;
};

SPAction *sp_verb_get_action (sp_verb_t verb, SPView *view);
sp_verb_t sp_verb_register (SPVerbActionFactory *factory);

gchar *sp_action_get_title (const SPAction *action);

void sp_fullscreen(SPDesktop *sv); /* fixme: put somewhere appropriate */

#endif
