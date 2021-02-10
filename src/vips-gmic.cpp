/* Pass VIPS images through gmic
 *
 * AF, 6/10/14
 * 23/8/19
 *      - revise for vips8.8
 * 9/2/21 kleisauke
 *      - revise for CMake
 */

/*

    This file is part of VIPS.
    
    VIPS is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA

 */

/*

    These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

#define _(S) (S)

#include <vips/vips.h>

#include <limits.h>

#include <iostream>

#include "CImg.h"
#include <gmic.h>

using namespace cimg_library;

typedef struct _VipsGMic {
	VipsOperation parent_instance;

	VipsArrayImage *in;
	VipsImage *out;
	char *command;
	int padding;
	double x_scale;
	double y_scale;
} VipsGMic;

typedef VipsOperationClass VipsGMicClass;

#define VIPS_TYPE_GMIC (vips_gmic_get_type())
#define VIPS_GMIC( obj ) \
	(G_TYPE_CHECK_INSTANCE_CAST( (obj), VIPS_TYPE_GMIC, VipsGMic ))
#define VIPS_GMIC_CLASS( klass ) \
	(G_TYPE_CHECK_CLASS_CAST( (klass), VIPS_TYPE_GMIC, VipsGMicClass))
#define VIPS_IS_GMIC( obj ) \
	(G_TYPE_CHECK_INSTANCE_TYPE( (obj), VIPS_TYPE_GMIC ))
#define VIPS_IS_GMIC_CLASS( klass ) \
	(G_TYPE_CHECK_CLASS_TYPE( (klass), VIPS_TYPE_GMIC ))
#define VIPS_GMIC_GET_CLASS( obj ) \
	(G_TYPE_INSTANCE_GET_CLASS( (obj), VIPS_TYPE_GMIC, VipsGMicClass ))

// We need C linkage for this.
extern "C" {
	G_DEFINE_TYPE( VipsGMic, vips_gmic, VIPS_TYPE_OPERATION );
}

static int 
vips_gmic_get_tile_border( VipsGMic *vipsgmic )
{
	return( vipsgmic->padding );
}

#define INDEX( IMG, X, Y, Z ) \
	((*(IMG))( (guint) (X), (guint) (Y), (guint) (Z), 0 ))

// copy part of a vips region into a cimg
static void
vips_to_gmic( VipsRegion *in, VipsRect *area, gmic_image<float> *img )
{
	VipsImage *im = in->im;

	for( int y = 0; y < area->height; y++ ) {
		float *p = (float *) 
			VIPS_REGION_ADDR( in, area->left, area->top + y );

		for( int x = 0; x < area->width; x++ ) {
			for( int z = 0; z < im->Bands; z++ ) 
				INDEX( img, x, y, z ) = p[z];

			p += im->Bands;
		}
	}
}

// write a CImg to a vips region
// fill out->valid, img has pixels in img_rect
static void
vips_from_gmic( gmic_image<float> *img, VipsRect *img_rect, VipsRegion *out )
{
	VipsImage *im = out->im;
	VipsRect *valid = &out->valid;

	g_assert( vips_rect_includesrect( img_rect, valid ) );

	int x_off = valid->left - img_rect->left;
	int y_off = valid->top - img_rect->top;

	for( int y = 0; y < valid->height; y++ ) {
		float *p = (float *) 
			   VIPS_REGION_ADDR( out, valid->left, valid->top + y );

		for( int x = 0; x < valid->width; x++ ) {
			for( int z = 0; z < im->Bands; z++ )
				p[z] = INDEX( img, x + x_off, y + y_off, z );

			p += im->Bands;
		}
	}
}

/* One of these for each thread.
 */
struct VipsGMicSequence { 
	VipsRegion **ir;
	gmic *gmic_instance;
};

static int
vips_gmic_stop( void *vseq, void *a, void *b )
{
	VipsGMicSequence *seq = (VipsGMicSequence *) vseq;

        if( seq->ir ) {
		int i;

		for( i = 0; seq->ir[i]; i++ )
			g_object_unref( seq->ir[i] );
		VIPS_FREE( seq->ir );
	}

	delete seq->gmic_instance;

	VIPS_FREE( seq );

	return( 0 );
}

static void *
vips_gmic_start( VipsImage *out, void *a, void *b )
{
	VipsImage **in = (VipsImage **) a;

	VipsGMicSequence *seq;
	int i, n;

	if( !(seq = VIPS_NEW( NULL, VipsGMicSequence )) )
		return( NULL ); 

	/* Make a region for each input image. 
	 */
	for( n = 0; in[n]; n++ )
		;

	if( !(seq->ir = VIPS_ARRAY( NULL, n + 1, VipsRegion * )) ) {
		vips_gmic_stop( seq, NULL, NULL );
		return( NULL );
	}

	for( i = 0; i < n; i++ )
		if( !(seq->ir[i] = vips_region_new( in[i] )) ) {
			vips_gmic_stop( seq, NULL, NULL );
			return( NULL );
		}
	seq->ir[n] = NULL;

	/* Make a gmic for this thread.
	 */
	seq->gmic_instance = new gmic; 

	return( (void *) seq );
}

static int
vips_gmic_gen( VipsRegion *oreg, void *vseq, void *a, void *b, gboolean *stop )
{
	VipsGMicSequence *seq = (VipsGMicSequence *) vseq;
	VipsGMic *vipsgmic = (VipsGMic *) b;
	int ninput = VIPS_AREA( vipsgmic->in )->n;
	const int tile_border = vips_gmic_get_tile_border( vipsgmic );
	const VipsRect *r = &oreg->valid;

	VipsRect need;
	VipsRect image;

	need = *r;
	vips_rect_marginadjust( &need, tile_border );
	image.left = 0;
	image.top = 0;
	image.width = seq->ir[0]->im->Xsize;
	image.height = seq->ir[0]->im->Ysize;
	vips_rect_intersectrect( &need, &image, &need );

	for( int i = 0; seq->ir[i]; i++ ) 
		if( vips_region_prepare( seq->ir[i], &need ) ) 
			return( -1 );

	gmic_list<float> images;
	gmic_list<char> images_names;

	try {
		images.assign( (guint) ninput );

		for( int i = 0; seq->ir[i]; i++ ) {
			gmic_image<float> &img = images[i];
			img.assign( need.width, need.height, 
				1, seq->ir[i]->im->Bands );
			vips_to_gmic( seq->ir[0], &need, &img );
		}

		seq->gmic_instance->run( vipsgmic->command, 
			images, images_names );
		vips_from_gmic( &images[0], &need, oreg );
	}
	catch( gmic_exception &e ) { 
		images.assign( (guint) 0 );

		vips_error( "VipsGMic", "%s", e.what() );

		return( -1 );
	}
	images.assign( (guint) 0 );

	return( 0 );
}

static int 
vips_gmic_build( VipsObject *object )
{
	VipsObjectClass *klass = VIPS_OBJECT_GET_CLASS( object );
	VipsGMic *vipsgmic = (VipsGMic *) object;

	VipsImage **in;
	VipsImage **t;
	int ninput;

	if( VIPS_OBJECT_CLASS( vips_gmic_parent_class )->build( object ) )
		return( -1 );

	in = vips_array_image_get( vipsgmic->in, &ninput );

	for( int i = 0; i < ninput; i++ ) 
		if( vips_image_pio_input( in[i] ) || 
			vips_check_coding_known( klass->nickname, in[i] ) )  
			return( -1 );

	/* Cast all inputs up to float. 
	 */
	t = (VipsImage **) vips_object_local_array( object, ninput );
	for( int i = 0; i < ninput; i++ )
		if( vips_cast( in[i], &t[i], VIPS_FORMAT_FLOAT, NULL ) )
			return( -1 );
	in = t;

	g_object_set( vipsgmic, "out", vips_image_new(), NULL ); 

	if( vips_image_pipeline_array( vipsgmic->out, 
		VIPS_DEMAND_STYLE_SMALLTILE, in ) )
		return( -1 );

	if( vips_image_generate( vipsgmic->out,
		vips_gmic_start, vips_gmic_gen, vips_gmic_stop, 
		in, vipsgmic ) )
		return( -1 );

	return( 0 );
}

static void
vips_gmic_class_init( VipsGMicClass *klass )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( klass );
	VipsObjectClass *vobject_class = VIPS_OBJECT_CLASS( klass );
	VipsOperationClass *operation_class = VIPS_OPERATION_CLASS( klass );

	gobject_class->set_property = vips_object_set_property;
	gobject_class->get_property = vips_object_get_property;

	vobject_class->nickname = "gmic";
	vobject_class->description = _( "Vips G'MIC" );
	vobject_class->build = vips_gmic_build;

	operation_class->flags = VIPS_OPERATION_SEQUENTIAL_UNBUFFERED;

	VIPS_ARG_BOXED( klass, "in", 0, 
		_( "Input" ), 
		_( "Array of input images" ),
		VIPS_ARGUMENT_REQUIRED_INPUT,
		G_STRUCT_OFFSET( VipsGMic, in ),
		VIPS_TYPE_ARRAY_IMAGE );

	VIPS_ARG_IMAGE( klass, "out", 1,
		_( "Output" ), 
		_( "Output image" ),
		VIPS_ARGUMENT_REQUIRED_OUTPUT, 
		G_STRUCT_OFFSET( VipsGMic, out ) );

	VIPS_ARG_INT( klass, "padding", 3,
		_( "padding" ), 
		_( "Tile overlap" ),
		VIPS_ARGUMENT_REQUIRED_INPUT, 
		G_STRUCT_OFFSET( VipsGMic, padding ),
		0, INT_MAX, 0);

	VIPS_ARG_DOUBLE( klass, "x_scale", 4,
		_( "x_scale" ), 
		_( "X Scale" ),
		VIPS_ARGUMENT_REQUIRED_INPUT, 
		G_STRUCT_OFFSET( VipsGMic, x_scale ),
		0, 100000000, 1);

	VIPS_ARG_DOUBLE( klass, "y_scale", 5,
		_( "y_scale" ), 
		_( "Y Scale" ),
		VIPS_ARGUMENT_REQUIRED_INPUT, 
		G_STRUCT_OFFSET( VipsGMic, y_scale ),
		0, 100000000, 1);

	VIPS_ARG_STRING( klass, "command", 10, 
		_( "command" ),
		_( "G'MIC command string" ),
		VIPS_ARGUMENT_REQUIRED_INPUT, 
		G_STRUCT_OFFSET( VipsGMic, command ),
		NULL );
}

static void
vips_gmic_init( VipsGMic *vipsgmic )
{
}

G_BEGIN_DECLS

/**
 * vips_gmic:
 * @in: (array length=n) (transfer none): array of input images
 * @out: output image
 * @n: number of input images
 * @padding: overlap tiles by this much
 * @x_scale: 
 * @y_scale: 
 * @command: command to execute
 *
 * Returns: 0 on success, -1 on failure. 
 */
G_MODULE_EXPORT int
vips_gmic( VipsImage **in, VipsImage **out, int n, 
	int padding, double x_scale, double y_scale, const char *command, ... )
{
	VipsArrayImage *array; 
	va_list ap;
	int result;

	array = vips_array_image_new( in, n ); 
	va_start( ap, command );
	result = vips_call_split( "gmic", ap, array, out, 
		padding, x_scale, y_scale, command );
	va_end( ap );
	vips_area_unref( VIPS_AREA( array ) );

	return( result );
}

G_END_DECLS

/* This is called on module load.
 */
extern "C" const gchar *
g_module_check_init( GModule *module )
{
#ifdef DEBUG
	printf( "vips_gmic: module init\n" ); 
#endif /*DEBUG*/

	vips_gmic_get_type();

	/* We can't be unloaded, there would be chaos.
	 */
	g_module_make_resident( module );

	return( NULL ); 
}
