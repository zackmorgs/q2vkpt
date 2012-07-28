/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
// r_main.c

#include "sw.h"

viddef_t    vid;

refcfg_t    r_config;

entity_t    r_worldentity;

refdef_t    r_newrefdef;
model_t     *currentmodel;

bsp_t       *r_worldmodel;

byte        r_warpbuffer[WARP_WIDTH * WARP_HEIGHT];

swstate_t sw_state;

vec3_t      viewlightvec;
alight_t    r_viewlighting = {128, 192, viewlightvec};
float       r_time1;
int         r_numallocatededges;
float       r_aliasuvscale = 1.0;
int         r_outofsurfaces;
int         r_outofedges;

qboolean    r_dowarp;

int         c_surf;
int         r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
qboolean    r_surfsonstack;
int         r_clipflags;

//
// view origin
//
vec3_t  vup, base_vup;
vec3_t  vpn, base_vpn;
vec3_t  vright, base_vright;
vec3_t  r_origin;

//
// screen size info
//
oldrefdef_t r_refdef;
float       xcenter, ycenter;
float       xscale, yscale;
float       xscaleinv, yscaleinv;
float       xscaleshrink, yscaleshrink;
float       aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int     r_screenwidth;

float   verticalFieldOfView;
float   xOrigin, yOrigin;

cplane_t    screenedge[4];

//
// refresh flags
//
int     r_framecount = 1;   // so frame counts initialized to 0 don't match
int     r_visframecount;
int     d_spanpixcount;
int     r_polycount;
int     r_drawnpolycount;
int     r_wholepolycount;

int         *pfrustum_indexes[4];
int         r_frustum_indexes[4 * 6];

mleaf_t     *r_viewleaf;
int         r_viewcluster, r_oldviewcluster;

float   da_time1, da_time2, dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
float   se_time1, se_time2, de_time1, de_time2;

void R_MarkLeaves(void);

cvar_t  *sw_aliasstats;
cvar_t  *sw_allow_modex;
cvar_t  *sw_clearcolor;
cvar_t  *sw_drawflat;
cvar_t  *sw_draworder;
cvar_t  *sw_maxedges;
cvar_t  *sw_maxsurfs;
cvar_t  *sw_reportedgeout;
cvar_t  *sw_reportsurfout;
cvar_t  *sw_stipplealpha;
cvar_t  *sw_surfcacheoverride;
cvar_t  *sw_waterwarp;

//Start Added by Lewey
// These flags allow you to turn SIRDS on and
// off from the console.
cvar_t  *sw_drawsird;
//End Added by Lewey

cvar_t  *r_drawworld;
cvar_t  *r_drawentities;
cvar_t  *r_dspeeds;
cvar_t  *r_fullbright;
cvar_t  *r_lerpmodels;
cvar_t  *r_novis;

cvar_t  *r_speeds;

cvar_t  *vid_gamma;

//PGM
cvar_t  *sw_lockpvs;
//PGM

#if USE_ASM

void            *d_pcolormap;

#else // USE_ASM

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver

float   d_sdivzstepu, d_tdivzstepu, d_zistepu;
float   d_sdivzstepv, d_tdivzstepv, d_zistepv;
float   d_sdivzorigin, d_tdivzorigin, d_ziorigin;

fixed16_t   sadjust, tadjust, bbextents, bbextentt;

pixel_t         *cacheblock;
int             cachewidth;
pixel_t         *d_viewbuffer;
short           *d_pzbuffer;
unsigned int    d_zrowbytes;
unsigned int    d_zwidth;

#endif  // !USE_ASM

int     sintable[CYCLE * 2];
int     intsintable[CYCLE * 2];
int     blanktable[CYCLE * 2];      // PGM

/*
================
R_InitTurb
================
*/
void R_InitTurb(void)
{
    int     i;

    for (i = 0; i < CYCLE * 2; i++) {
        sintable[i] = AMP + sin(i * M_PI * 2 / CYCLE) * AMP;
        intsintable[i] = AMP2 + sin(i * M_PI * 2 / CYCLE) * AMP2; // AMP2, not 20
        blanktable[i] = 0;          //PGM
    }
}

void D_SCDump_f(void);

void R_Register(void)
{
    sw_aliasstats = Cvar_Get("sw_polymodelstats", "0", 0);
    sw_allow_modex = Cvar_Get("sw_allow_modex", "1", CVAR_ARCHIVE);
    sw_clearcolor = Cvar_Get("sw_clearcolor", "2", 0);
    sw_drawflat = Cvar_Get("sw_drawflat", "0", CVAR_CHEAT);
    sw_draworder = Cvar_Get("sw_draworder", "0", CVAR_CHEAT);
    sw_maxedges = Cvar_Get("sw_maxedges", va("%i", NUMSTACKEDGES), 0);
    sw_maxsurfs = Cvar_Get("sw_maxsurfs", va("%i", NUMSTACKSURFACES), 0);
    sw_mipcap = Cvar_Get("sw_mipcap", "0", 0);
    sw_mipscale = Cvar_Get("sw_mipscale", "1", 0);
    sw_reportedgeout = Cvar_Get("sw_reportedgeout", "0", 0);
    sw_reportsurfout = Cvar_Get("sw_reportsurfout", "0", 0);
    sw_stipplealpha = Cvar_Get("sw_stipplealpha", "0", CVAR_ARCHIVE);
    sw_waterwarp = Cvar_Get("sw_waterwarp", "1", 0);

    //Start Added by Lewey
    sw_drawsird = Cvar_Get("sw_drawsird", "0", 0);
    //End Added by Lewey

    r_speeds = Cvar_Get("r_speeds", "0", 0);
    r_fullbright = Cvar_Get("r_fullbright", "0", CVAR_CHEAT);
    r_drawentities = Cvar_Get("r_drawentities", "1", 0);
    r_drawworld = Cvar_Get("r_drawworld", "1", CVAR_CHEAT);
    r_dspeeds = Cvar_Get("r_dspeeds", "0", 0);
    r_lerpmodels = Cvar_Get("r_lerpmodels", "1", 0);
    r_novis = Cvar_Get("r_novis", "0", 0);

    vid_gamma = Cvar_Get("vid_gamma", "1.0", CVAR_ARCHIVE);

    Cmd_AddCommand("scdump", D_SCDump_f);

//PGM
    sw_lockpvs = Cvar_Get("sw_lockpvs", "0", 0);
//PGM

}

void R_UnRegister(void)
{
    Cmd_RemoveCommand("screenshot");
    Cmd_RemoveCommand("scdump");
}

void R_ModeChanged(int width, int height, int flags, int rowbytes, void *pixels)
{
    vid.width = width > MAXWIDTH ? MAXWIDTH : width;
    vid.height = height > MAXHEIGHT ? MAXHEIGHT : height;
    vid.buffer = pixels;
    vid.rowbytes = rowbytes;

    r_config.width = vid.width;
    r_config.height = vid.height;
    r_config.flags = flags;

    sw_surfcacheoverride = Cvar_Get("sw_surfcacheoverride", "0", 0);

    D_FlushCaches();

    if (d_pzbuffer) {
        Z_Free(d_pzbuffer);
        d_pzbuffer = NULL;
    }

    // free surface cache
    if (sc_base) {
        Z_Free(sc_base);
        sc_base = NULL;
    }

    d_pzbuffer = R_Mallocz(vid.width * vid.height * 2);

    R_InitCaches();

    R_GammaCorrectAndSetPalette((const byte *) d_8to24table);
}

/*
===============
R_Init
===============
*/
qboolean R_Init(qboolean total)
{
    Com_DPrintf("R_Init( %i )\n", total);

    if (!total) {
        R_InitImages();
        R_InitDraw();
        MOD_Init();
        return qtrue;
    }

    Com_Printf("ref_soft " VERSION ", " __DATE__ "\n");

#if USE_ASM
    Sys_MakeCodeWriteable((uintptr_t)R_EdgeCodeStart,
                          (uintptr_t)R_EdgeCodeEnd - (uintptr_t)R_EdgeCodeStart);
#endif

    r_aliasuvscale = 1.0;

    // create the window
    if (!VID_Init()) {
        return qfalse;
    }

    R_Register();

    IMG_Init();
    MOD_Init();

    /* get the palette before we create the window */
    R_InitImages();

    R_InitSkyBox();

    view_clipplanes[0].leftedge = qtrue;
    view_clipplanes[1].rightedge = qtrue;
    view_clipplanes[1].leftedge =
        view_clipplanes[2].leftedge =
            view_clipplanes[3].leftedge = qfalse;
    view_clipplanes[0].rightedge =
        view_clipplanes[2].rightedge =
            view_clipplanes[3].rightedge = qfalse;

    r_refdef.xOrigin = XCENTERING;
    r_refdef.yOrigin = YCENTERING;

    R_InitTurb();

    R_GammaCorrectAndSetPalette((const byte *) d_8to24table);
    vid_gamma->modified = qfalse;

    return qtrue;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown(qboolean total)
{
    Com_DPrintf("R_Shutdown( %i )\n", total);

    D_FlushCaches();

    MOD_Shutdown();

    R_ShutdownImages();

    // free world model
    if (r_worldmodel) {
        BSP_Free(r_worldmodel);
        r_worldmodel = NULL;
    }

    if (!total) {
        return;
    }

    // free z buffer
    if (d_pzbuffer) {
        Z_Free(d_pzbuffer);
        d_pzbuffer = NULL;
    }

    // free surface cache
    if (sc_base) {
        Z_Free(sc_base);
        sc_base = NULL;
    }

    // free colormap
    if (vid.colormap) {
        Z_Free(vid.colormap);
        vid.colormap = NULL;
    }

    R_UnRegister();

    IMG_Shutdown();

    VID_Shutdown();
}

/*
===============
R_NewMap
===============
*/
void R_NewMap(void)
{
    r_viewcluster = -1;

    r_cnumsurfs = sw_maxsurfs->integer;

    if (r_cnumsurfs <= MINSURFACES)
        r_cnumsurfs = MINSURFACES;

    if (r_cnumsurfs > NUMSTACKSURFACES) {
        surfaces = R_Mallocz(r_cnumsurfs * sizeof(surf_t));
        surface_p = surfaces;
        surf_max = &surfaces[r_cnumsurfs];
        r_surfsonstack = qfalse;
        // surface 0 doesn't really exist; it's just a dummy because index 0
        // is used to indicate no edge attached to surface
        surfaces--;
        R_SurfacePatch();
    } else {
        r_surfsonstack = qtrue;
    }

    r_maxedgesseen = 0;
    r_maxsurfsseen = 0;

    r_numallocatededges = sw_maxedges->integer;

    if (r_numallocatededges < MINEDGES)
        r_numallocatededges = MINEDGES;

    if (r_numallocatededges <= NUMSTACKEDGES) {
        auxedges = NULL;
    } else {
        auxedges = R_Mallocz(r_numallocatededges * sizeof(edge_t));
    }
}


/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
void R_MarkLeaves(void)
{
    byte    vis[VIS_MAX_BYTES];
    mnode_t *node;
    int     i;
    mleaf_t *leaf;
    int     cluster;

    if (r_oldviewcluster == r_viewcluster && !r_novis->integer &&
        r_viewcluster != -1) {
        return;
    }

    // development aid to let you run around and see exactly where
    // the pvs ends
    if (sw_lockpvs->integer)
        return;

    r_visframecount++;
    r_oldviewcluster = r_viewcluster;

    if (r_novis->integer || r_viewcluster == -1 || !r_worldmodel->vis) {
        // mark everything
        for (i = 0; i < r_worldmodel->numleafs; i++)
            r_worldmodel->leafs[i].visframe = r_visframecount;
        for (i = 0; i < r_worldmodel->numnodes; i++)
            r_worldmodel->nodes[i].visframe = r_visframecount;
        return;
    }

    BSP_ClusterVis(r_worldmodel, vis, r_viewcluster, DVIS_PVS);

    for (i = 0, leaf = r_worldmodel->leafs; i < r_worldmodel->numleafs; i++, leaf++) {
        cluster = leaf->cluster;
        if (cluster == -1)
            continue;
        if (Q_IsBitSet(vis, cluster)) {
            node = (mnode_t *)leaf;
            do {
                if (node->visframe == r_visframecount)
                    break;
                node->visframe = r_visframecount;
                node = node->parent;
            } while (node);
        }
    }

}

/*
** R_DrawNullModel
**
** IMPLEMENT THIS!
*/
static void R_DrawNullModel(void)
{
}

static int R_DrawEntities(int translucent)
{
    int         i;
    qboolean    translucent_entities = 0;

    // all bmodels have already been drawn by the edge list
    for (i = 0; i < r_newrefdef.num_entities; i++) {
        currententity = &r_newrefdef.entities[i];

        if ((currententity->flags & RF_TRANSLUCENT) == translucent) {
            translucent_entities++;
            continue;
        }

        if (currententity->flags & RF_BEAM) {
            modelorg[0] = -r_origin[0];
            modelorg[1] = -r_origin[1];
            modelorg[2] = -r_origin[2];
            VectorCopy(vec3_origin, r_entorigin);
            R_DrawBeam(currententity);
        } else {
            if (currententity->model & 0x80000000) {
                continue;
            }
            currentmodel = MOD_ForHandle(currententity->model);
            if (!currentmodel) {
                R_DrawNullModel();
                continue;
            }
            VectorCopy(currententity->origin, r_entorigin);
            VectorSubtract(r_origin, r_entorigin, modelorg);

            switch (currentmodel->type) {
            case MOD_ALIAS:
                R_AliasDrawModel();
                break;
            case MOD_SPRITE:
                R_DrawSprite();
                break;
            case MOD_EMPTY:
                break;
            default:
                Com_Error(ERR_FATAL, "%s: bad model type", __func__);
            }
        }
    }
    return translucent_entities;
}

/*
=============
R_DrawEntitiesOnList
=============
*/
static void R_DrawEntitiesOnList(void)
{
    int         translucent_entities;

    if (!r_drawentities->integer)
        return;

    translucent_entities = R_DrawEntities(RF_TRANSLUCENT);
    if (translucent_entities) {
        R_DrawEntities(0);
    }
}


/*
=============
R_BmodelCheckBBox
=============
*/
int R_BmodelCheckBBox(float *minmaxs)
{
    int         i, *pindex, clipflags;
    vec3_t      acceptpt, rejectpt;
    float       d;

    clipflags = 0;

    for (i = 0; i < 4; i++) {
        // generate accept and reject points
        // FIXME: do with fast look-ups or integer tests based on the sign bit
        // of the floating point values

        pindex = pfrustum_indexes[i];

        rejectpt[0] = minmaxs[pindex[0]];
        rejectpt[1] = minmaxs[pindex[1]];
        rejectpt[2] = minmaxs[pindex[2]];

        d = DotProduct(rejectpt, view_clipplanes[i].normal);
        d -= view_clipplanes[i].dist;

        if (d <= 0)
            return BMODEL_FULLY_CLIPPED;

        acceptpt[0] = minmaxs[pindex[3 + 0]];
        acceptpt[1] = minmaxs[pindex[3 + 1]];
        acceptpt[2] = minmaxs[pindex[3 + 2]];

        d = DotProduct(acceptpt, view_clipplanes[i].normal);
        d -= view_clipplanes[i].dist;

        if (d <= 0)
            clipflags |= (1 << i);
    }

    return clipflags;
}


/*
===================
R_FindTopnode

Find the first node that splits the given box
===================
*/
mnode_t *R_FindTopnode(vec3_t mins, vec3_t maxs)
{
    int         sides;
    mnode_t *node;

    node = r_worldmodel->nodes;

    while (node->visframe == r_visframecount) {
        if (!node->plane) {
            if (((mleaf_t *)node)->contents != CONTENTS_SOLID)
                return node; // we've reached a non-solid leaf, so it's
            //  visible and not BSP clipped
            return NULL;    // in solid, so not visible
        }

        sides = BoxOnPlaneSideFast(mins, maxs, node->plane);

        if (sides == 3)
            return node;    // this is the splitter

        // not split yet; recurse down the contacted side
        if (sides & 1)
            node = node->children[0];
        else
            node = node->children[1];
    }

    return NULL;        // not visible at all
}


/*
=============
RotatedBBox

Returns an axially aligned box that contains the input box at the given rotation
=============
*/
void RotatedBBox(vec3_t mins, vec3_t maxs, vec3_t angles,
                 vec3_t tmins, vec3_t tmaxs)
{
    vec3_t  tmp, v;
    int     i, j;
    vec3_t  forward, right, up;

    if (!angles[0] && !angles[1] && !angles[2]) {
        VectorCopy(mins, tmins);
        VectorCopy(maxs, tmaxs);
        return;
    }

    for (i = 0; i < 3; i++) {
        tmins[i] = 99999;
        tmaxs[i] = -99999;
    }

    AngleVectors(angles, forward, right, up);

    for (i = 0; i < 8; i++) {
        if (i & 1)
            tmp[0] = mins[0];
        else
            tmp[0] = maxs[0];

        if (i & 2)
            tmp[1] = mins[1];
        else
            tmp[1] = maxs[1];

        if (i & 4)
            tmp[2] = mins[2];
        else
            tmp[2] = maxs[2];


        VectorScale(forward, tmp[0], v);
        VectorMA(v, -tmp[1], right, v);
        VectorMA(v, tmp[2], up, v);

        for (j = 0; j < 3; j++) {
            if (v[j] < tmins[j])
                tmins[j] = v[j];
            if (v[j] > tmaxs[j])
                tmaxs[j] = v[j];
        }
    }
}

/*
=============
R_DrawBEntitiesOnList
=============
*/
void R_DrawBEntitiesOnList(void)
{
    int         i, index, clipflags;
    vec3_t      oldorigin;
    vec3_t      mins, maxs;
    float       minmaxs[6];
    mnode_t     *topnode;
    mmodel_t    *model;

    if (!r_drawentities->value)
        return;

    VectorCopy(modelorg, oldorigin);
    insubmodel = qtrue;
    r_dlightframecount = r_framecount;

    for (i = 0; i < r_newrefdef.num_entities; i++) {
        currententity = &r_newrefdef.entities[i];
        index = currententity->model;
        if (!(index & 0x80000000)) {
            continue;
        }
        index = ~index;
        if (index < 1 || index >= r_worldmodel->nummodels) {
            Com_Error(ERR_DROP, "%s: inline model %d out of range",
                      __func__, index);
        }
        model = &r_worldmodel->models[index];
        if (model->numfaces == 0)
            continue;   // clip brush only
        if (currententity->flags & RF_BEAM)
            continue;
        // see if the bounding box lets us trivially reject, also sets
        // trivial accept status
        RotatedBBox(model->mins, model->maxs,
                    currententity->angles, mins, maxs);
        VectorAdd(mins, currententity->origin, minmaxs);
        VectorAdd(maxs, currententity->origin, (minmaxs + 3));

        clipflags = R_BmodelCheckBBox(minmaxs);
        if (clipflags == BMODEL_FULLY_CLIPPED)
            continue;   // off the edge of the screen

        topnode = R_FindTopnode(minmaxs, minmaxs + 3);
        if (!topnode)
            continue;   // no part in a visible leaf

        VectorCopy(currententity->origin, r_entorigin);
        VectorSubtract(r_origin, r_entorigin, modelorg);

        // FIXME: stop transforming twice
        R_RotateBmodel();

        // calculate dynamic lighting for bmodel
        R_PushDlights(model->headnode);

        if (topnode->plane) {
            // not a leaf; has to be clipped to the world BSP
            r_clipflags = clipflags;
            R_DrawSolidClippedSubmodelPolygons(model, topnode);
        } else {
            // falls entirely in one leaf, so we just put all the
            // edges in the edge list and let 1/z sorting handle
            // drawing order
            R_DrawSubmodelPolygons(model, clipflags, topnode);
        }

        // put back world rotation and frustum clipping
        // FIXME: R_RotateBmodel should just work off base_vxx
        VectorCopy(base_vpn, vpn);
        VectorCopy(base_vup, vup);
        VectorCopy(base_vright, vright);
        VectorCopy(oldorigin, modelorg);
        R_TransformFrustum();
    }

    insubmodel = qfalse;
}


/*
================
R_EdgeDrawing
================
*/
void R_EdgeDrawing(void)
{
    edge_t  ledges[NUMSTACKEDGES +
                   ((CACHE_SIZE - 1) / sizeof(edge_t)) + 1];
    surf_t  lsurfs[NUMSTACKSURFACES +
                   ((CACHE_SIZE - 1) / sizeof(surf_t)) + 1];

    if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
        return;

    if (auxedges) {
        r_edges = auxedges;
    } else {
        r_edges = (edge_t *)
                  (((long)&ledges[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
    }

    if (r_surfsonstack) {
        surfaces = (surf_t *)
                   (((long)&lsurfs[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
        surf_max = &surfaces[r_cnumsurfs];
        // surface 0 doesn't really exist; it's just a dummy because index 0
        // is used to indicate no edge attached to surface
        surfaces--;
        R_SurfacePatch();
    }

    R_BeginEdgeFrame();

    if (r_dspeeds->integer) {
        rw_time1 = Sys_Milliseconds();
    }

    R_RenderWorld();

    if (r_dspeeds->integer) {
        rw_time2 = Sys_Milliseconds();
        db_time1 = rw_time2;
    }

    R_DrawBEntitiesOnList();

    if (r_dspeeds->integer) {
        db_time2 = Sys_Milliseconds();
        se_time1 = db_time2;
    }

    R_ScanEdges();
}

//=======================================================================


/*
=============
R_CalcPalette

=============
*/
void R_CalcPalette(void)
{
    static qboolean modified;
    byte    palette[256 * 4], *in, *out;
    int     i;
    float   alpha, one_minus_alpha;
    vec3_t  premult;
    int     r, g, b;

    alpha = r_newrefdef.blend[3];
    if (alpha <= 0) {
        if (modified) {
            // set back to default
            modified = qfalse;
            R_GammaCorrectAndSetPalette((const byte *) d_8to24table);
        }
        return;
    }

    modified = qtrue;
    if (alpha > 1)
        alpha = 1;

    premult[0] = r_newrefdef.blend[0] * alpha * 255;
    premult[1] = r_newrefdef.blend[1] * alpha * 255;
    premult[2] = r_newrefdef.blend[2] * alpha * 255;

    one_minus_alpha = (1.0 - alpha);

    in = (byte *)d_8to24table;
    out = palette;
    for (i = 0; i < 256; i++, in += 4, out += 4) {
        r = premult[0] + one_minus_alpha * in[0];
        g = premult[1] + one_minus_alpha * in[1];
        b = premult[2] + one_minus_alpha * in[2];
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        out[0] = r;
        out[1] = g;
        out[2] = b;
        out[3] = 255;
    }

    R_GammaCorrectAndSetPalette(palette);
}

byte *IMG_ReadPixels(byte **palette, int *width, int *height, int *rowbytes)
{
    *palette = sw_state.currentpalette;
    *width = vid.width;
    *height = vid.height;
    *rowbytes = vid.rowbytes;
    return vid.buffer;
}

//=======================================================================

/*
@@@@@@@@@@@@@@@@
R_RenderFrame

@@@@@@@@@@@@@@@@
*/
void R_RenderFrame(refdef_t *fd)
{
    r_newrefdef = *fd;

    if (!r_worldmodel && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
        Com_Error(ERR_FATAL, "R_RenderView: NULL worldmodel");

    VectorCopy(fd->vieworg, r_refdef.vieworg);
    VectorCopy(fd->viewangles, r_refdef.viewangles);

    if (r_speeds->integer || r_dspeeds->integer)
        r_time1 = Sys_Milliseconds();

    R_SetupFrame();

    R_MarkLeaves();     // done here so we know if we're in water

    R_PushDlights(r_worldmodel->nodes);

    R_EdgeDrawing();

    if (r_dspeeds->integer) {
        se_time2 = Sys_Milliseconds();
        de_time1 = se_time2;
    }

    R_DrawEntitiesOnList();

    if (r_dspeeds->integer) {
        de_time2 = Sys_Milliseconds();
        dp_time1 = Sys_Milliseconds();
    }

    R_DrawParticles();

    if (r_dspeeds->integer)
        dp_time2 = Sys_Milliseconds();

    R_DrawAlphaSurfaces();

    //Start Replaced by Lewey
    if (sw_drawsird->integer && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL)) {
        R_ApplySIRDAlgorithum();
    } else {
        //don't do warp if we are doing SIRD because the warp
        //would make the SIRD impossible to see.
        if (r_dowarp)
            D_WarpScreen();
    }
    //End Replaced by Lewey

    if (r_dspeeds->integer)
        da_time1 = Sys_Milliseconds();

    if (r_dspeeds->integer)
        da_time2 = Sys_Milliseconds();

    R_CalcPalette();

    if (sw_aliasstats->integer)
        R_PrintAliasStats();

    if (r_speeds->integer)
        R_PrintTimes();

    if (r_dspeeds->integer)
        R_PrintDSpeeds();

    if (sw_reportsurfout->integer && r_outofsurfaces)
        Com_Printf("Short %d surfaces\n", r_outofsurfaces);

    if (sw_reportedgeout->integer && r_outofedges)
        Com_Printf("Short roughly %d edges\n", r_outofedges * 2 / 3);
}

/*
** R_BeginFrame
*/
void R_BeginFrame(void)
{
    VID_BeginFrame();
}

void R_EndFrame(void)
{
    if (vid_gamma->modified) {
        R_BuildGammaTable();
        R_GammaCorrectAndSetPalette((const byte *) d_8to24table);
        vid_gamma->modified = qfalse;
    }

    VID_EndFrame();
}

/*
** R_GammaCorrectAndSetPalette
*/
void R_GammaCorrectAndSetPalette(const byte *palette)
{
    int i;
    byte *dest;

    dest = sw_state.currentpalette;
    for (i = 0; i < 256; i++) {
        dest[0] = sw_state.gammatable[palette[0]];
        dest[1] = sw_state.gammatable[palette[1]];
        dest[2] = sw_state.gammatable[palette[2]];
        palette += 4; dest += 4;
    }

    VID_UpdatePalette(sw_state.currentpalette);
}

#if 0
/*
** R_CinematicSetPalette
*/
void R_CinematicSetPalette(const byte *palette)
{
    byte palette32[1024];
    int     i, j, w;
    int     *d;

    // clear screen to black to avoid any palette flash
    w = abs(vid.rowbytes) >> 2; // stupid negative pitch win32 stuff...
    for (i = 0; i < vid.height; i++, d += w) {
        d = (int *)(vid.buffer + i * vid.rowbytes);
        for (j = 0; j < w; j++)
            d[j] = 0;
    }
    // flush it to the screen
    R_EndFrame();

    if (palette) {
        for (i = 0; i < 256; i++) {
            palette32[i * 4 + 0] = palette[i * 3 + 0];
            palette32[i * 4 + 1] = palette[i * 3 + 1];
            palette32[i * 4 + 2] = palette[i * 3 + 2];
            palette32[i * 4 + 3] = 0xFF;
        }

        R_GammaCorrectAndSetPalette(palette32);
    } else {
        R_GammaCorrectAndSetPalette((const byte *) d_8to24table);
    }
}
#endif

/*
** R_DrawBeam
*/
void R_DrawBeam(entity_t *e)
{
#define NUM_BEAM_SEGS 6

    int i;

    vec3_t perpvec;
    vec3_t direction, normalized_direction;
    vec3_t start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
    vec3_t oldorigin, origin;

    oldorigin[0] = e->oldorigin[0];
    oldorigin[1] = e->oldorigin[1];
    oldorigin[2] = e->oldorigin[2];

    origin[0] = e->origin[0];
    origin[1] = e->origin[1];
    origin[2] = e->origin[2];

    normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
    normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
    normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

    if (VectorNormalize(normalized_direction) == 0)
        return;

    PerpendicularVector(perpvec, normalized_direction);
    VectorScale(perpvec, e->frame / 2, perpvec);

    for (i = 0; i < NUM_BEAM_SEGS; i++) {
        R_RotatePointAroundVector(start_points[i], normalized_direction,
                                  perpvec, (360.0 / NUM_BEAM_SEGS)*i);
        VectorAdd(start_points[i], origin, start_points[i]);
        VectorAdd(start_points[i], direction, end_points[i]);
    }

    for (i = 0; i < NUM_BEAM_SEGS; i++) {
        R_IMFlatShadedQuad(start_points[i],
                           end_points[i],
                           end_points[(i + 1) % NUM_BEAM_SEGS],
                           start_points[(i + 1) % NUM_BEAM_SEGS],
                           e->skinnum & 0xFF,
                           e->alpha);
    }
}
