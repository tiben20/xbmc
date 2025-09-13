/*-
 * Copyright (c) 2005 Boris Mikhaylov
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* libsndfile is copyright by Erik de Castro Lopo.
 * http://www.mega-nerd.com/libsndfile/
 */
#include <sndfile.h>

#include "bs2b.h"

#define	 BUFFER_LEN	1024 /* must be multiply of 2 */

static void copy_metadata( SNDFILE *outfile, SNDFILE *infile );
static void copy_data( SNDFILE *outfile, SNDFILE *infile, t_bs2bdp bs2bdp );

static void print_usage( char *progname )
{
	printf( "\n"
		"Bauer stereophonic-to-binaural DSP converter. Version %s\n\n",
		BS2B_VERSION_STR );
	printf(
		"Usage : %s [-l L|(L1 L2)] <input file> <output file>\n",
		progname );
	printf(
		"-h - this help.\n"
		"-l - crossfeed level, L = d|c|m:\n"
		"     d - default preset     - 700Hz/260us, 4.5 dB;\n"
		"     c - Chu Moy's preset   - 700Hz/260us, 6.0 dB;\n"
		"     m - Jan Meier's preset - 650Hz/280us, 9.5 dB.\n"
		"     Or L1 = [%d..%d] mB of feed level (%d..%d dB)\n"
		"     and L2 = [%d..%d] Hz of cut frequency.\n",
		BS2B_MINFEED, BS2B_MAXFEED, BS2B_MINFEED / 10, BS2B_MAXFEED / 10,
		BS2B_MINFCUT, BS2B_MAXFCUT );
} /* print_usage() */

int main( int argc, char *argv[] )
{
	char     *progname, *infilename, *outfilename, *tmpstr;
	SNDFILE  *infile, *outfile;
	SF_INFO  sfinfo;
	t_bs2bdp bs2bdp;
	uint32_t srate = BS2B_DEFAULT_SRATE;
	uint32_t level = BS2B_DEFAULT_CLEVEL;
	int i;

	tmpstr = strrchr( argv[ 0 ], '/' );
	tmpstr = tmpstr ? tmpstr + 1 : argv[ 0 ];
	progname = strrchr( tmpstr, '\\' );
	progname = progname ? progname + 1 : tmpstr;

	if( argc >= 3 )
	{
		outfilename = argv[ --argc ];
		infilename  = argv[ --argc ];
	}
	else
	{
		print_usage( progname );
		return 1;
	}

	for( i = 1; i < argc; i++ )
	{
		if( '-' == argv[ i ][ 0 ] )
		{
			switch( argv[ i ][ 1 ] )
			{
			case 'h':
				print_usage( progname );
				return 1;

			case 'l':
				if( ++i >= argc )
				{
					print_usage( progname );
					return 1;
				}
				switch( argv[ i ][ 0 ] )
				{
				case 'd':
					level = BS2B_DEFAULT_CLEVEL;
					break;
				case 'c':
					level = BS2B_CMOY_CLEVEL;
					break;
				case 'm':
					level = BS2B_JMEIER_CLEVEL;
					break;
				default:
					{
						int feed, fcut;

						feed = atoi( argv[ i ] );
						if( ++i >= argc )
						{
							print_usage( progname );
							return 1;
						}
						fcut = atoi( argv[ i ] );
						if( feed < BS2B_MINFEED || feed > BS2B_MAXFEED ||
							fcut < BS2B_MINFCUT || fcut > BS2B_MAXFCUT )
						{
							print_usage( progname );
							return 1;
						}
						level = ( ( uint32_t )feed << 16 ) | ( uint32_t )fcut;
					}
				} /* switch */
				break;

			default:
				print_usage( progname );
				return 1;
			} /* swith */
		}
		else
		{
			print_usage( progname );
			return 1;
		}
	} /* for */

	if( strcmp( infilename, outfilename ) == 0 )
	{
		printf( "Error : Input and output filenames are the same.\n\n" );
		return 1;
	}
	
	if( ( infile = sf_open( infilename, SFM_READ, &sfinfo ) ) == NULL )
	{
		printf( "Not able to open input file %s.\n", infilename );
		printf( "%s",sf_strerror( NULL ) );
		return 1;
	}

	if( sfinfo.channels != 2 )
	{
		printf( "Input file is not a stereo.\n" );
		sf_close( infile );
		return 1;
	}

	srate = sfinfo.samplerate;

	if( srate < BS2B_MINSRATE || srate > BS2B_MAXSRATE )
	{
		printf( "Not supported sample rate '%d'.\n", srate );
		sf_close( infile );
		return 1;
	}

	/* Open the output file. */
	if( ( outfile = sf_open( outfilename, SFM_WRITE, &sfinfo ) ) == NULL )
	{
		printf( "Not able to open output file %s : %s\n",
			outfilename, sf_strerror( NULL ) );
		sf_close( infile );
		return 1;
	}

	if( NULL == ( bs2bdp = bs2b_open() ) )
	{
		printf( "Not able to allocate data\n" );
		sf_close( infile );
		sf_close( outfile );
		return 1;
	}

	bs2b_set_srate( bs2bdp, srate );
	bs2b_set_level( bs2bdp, level );

	printf( "Crossfeed level: %.1f dB, %d Hz, %d us.\n",
		( double )bs2b_get_level_feed( bs2bdp ) / 10.0,
		bs2b_get_level_fcut( bs2bdp ), bs2b_get_level_delay( bs2bdp ) );
	printf( "Converting file '%s' to file '%s'\nsample rate = %u...",
		infilename, outfilename, bs2b_get_srate( bs2bdp ) );

	copy_metadata( outfile, infile );

	copy_data( outfile, infile, bs2bdp );

	bs2b_close( bs2bdp );
	bs2bdp = 0;

	sf_close( infile );
	sf_close( outfile );

	printf( " Done.\n" );

	return 0;
} /* main() */

static void copy_metadata( SNDFILE *outfile, SNDFILE *infile )
{
	const char *str;
	int k, err = 0;

	for( k = SF_STR_FIRST; k <= SF_STR_LAST; k++ )
	{
		str = sf_get_string( infile, k );
		if( str != NULL )
			err = sf_set_string( outfile, k, str );
	}
	
	err = sf_set_string( outfile, SF_STR_COMMENT, "CROSSFEEDED" );
} /* copy_metadata() */

static void copy_data( SNDFILE *outfile, SNDFILE *infile, t_bs2bdp bs2bdp )
{
	static double data[ BUFFER_LEN ];
	int readcount;

	for( ;; )
	{
		readcount = ( int )sf_read_double( infile, data, BUFFER_LEN );
		if( readcount < 2 ) break;
		bs2b_cross_feed_d( bs2bdp, data, readcount / 2 );
		sf_write_double( outfile, data, readcount );
	}
} /* copy_data() */
