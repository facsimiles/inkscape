#ifndef __NR_TYPEFACE_H__
#define __NR_TYPEFACE_H__

/*
 * Typeface and script library
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * This code is in public domain
 */

#define NR_TYPE_TYPEFACE (nr_typeface_get_type ())
#define NR_TYPEFACE(o) (NR_CHECK_INSTANCE_CAST ((o), NR_TYPE_TYPEFACE, NRTypeFace))
#define NR_IS_TYPEFACE(o) (NR_CHECK_INSTANCE_TYPE ((o), NR_TYPE_TYPEFACE))

struct NRTypeFace;
struct NRTypeFaceClass;

struct NRTypeFaceDef;
class NRTypePosDef;

#include <libnr/nr-forward.h>
#include <libnr/nr-path.h>
#include <libnr/nr-object.h>
#include <libnrtype/nr-font.h>
#include <libnrtype/nr-rasterfont.h>

#include "FontInstance.h"

enum {
	NR_TYPEFACE_METRICS_DEFAULT,
	NR_TYPEFACE_METRICS_HORIZONTAL,
	NR_TYPEFACE_METRICS_VERTICAL
};

enum {
	NR_TYPEFACE_LOOKUP_RULE_DEFAULT
};

struct NRTypeFaceDef {
	NRTypeFaceDef *next;
	NRType type;
	NRTypePosDef *pdef;
	unsigned int idx;
	gchar *name;
	gchar *family;
	NRTypeFace *typeface;
};

struct NRTypeFaceClass {
	NRObjectClass parent_class;

	void (* setup) (NRTypeFace *tface, NRTypeFaceDef *def);

	unsigned int (* attribute_get) (NRTypeFace *tf, const gchar *key, gchar *str, unsigned int size);
	NRBPath *(* glyph_outline_get) (NRTypeFace *tf, unsigned int glyph, unsigned int metrics, NRBPath *path, unsigned int ref);
	void (* glyph_outline_unref) (NRTypeFace *tf, unsigned int glyph, unsigned int metrics);
	NR::Point (* glyph_advance_get) (NRTypeFace *tf, unsigned int glyph, unsigned int metrics);
	unsigned int (* lookup) (NRTypeFace *tf, unsigned int rule, unsigned int glyph);
	NRFont *(* font_new) (NRTypeFace *tf, unsigned int metrics, NR::Matrix const transform);

	void (* font_free) (NRFont *font);
	NRBPath *(* font_glyph_outline_get) (NRFont *font, unsigned int glyph, NRBPath *path, unsigned int ref);
	void (* font_glyph_outline_unref) (NRFont *font, unsigned int glyph);
	NR::Point (* font_glyph_advance_get) (NRFont *font, unsigned int glyph);
	NRRect *(* font_glyph_area_get) (NRFont *font, unsigned int glyph, NRRect *area);
	NRRasterFont *(* rasterfont_new) (NRFont *font, NR::Matrix const transform);

	void (* rasterfont_free) (NRRasterFont *rfont);
	NR::Point (* rasterfont_glyph_advance_get) (NRRasterFont *rfont, unsigned int glyph);
	NRRect *(* rasterfont_glyph_area_get) (NRRasterFont *rfont, unsigned int glyph, NRRect *area);
	void (* rasterfont_glyph_mask_render) (NRRasterFont *rfont, unsigned int glyph, NRPixBlock *m, float x, float y);
};

struct NRTypeFace : public NRObject {
	NRTypeFaceDef *def;
	unsigned int nglyphs;
};

NRType nr_typeface_get_type (void);

NRTypeFace *nr_typeface_new (NRTypeFaceDef *def);

#if 0
/* NRTypeFace *nr_typeface_ref (NRTypeFace *tf); */
#define nr_typeface_ref(t) (NRTypeFace *) nr_object_ref ((NRObject *) (t))
/* NRTypeFace *nr_typeface_unref (NRTypeFace *tf); */
#define nr_typeface_unref(t) (NRTypeFace *) nr_object_unref ((NRObject *) (t))
#else
NRTypeFace* nr_typeface_ref(NRTypeFace* t);
NRTypeFace* nr_typeface_unref(NRTypeFace* t);
#endif

unsigned int nr_typeface_name_get (NRTypeFace *tf, gchar *str, unsigned int size);
unsigned int nr_typeface_family_name_get (NRTypeFace *tf, gchar *str, unsigned int size);
unsigned int nr_typeface_attribute_get (NRTypeFace *tf, const gchar *key, gchar *str, unsigned int size);

NRBPath *nr_typeface_glyph_outline_get (NRTypeFace *tf, unsigned int glyph, unsigned int metrics, NRBPath *d, unsigned int ref);
void nr_typeface_glyph_outline_unref (NRTypeFace *tf, unsigned int glyph, unsigned int metrics);
NR::Point nr_typeface_glyph_advance_get (NRTypeFace *tf, unsigned int glyph, unsigned int metrics);

unsigned int nr_typeface_lookup_default (NRTypeFace *tf, unsigned int unival);

NRFont *nr_font_new_default (NRTypeFace *tf, unsigned int metrics, float size);

void nr_type_empty_build_def (NRTypeFaceDef *def, const gchar *name, const gchar *family);

#endif
