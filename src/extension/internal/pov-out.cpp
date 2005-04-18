/*
 * A simple utility for exporting Inkscape svg Shapes as PovRay bezier
 * prisms.  Note that this is output-only, and would thus seem to be
 * better placed as an 'export' rather than 'output'.  However, Export
 * handles all or partial documents, while this outputs ALL shapes in
 * the current SVG document.
 *
 *  For information on the PovRay file format, see:
 *      http://www.povray.org
 *
 * Authors:
 *   Bob Jamison <rjamison@titan.com>
 *
 * Copyright (C) 2004 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "pov-out.h"
#include <inkscape.h>
#include <sp-path.h>
#include <style.h>
#include <color.h>
#include <display/curve.h>
#include <libnr/n-art-bpath.h>
#include <libnr/nr-point-matrix-ops.h>
#include <extension/system.h>
#include <extension/db.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <vector>
#include <string>

#include "io/sys.h"

namespace Inkscape {
namespace Extension {
namespace Internal {



/**
 * Make sure that we are in the database
 */
bool
PovOutput::check (Inkscape::Extension::Extension *module)
{
    /* We don't need a Key
    if (NULL == Inkscape::Extension::db.get(SP_MODULE_KEY_OUTPUT_POV))
        return FALSE;
    */

    return TRUE;
}




/**
 * This function searches the Repr tree recursively from the given node,
 * and adds refs to all nodes with the given name, to the result vector
 */
static void
findElementsByTagName(std::vector<Inkscape::XML::Node *> &results,
                      Inkscape::XML::Node *node,
                      char const *name)
{
    if ( !name
         || strcmp(node->name(), name) == 0 ) {
        results.push_back(node);
    }

    for (Inkscape::XML::Node *child = node->firstChild() ; child ; child = child->next())
        findElementsByTagName( results, child, name );

}


/**
 * used for saving information about shapes
 */
class PovShapeInfo
{
public:
    PovShapeInfo()
    {}
    virtual ~PovShapeInfo()
    {}
    std::string id;
    std::string color;
};




/**
 * Saves the <paths> of an Inkscape SVG file as PovRay spline definitions
*/
void
PovOutput::save(Inkscape::Extension::Output *mod, SPDocument *doc, gchar const *uri)
{
    std::vector<Inkscape::XML::Node *>results;
    //findElementsByTagName(results, SP_ACTIVE_DOCUMENT->rroot, "path");
    findElementsByTagName(results, SP_ACTIVE_DOCUMENT->rroot, NULL);//Check all nodes
    if (results.size() == 0)
        return;
    Inkscape::IO::dump_fopen_call(uri, "L");
    FILE *f = Inkscape::IO::fopen_utf8name(uri, "w");
    if (!f)
        return;

    time_t tim = time(NULL);
    fprintf(f, "/*#################################################\n");
    fprintf(f, "### This PovRay document was generated by Inkscape\n");
    fprintf(f, "### http://www.inkscape.org\n");
    fprintf(f, "### Created: %s", ctime(&tim));
    fprintf(f, "##################################################*/\n\n\n");

    std::vector<PovShapeInfo>povShapes; //A list for saving information about the shapes

    double bignum = 1000000.0;
    double minx  =  bignum;
    double maxx  = -bignum;
    double miny  =  bignum;
    double maxy  = -bignum;

    unsigned indx;
    for (indx = 0; indx < results.size() ; indx++)
    {
        //### Fetch the object from the repr info
        Inkscape::XML::Node *rpath = results[indx];
        gchar *id  = (gchar *)rpath->attribute("id");
        SPObject *reprobj = SP_ACTIVE_DOCUMENT->getObjectByRepr(rpath);
        if (!reprobj)
            continue;

        //### Get the transform of the item
        if (!SP_IS_ITEM(reprobj))
        {
            continue;
        }
        SPItem *item = SP_ITEM(reprobj);
        NR::Matrix tf = sp_item_i2d_affine(item);

        //### Get the Shape
        if (!SP_IS_SHAPE(reprobj))//Bulia's suggestion.  Allow all shapes
        {
            continue;
        }
        SPShape *shape = SP_SHAPE(reprobj);
        SPCurve *curve = shape->curve;
        if (sp_curve_empty(curve))
            continue;

        PovShapeInfo shapeInfo;

        shapeInfo.id           = id;
        shapeInfo.color        = "";

        //Try to get the fill color of the shape
        SPStyle *style = SP_OBJECT_STYLE(shape);
        /* fixme: Handle other fill types, even if this means translating gradients to a single
           flat colour. */
        if (style && (style->fill.type == SP_PAINT_TYPE_COLOR)) {
            // see color.h for how to parse SPColor
            float rgb[3];
            sp_color_get_rgb_floatv(&style->fill.value.color, rgb);
            double const dopacity = ( SP_SCALE24_TO_FLOAT(style->opacity.value) *
                                      SP_SCALE24_TO_FLOAT(style->fill_opacity.value) );
            gchar *str = g_strdup_printf("rgbf < %1.3f, %1.3f, %1.3f %1.3f>",
                                         rgb[0], rgb[1], rgb[2], 1.0 - dopacity);
            shapeInfo.color += str;
            g_free(str);
        }

        povShapes.push_back(shapeInfo); //passed all tests.  save the info

        int curveNr;

        //Count the NR_CURVETOs/LINETOs
        int segmentCount=0;
        NArtBpath *bp = curve->bpath;
        for (curveNr=0 ; curveNr<curve->length ; curveNr++, bp++)
            if (bp->code == NR_CURVETO || bp->code == NR_LINETO)
                segmentCount++;

        bp = curve->bpath;
        double cminx  =  bignum;
        double cmaxx  = -bignum;
        double cminy  =  bignum;
        double cmaxy  = -bignum;
        double lastx  = 0.0;
        double lasty  = 0.0;

        fprintf(f, "/*##############################################\n");
        fprintf(f, "### PRISM:  %s\n", id);
        fprintf(f, "##############################################*/\n");
        fprintf(f, "#declare %s = prism {\n", id);
        fprintf(f, "    linear_sweep\n");
        fprintf(f, "    bezier_spline\n");
        fprintf(f, "    1.0, //top\n");
        fprintf(f, "    0.0, //bottom\n");
        fprintf(f, "    %d, //nr points\n", segmentCount * 4);
        int segmentNr = 0;
        for (bp = curve->bpath, curveNr=0 ; curveNr<curve->length ; curveNr++, bp++) {
            using NR::X;
            using NR::Y;
            NR::Point const p1(bp->c(1) * tf);
            NR::Point const p2(bp->c(2) * tf);
            NR::Point const p3(bp->c(3) * tf);
            double const x1 = p1[X], y1 = p1[Y];
            double const x2 = p1[X], y2 = p1[Y];
            double const x3 = p1[X], y3 = p1[Y];

            switch (bp->code) {
                case NR_MOVETO:
                case NR_MOVETO_OPEN:
                    //fprintf(f, "moveto: %f %f\n", bp->x3, bp->y3);
                    break;
                case NR_CURVETO:

                    fprintf(f, "    /*%4d*/ <%f, %f>, <%f, %f>, <%f,%f>, <%f,%f>",
                            segmentNr++, lastx, lasty, x1, y1, x2, y2, x3, y3);

                    if (segmentNr < segmentCount)
                        fprintf(f, ",\n");
                    else
                        fprintf(f, "\n");

                    if (lastx < cminx)
                        cminx = lastx;
                    if (lastx > cmaxx)
                        cmaxx = lastx;
                    if (lasty < cminy)
                        cminy = lasty;
                    if (lasty > cmaxy)
                        cmaxy = lasty;
                    break;
                case NR_LINETO:

                    fprintf(f, "    /*%4d*/ <%f, %f>, <%f, %f>, <%f,%f>, <%f,%f>",
                            segmentNr++, lastx, lasty, lastx, lasty, x3, y3, x3, y3);

                    if (segmentNr < segmentCount)
                        fprintf(f, ",\n");
                    else
                        fprintf(f, "\n");

                    //fprintf(f, "lineto\n");
                    if (lastx < cminx)
                        cminx = lastx;
                    if (lastx > cmaxx)
                        cmaxx = lastx;
                    if (lasty < cminy)
                        cminy = lasty;
                    if (lasty > cmaxy)
                        cmaxy = lasty;
                    break;
                case NR_END:
                    //fprintf(f, "end\n");
                    break;
            }
            lastx = x3;
            lasty = y3;
        }
        fprintf(f, "}\n");
        fprintf(f, "#declare %s_MIN_X    = %4.3f;\n", id, cminx);
        fprintf(f, "#declare %s_CENTER_X = %4.3f;\n", id, (cmaxx+cminx)/2.0);
        fprintf(f, "#declare %s_MAX_X    = %4.3f;\n", id, cmaxx);
        fprintf(f, "#declare %s_WIDTH    = %4.3f;\n", id, cmaxx-cminx);
        fprintf(f, "#declare %s_MIN_Y    = %4.3f;\n", id, cminy);
        fprintf(f, "#declare %s_CENTER_Y = %4.3f;\n", id, (cmaxy+cminy)/2.0);
        fprintf(f, "#declare %s_MAX_Y    = %4.3f;\n", id, cmaxy);
        fprintf(f, "#declare %s_HEIGHT   = %4.3f;\n", id, cmaxy-cminy);
        if (shapeInfo.color.length()>0)
            fprintf(f, "#declare %s_COLOR    = %s;\n",
                    id, shapeInfo.color.c_str());
        fprintf(f, "/*##############################################\n");
        fprintf(f, "### end %s\n", id);
        fprintf(f, "##############################################*/\n\n\n\n");
        if (cminx < minx)
            minx = cminx;
        else if (cmaxx > maxx)
            maxx = cmaxx;
        if (cminy < miny)
            miny = cminy;
        else if (cmaxy > maxy)
            maxy = cmaxy;


    }//for



    //## Let's make a union of all of the Shapes
    if (!povShapes.empty()) {
        char const *id = "AllShapes";
        fprintf(f, "/*##############################################\n");
        fprintf(f, "### UNION OF ALL SHAPES IN DOCUMENT\n");
        fprintf(f, "##############################################*/\n");
        fprintf(f, "\n\n");
        fprintf(f, "/**\n");
        fprintf(f, " * Allow the user to redefine the finish{}\n");
        fprintf(f, " * by declaring it before #including this file\n");
        fprintf(f, " */\n");
        fprintf(f, "#ifndef (%s_Finish)\n", id);
        fprintf(f, "#declare %s_Finish = finish {\n", id);
        fprintf(f, "    phong 0.5\n");
        fprintf(f, "    reflection 0.3\n");
        fprintf(f, "    specular 0.5\n");
        fprintf(f, "}\n");
        fprintf(f, "#end\n");
        fprintf(f, "\n\n");
        fprintf(f, "#declare %s = union {\n", id);
        for (unsigned i = 0 ; i < povShapes.size() ; i++) {
            fprintf(f, "    object { %s\n", povShapes[i].id.c_str());
            fprintf(f, "        texture { \n");
            if (povShapes[i].color.length()>0)
                fprintf(f, "            pigment { %s }\n", povShapes[i].color.c_str());
            else
                fprintf(f, "            pigment { rgb <0,0,0> }\n");
            fprintf(f, "            finish { %s_Finish }\n", id);
            fprintf(f, "            } \n");
            fprintf(f, "        } \n");
        }
        fprintf(f, "}\n\n\n\n");


        double zinc   = 0.2 / (double)povShapes.size();
        fprintf(f, "/*#### Same union, but with Z-diffs (actually Y in pov) ####*/\n");
        fprintf(f, "\n\n");
        fprintf(f, "/**\n");
        fprintf(f, " * Allow the user to redefine the Z-Increment\n");
        fprintf(f, " */\n");
        fprintf(f, "#ifndef (AllShapes_Z_Increment)\n");
        fprintf(f, "#declare AllShapes_Z_Increment = %f;\n", zinc);
        fprintf(f, "#end\n");
        fprintf(f, "\n");
        fprintf(f, "#declare AllShapes_Z_Scale = 1.0;\n");
        fprintf(f, "\n\n");
        fprintf(f, "#declare %s_Z = union {\n", id);
        for (unsigned i = 0 ; i < povShapes.size() ; i++) {
            fprintf(f, "    object { %s\n", povShapes[i].id.c_str());
            fprintf(f, "        texture { \n");
            if (povShapes[i].color.length()>0)
                fprintf(f, "            pigment { %s }\n", povShapes[i].color.c_str());
            else
                fprintf(f, "            pigment { rgb <0,0,0> }\n");
            fprintf(f, "            finish { %s_Finish }\n", id);
            fprintf(f, "            } \n");
            fprintf(f, "        scale <1, %s_Z_Scale, 1>\n", id);
            fprintf(f, "        } \n");
            fprintf(f, "#declare %s_Z_Scale = %s_Z_Scale + %s_Z_Increment;\n\n",
                    id, id, id);
        }

        fprintf(f, "}\n");
        fprintf(f, "#declare %s_MIN_X    = %4.3f;\n", id, minx);
        fprintf(f, "#declare %s_CENTER_X = %4.3f;\n", id, (maxx+minx)/2.0);
        fprintf(f, "#declare %s_MAX_X    = %4.3f;\n", id, maxx);
        fprintf(f, "#declare %s_WIDTH    = %4.3f;\n", id, maxx-minx);
        fprintf(f, "#declare %s_MIN_Y    = %4.3f;\n", id, miny);
        fprintf(f, "#declare %s_CENTER_Y = %4.3f;\n", id, (maxy+miny)/2.0);
        fprintf(f, "#declare %s_MAX_Y    = %4.3f;\n", id, maxy);
        fprintf(f, "#declare %s_HEIGHT   = %4.3f;\n", id, maxy-miny);
        fprintf(f, "/*##############################################\n");
        fprintf(f, "### end %s\n", id);
        fprintf(f, "##############################################*/\n\n\n\n");
    }

    //All done
    fclose(f);
}

/**
 * This is the definition of PovRay output.  This function just
 * calls the extension system with the memory allocated XML that
 * describes the data.
*/
void
PovOutput::init()
{
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension>\n"
            "<name>PovRay Output</name>\n"
            "<id>org.inkscape.output.pov</id>\n"
            "<output>\n"
                "<extension>.pov</extension>\n"
                "<mimetype>text/x-povray-script</mimetype>\n"
                "<filetypename>PovRay (*.pov) (export splines)</filetypename>\n"
                "<filetypetooltip>PovRay Raytracer File</filetypetooltip>\n"
            "</output>\n"
        "</inkscape-extension>",
        new PovOutput());
}





}  //namespace Internal
}  //namespace Extension
}  //namespace Inkscape


/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :
