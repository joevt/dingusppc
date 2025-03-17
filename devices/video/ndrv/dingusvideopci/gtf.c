/*
	Joe van Tunen
	Nov 4, 2005
*/

#include "gtf.h"

/*
 *  GTF calculations and comments are taken from GTF_V1R1.xls created by ANDY.MORRISH@NSC.COM
*/

#include <string.h>
#include <math.h>
#include <stdio.h>


#define ROUND(x,n) round(x*(1/(n==0))) /* this should give a warning if n is not 0 */
#define IF(x,y,z) ((x)?(y):(z))
#define SQRT(x) sqrt(x)


#define CASE(x,y,z,w) (IF(gtf->timing_constraint_choice==1,(x),IF(gtf->timing_constraint_choice==2,(y),IF(gtf->timing_constraint_choice==3,(z),(w)))))
inline double SQR(double x) { return x * x; }

#ifndef gtf_verbose
	#define gtf_verbose 0
#else
	#define gtf_verbose 1
#endif

#if gtf_verbose
	#define gtfout printf
	#define STROF(x,y)	12,(y)*((x)!=0),0,""
#else
	#define gtfout
	#define STROF(x,y)	y
#endif


/*
			THE VESA GENERALIZED TIMING FORMULA (GTF)
	GTF SPREADSHEET BY ANDY MORRISH 1/5/97
	Comments, bugs, Call Andy Morrish at 408 721-2268 or fax (408) 721-7321 or EMAIL ANDY.MORRISH@NSC.COM
		REVISION HISTORY:
			V1 REV 0.5:
				Character width correted to ns						Default values added to default parameter list
			V1 REV 0.6:
				Vertical blank unit corrected to ms
			V1 REV 0.7:
				Layout reformatted for easier formula printing		Hz/kHz/MHz unit added after input parameter
			V1REV0.99:
				Error message added when input parameters cause an error in the output
			V1REV1.0
				Final release version
*/



static void gtf_from_vertical_rate( gtf_t* gtf );
static void gtf_from_horizontal_rate( gtf_t* gtf );
static void gtf_from_pixel_rate( gtf_t* gtf );
static void gtf_derived_parameters( gtf_t* gtf );


void gtf_init( gtf_t* gtf )
{
	memset( gtf, 0, sizeof( *gtf ));

	gtf->h_pixels = 640;
	gtf->v_pixels = 480;
	gtf->want_margins = 0;
	gtf->want_interlaced = 0;

	gtf->timing_constraint_choice = timing_from_vertical_refresh_rate;
	gtf->timing_constraint = 75;

	gtf->margin_percent_of_height = 1.8;

	gtf->character_cell_pixels = 8;
	gtf->minimum_porch_lines = 1;

	gtf->v_sync_lines = 3;
	gtf->sync_percent_of_line_time = 8;

	gtf->minimum_v_sync_and_back_porch_for_flyback_us = 550;

	gtf->M_gradient_percent_per_kHz = 600;
	gtf->C_offset_percent = 40;
	gtf->K_blank_time_scaling_factor = 128;
	gtf->J_scaling_factor_weighting = 20;
}



int gtf_main( gtf_t* gtf )
{
		gtfout( "%s\n",				"Enter the desired PIXEL format:" );
		gtfout( "%s\n",				"		Note:" );
		gtfout( "%s\n",				"		Value will be rounded to nearest integer no of lines and character cells" );
		gtfout( "\n" );
		gtfout( "%-60s %10d\n",		"ENTER HORIZONTAL PIXELS HERE =>",					gtf->h_pixels );
		gtfout( "%-60s %10d\n",		"ENTER VERTICAL PIXELS HERE =>",					gtf->v_pixels );
		gtfout( "%-60s %10s\n",		"ENTER IF YOU WANT MARGINS HERE (Y or N) =>",		IF(gtf->want_margins,"y","n") );
		gtfout( "%-60s %10s\n",		"ENTER IF YOU WANT INTERLACE HERE (Y or N) =>",		IF(gtf->want_interlaced,"y","n") );
		gtfout( "\n" );
		gtfout( "\n" );
		gtfout( "%s\n",				"Timings can be defined based on any one of the following" );
		gtfout( "\n" );
		gtfout( "%s\n",				"	1) VERTICAL RETRACE FREQUENCY REQUIRED (in Hz)" );
		gtfout( "%s\n",				"	2) HORIZONTAL FREQUENCY REQUIRED (in kHz)" );
		gtfout( "%s\n",				"	3) PIXEL CLOCK REQUIRED (in MHz)" );
		gtfout( "\n" );
		gtfout( "%s\n",				"ENTER YOUR CHOICE (1,2 or 3) HERE =>" );
		gtfout( "%-60s %10d\n",		"Now enter chosen timing constraint:",				gtf->timing_constraint_choice );
		gtfout( "\n" );
		gtfout( "%-60s %10.3f %s\n", (CASE("ENTER VERTICAL SCAN FRAME RATE HERE=>.:","ENTER HORIZ SCAN FREQ. HERE=>","ENTER PIXEL CLOCK FREQ. HERE=>","INCORRECT CHOICE ENTERED")),	gtf->timing_constraint,	(CASE("Hz","kHz","MHz","")) );
		gtfout( "\n" );
		gtfout( "\n" );
		gtfout( "%s\n",				"			DEFAULT PARAMETER VALUES" );
		gtfout( "\n" );
		gtfout( "%s\n",				"1) These are the default values that define the MARGIN size:" );
		gtfout( "%s\n",				"Note:" );
		gtfout( "%s\n",				"	Only ratio of MARGIN to image is important. Top and Bottom MARGINs are equal" );
		gtfout( "%s\n",				"	Side MARGINs are proportional to the ratio of image H/V pixels" );
		gtfout( "%-90s %10.3f\n",	"	GIVE:	Top/ bottom MARGIN size as % of height (%)={DEFAULT = 1.8}",		gtf->margin_percent_of_height );
		gtfout( "\n" );
		gtfout( "\n" );
		gtfout( "%s\n",				"2) This default value defines the horizontal timing boundaries:" );
		gtfout( "%-90s %10d\n",		"	GIVE:	Character cell horizontal granularity (pixels)= {DEFAULT = 8}",		gtf->character_cell_pixels );
		gtfout( "%-90s %10d\n",		"	GIVE:	Minimum porch (no of lines) = {DEFAULT = 1}",						gtf->minimum_porch_lines );
		gtfout( "\n" );
		gtfout( "%s\n",				"3) These default values define analog system sync pulse width limitations:" );
		gtfout( "%s\n",				"Note:" );
		gtfout( "%s\n",				"	Vertical sync width (in lines) will be rounded down to nearest integer" );
		gtfout( "%s\n",				"	Horizontal sync width will be rounded to nearest char cell boundary" );
		gtfout( "%-90s %10d\n",		"	GIVE:	Number of lines for vertical sync (lines)={DEFAULT = 3}",			gtf->v_sync_lines );
		gtfout( "%-90s %10.3f\n",	"	GIVE:	Nominal H sync width (% of line period) ={DEFAULT = 8}",			gtf->sync_percent_of_line_time );
		gtfout( "\n" );
		gtfout( "\n" );
		gtfout( "%s\n",				"4) These default values define analog scan system vertical blanking time limitations" );
		gtfout( "%s\n",				"Note:" );
		gtfout( "%s\n",				"	Vertical blanking time will rounded to nearest integer number of lines" );
		gtfout( "%-90s %10.3f\n",	"	GIVE:	Minimum time of vertical sync+back porch interval (us)=",			gtf->minimum_v_sync_and_back_porch_for_flyback_us );
		gtfout( "%s\n",				"						{DEFAULT = 550}" );
		gtfout( "\n" );
		gtfout( "%s\n",				"5) Definition of Horizontal blanking time limitation:" );
		gtfout( "%s\n",				"Note:" );
		gtfout( "%s\n",				"	Generalized blanking limitation formula used of the form:" );
		gtfout( "%s\n",				"	<H BLANKING TIME (%)> =C - ( M / Fh)" );
		gtfout( "%s\n",				"	Where:" );
		gtfout( "%-90s %10.3f\n",	"	GIVE:	M (gradient) (%/kHz) ={DEFAULT = 600}",								gtf->M_gradient_percent_per_kHz );
		gtfout( "%-90s %10.3f\n",	"	GIVE:	C (offset) (%) = {DEFAULT = 40}",									gtf->C_offset_percent );
		gtfout( "%-90s %10.3f\n",	"	GIVE:	K (blanking time scaling factor) = {DEFAULT = 128}",				gtf->K_blank_time_scaling_factor );
		gtfout( "%-90s %10.3f\n",	"	GIVE:	J (scaling factor weighting) = {DEFAULT = 20}",						gtf->J_scaling_factor_weighting );
		gtfout( "\n" );
		gtfout( "\n" );
		gtfout( "\n" );
		gtfout( "\n" );
		gtfout( "%s\n",				"		WORK AREA:" );
		gtfout( "%s\n",				"	ROUNDED VARIABLES" );
		gtfout( "\n" );
		gtfout( "%10d %s\n",	gtf->character_cell_pixels_rounded			= ((ROUND(gtf->character_cell_pixels,0))),																									"- THIS IS THE CHARACTER CELL GRANULARITY ROUNDED TO NEAREST INTEGER NUMBER PIXELS" );
		gtfout( "%10d %s\n",	gtf->frontporch_lines_rounded				= ((ROUND(gtf->minimum_porch_lines,0))),																									"- THIS IS THE FRONT PORCH ROUNDED TO NEAREST INTEGER NUMBER LINES" );
		gtfout( "%10d %s\n",	gtf->sync_lines_rounded						= ((ROUND(gtf->v_sync_lines,0))),																											"- THIS IS THE V SYNC ROUNDED TO NEAREST INTEGER NUMBER LINES" );
		gtfout( "%10.3f\n",		gtf->K_blank_time_scaling_factor_adjusted	= IF(gtf->K_blank_time_scaling_factor==0.0,0.001,gtf->K_blank_time_scaling_factor) );
		gtfout( "%10.3f %s\n",	gtf->blank_equation_M						= gtf->K_blank_time_scaling_factor_adjusted/256*gtf->M_gradient_percent_per_kHz,															"- THIS CREATES THE ACTUAL 'M' USED BY THE BLANKING EQUATION, INCORPORATING THE SCALING FACTOR" );
		gtfout( "%10.3f %s\n",	gtf->blank_equation_C						= ((gtf->C_offset_percent-gtf->J_scaling_factor_weighting)*gtf->K_blank_time_scaling_factor_adjusted/256)+gtf->J_scaling_factor_weighting,	"- THIS CREATES THE ACTUAL 'C' USED BY THE BLANKING EQUATION, INCORPORATING THE SCALING FACTOR" );
		gtfout( "%10d %s\n",	gtf->pixels_rounded_to_character			= ((ROUND(gtf->h_pixels/gtf->character_cell_pixels_rounded,0))*gtf->character_cell_pixels_rounded),											"- THIS IS THE NUMBER OF PIXELS ROUNDED TO NEAREST INTEGER NUMBER OF CHARACTERS" );
		gtfout( "%10d %s\n",	gtf->lines_rounded							= (IF(gtf->want_interlaced,ROUND(gtf->v_pixels/2,0),ROUND(gtf->v_pixels,0))),																"- THIS IS THE NUMBER OF LINES PER FIELD, ROUNDED TO NEAREST INTEGER NUMBER OF LINES" );
		gtfout( "\n" );
		gtfout( "\n" );

		switch ( gtf->timing_constraint_choice )
		{
			case timing_from_vertical_refresh_rate:
				gtf_from_vertical_rate( gtf );
				break;
			case timing_from_horizontal_scan_rate:
				gtf_from_horizontal_rate( gtf );
				break;
			case timing_from_pixel_clock:
				gtf_from_pixel_rate( gtf );
				break;
		}
		gtfout( "\n" );
		gtfout( "\n" );
		
		gtf_derived_parameters( gtf );
		gtfout( "\n" );
		gtfout( "\n" );

		gtfout( "%s\n",																"OUTPUTS:" );
		gtfout( "\n" );
		gtfout( "%-40s %12s %-7s %12s %-7s %12s %-7s %12s %-7s\n",					"", 								"TIME",												"UNIT",		"TIME2",																"UNIT2",								"PIXEL COUNT",								"UNIT",			"PIXEL COUNT2",															"UNIT2" );
		gtfout( "%-40s %12d %-7s\n",												"HOR PIXELS",						gtf->pixels_rounded_to_character,					"PIXELS" );
		gtfout( "%-40s %12s %-7s %12s\n",											"",									(IF(gtf->want_interlaced,"PER FRAME","")),			"",			(IF(gtf->want_interlaced,"PER FIELD","")) );
		gtfout( "%-40s %12d %-7s %*d%*s %-7s\n",									"VER PIXELS",						gtf->v_pixels,										"LINES",	STROF(gtf->want_interlaced,gtf->v_pixels/2),							(IF(gtf->want_interlaced,"LINES","")) );
		gtfout( "\n" );
		gtfout( "%-40s %12.3f %-7s\n",												"HOR FREQUENCY",					gtf->h_freq_kHz,									"kHz" );
		gtfout( "%-40s %12s %-7s %12s\n",											"",									(IF(gtf->want_interlaced,"FRAME RATE","")),			"",			(IF(gtf->want_interlaced,"FIELD RATE","")) );
		gtfout( "%-40s %12.3f %-7s %*.3f%*s %-7s\n",								"VER FREQUENCY",					gtf->actual_v_frame_frequency_Hz,					"Hz",		STROF(gtf->want_interlaced,gtf->v_field_rate_Hz),						(IF(gtf->want_interlaced,"Hz","")) );
		gtfout( "\n" );
		gtfout( "%-40s %12.3f %-7s %-12s %-7s %12d %-7s\n",							"PIXEL CLOCK",						gtf->pixel_freq_MHz,								"MHz",		"",																		"",										1,											"PIXELS" );
		gtfout( "%-40s %12.3f %-7s %-12s %-7s %12d %-7s\n",							"CHARACTER WIDTH",					gtf->char_time_ns,									"ns",		"",																		"",										gtf->character_cell_pixels_rounded,			"PIXELS" );
		gtfout( "\n" );
		gtfout( "%-40s %-12s\n",													"SCAN TYPE",						(IF(gtf->want_interlaced,"INTERLACED", "NON-INT")) );
		gtfout( "%s\n",																gtf->input_parameter_error = IF((( /*OR*/ (
																gtf->h_total_time_us<0 ||
																gtf->ideal_duty_cycle_percent<0 ||
																gtf->h_addr_time_us<0 ||
																gtf->h_blank_time_us<0 ||
																gtf->h_blank_and_margin_time_us<0 ||
																gtf->actual_h_blank_duty_cycle_percent<0 ||
																gtf->act_h_blank_and_margin_duty_cycle_percent<0 ||
																gtf->h_left_margin_us<0 ||
																gtf->h_front_porch_to_nearest_char_cell_us<0 ||
																gtf->h_sync_width_to_nearest_char_cell_us<0 ||
																gtf->h_back_porch_to_nearest_char_cell_us<0 ||
																gtf->h_right_margin_us<0 ||
																gtf->v_frame_period_ms<0 ||
																gtf->v_addr_time_per_frame_ms<0 ||
																gtf->odd_field_total_v_blank_time_ms<0 ||
																gtf->v_top_margin_us<0 ||
																gtf->odd_v_front_porch_us<0 ||
																gtf->v_sync_time_us<0 ||
																gtf->odd_v_back_porch_us<0 ||
																gtf->v_total_time_per_field_ms<0 ||
																gtf->v_addr_time_per_field_ms<0 ||
																gtf->even_field_total_v_blank_time_ms<0 ||
																gtf->v_front_porch_even_field_us<0 ||
																gtf->even_v_back_porch_us<0 ||
																gtf->pixels_rounded_to_character<0 ||
																gtf->v_pixels<0 ||
																gtf->h_freq_kHz<0 ||
																gtf->pixel_freq_MHz<0
														))/*=TRUE*/), "!!!!! INPUT PARAMETER ERROR !!!!!", "") );


		gtfout( "%-40s %12.3f %-7s\n",												"PREDICTED H BLANK DUTY CYCLE",		gtf->ideal_duty_cycle_percent,						"%" );
		gtfout( "%-40s\n",															"	(from blanking formula)" );
		gtfout( "%-40s %12.3f %-7s %-12s %-7s %12d %-7s\n",							"HOR TOTAL TIME",					gtf->h_total_time_us,								"us",		"",																		"",										gtf->total_h_chars_rounded,						"CHARS" );
		gtfout( "%-40s %12.3f %-7s %-12s %-7s %12d %-7s\n",							"HOR ADDR TIME",					gtf->h_addr_time_us,								"us",		"",																		"",										gtf->addr_time_chars_rounded,					"CHARS" );
		gtfout( "%-40s %12.3f %-7s %-12s %-7s %12d %-7s\n",							"HOR BLANK TIME",					gtf->h_blank_time_us,								"us",		"",																		"",										gtf->blank_time_chars_rounded,					"CHARS" );
		gtfout( "%-40s %12.3f %-7s %-12s %-7s %12d %-7s\n",							"HOR BLANK+MARGIN TIME",			gtf->h_blank_and_margin_time_us,					"us",		"",																		"",										gtf->blank_and_margin_time_chars_rounded,		"CHARS" );
		gtfout( "%-40s %12.3f %-7s\n",												"ACTUAL HOR BLANK DUTY CYCLE",		gtf->actual_h_blank_duty_cycle_percent,				"%" );
		gtfout( "%-40s %12.3f %-7s\n",												"ACT. HOR BLNK+MARGIN DUTY CYCLE",	gtf->act_h_blank_and_margin_duty_cycle_percent,		"%" );
		gtfout( "\n" );
		gtfout( "\n" );
		gtfout( "%-40s %12.3f %-7s %-12s %-7s %12d %-7s\n",							"H LEFT MARGIN",					gtf->h_left_margin_us,								"us",		"",																		"",										gtf->h_left_margin_chars,						"CHARS" );
		gtfout( "%-40s %12.3f %-7s %-12s %-7s %12d %-7s\n",							"H FRONT PORCH",					gtf->h_front_porch_to_nearest_char_cell_us,			"us",		"",																		"",										gtf->h_front_porch_to_nearest_char_cell_chars,	"CHARS" );
		gtfout( "%-40s %12.3f %-7s %-12s %-7s %12d %-7s\n",							"HOR SYNC TIME",					gtf->h_sync_width_to_nearest_char_cell_us,			"us",		"",																		"",										gtf->h_sync_width_to_nearest_char_cell_chars,	"CHARS" );
		gtfout( "%-40s %12.3f %-7s %-12s %-7s %12d %-7s\n",							"H BACK PORCH",						gtf->h_back_porch_to_nearest_char_cell_us,			"us",		"",																		"",										gtf->h_back_porch_to_nearest_char_cell_chars,	"CHARS" );
		gtfout( "%-40s %12.3f %-7s %-12s %-7s %12d %-7s\n",							"H RIGHT MARGIN",					gtf->h_right_margin_us,								"us",		"",																		"",										gtf->h_right_margin_chars,						"CHARS" );
		gtfout( "\n" );
		gtfout( "%-40s %12s %-7s %12s %-7s %12s %-7s %12s\n",						"",									(IF(gtf->want_interlaced,"PER FRAME","")),			"",			(IF(gtf->want_interlaced,"PER FIELD","")),								"",										(IF(gtf->want_interlaced,"PER FRAME","")),		"",			(IF(gtf->want_interlaced,"PER FIELD","")) );
		gtfout( "%-40s %12.3f %-7s %*.3f%*s %-7s %12.3f %-7s %*.3f%*s %-7s\n",		"VER TOTAL TIME",					gtf->v_frame_period_ms,								"ms",		STROF(gtf->want_interlaced,gtf->v_total_time_per_field_ms),				(IF(gtf->want_interlaced,"ms","")),		gtf->total_lines_in_v_frame,					"LINES",	STROF(gtf->want_interlaced,gtf->total_lines),							(IF(gtf->want_interlaced,"LINES","")) );
		gtfout( "%-40s %12.3f %-7s %*.3f%*s %-7s %12d %-7s %*d%*s %-7s\n",			"VER ADDR TIME",					gtf->v_addr_time_per_frame_ms,						"ms",		STROF(gtf->want_interlaced,gtf->v_addr_time_per_field_ms),				(IF(gtf->want_interlaced,"ms","")),		gtf->vert_addr_lines_per_frame,					"LINES",	STROF(gtf->want_interlaced,gtf->lines_rounded),							(IF(gtf->want_interlaced,"LINES","")) );
		gtfout( "\n" );
		gtfout( "\n" );
		gtfout( "%-40s %12s %-7s %12s %-7s %12s %-7s %12s %-7s\n",					"",									(IF(gtf->want_interlaced,"ODD FIELD","")),			"",			(IF(gtf->want_interlaced,"EVEN FIELD","")),								"",										(IF(gtf->want_interlaced,"ODD FIELD","")),		"",			(IF(gtf->want_interlaced,"EVEN FIELD","")) );
		gtfout( "%-40s %12.3f %-7s %*.3f%*s %-7s %12d %-7s %*d%*s %-7s\n",			"VER BLANK TIME",					gtf->odd_field_total_v_blank_time_ms,				"ms",		STROF(gtf->want_interlaced,gtf->even_field_total_v_blank_time_ms),		(IF(gtf->want_interlaced,"ms","")),		gtf->odd_field_total_v_blank_time_lines,		"LINES",	STROF(gtf->want_interlaced,gtf->even_field_total_v_blank_time_lines),	(IF(gtf->want_interlaced,"LINES","")) );
		gtfout( "\n" );
		gtfout( "%-40s %12.3f %-7s %*.3f%*s %-7s %12d %-7s %*d%*s %-7s\n",			"V TOP MARGIN",						gtf->v_top_margin_us,								"us",		STROF(gtf->want_interlaced,gtf->v_top_margin_us),						(IF(gtf->want_interlaced,"us","")),		gtf->top_margin_lines_rounded,					"LINES",	STROF(gtf->want_interlaced,gtf->top_margin_lines_rounded),				(IF(gtf->want_interlaced,"LINES","")) );
		gtfout( "%-40s %12.3f %-7s %*.3f%*s %-7s %12.1f %-7s %*d%*s %-7s\n",		"V FRONT PORCH",					gtf->odd_v_front_porch_us,							"us",		STROF(gtf->want_interlaced,gtf->v_front_porch_even_field_us),			(IF(gtf->want_interlaced,"us","")),		gtf->odd_v_front_porch_lines,					"LINES",	STROF(gtf->want_interlaced,gtf->frontporch_lines_rounded),				(IF(gtf->want_interlaced,"LINES","")) );
		gtfout( "%-40s %12.3f %-7s %*.3f%*s %-7s %12d %-7s %*d%*s %-7s\n",			"VER SYNC TIME",					gtf->v_sync_time_us,								"us",		STROF(gtf->want_interlaced,gtf->v_sync_time_us),						(IF(gtf->want_interlaced,"us","")),		gtf->sync_lines_rounded,						"LINES",	STROF(gtf->want_interlaced,gtf->sync_lines_rounded),					(IF(gtf->want_interlaced,"LINES","")) );
		gtfout( "%-40s %12.3f %-7s %*.3f%*s %-7s %12d %-7s %*.1f%*s %-7s\n",		"V BACK PORCH",						gtf->odd_v_back_porch_us,							"us",		STROF(gtf->want_interlaced,gtf->even_v_back_porch_us),					(IF(gtf->want_interlaced,"us","")),		gtf->back_porch_lines_rounded,					"LINES",	STROF(gtf->want_interlaced,gtf->even_v_back_porch_lines),				(IF(gtf->want_interlaced,"LINES","")) );
		gtfout( "%-40s %12.3f %-7s %*.3f%*s %-7s %12d %-7s %*d%*s %-7s\n",			"V BOTTOM MARGIN",					gtf->v_bottom_margin_us,							"us",		STROF(gtf->want_interlaced,gtf->v_bottom_margin_us),					(IF(gtf->want_interlaced,"us","")),		gtf->bottom_margin_lines_rounded,				"LINES",	STROF(gtf->want_interlaced,gtf->bottom_margin_lines_rounded),			(IF(gtf->want_interlaced,"LINES","")) );
		gtfout( "\n" );
		gtfout( "%s\n",																IF(gtf->input_parameter_error="","","NOTE: ANY RESULT IN RED PARENTHESIS INDICATES AN ERROR: SOLUTION NOT POSSIBLE WITH GIVEN INPUTS REQUIREMENTS") );
		gtfout( "\n" );
		gtfout( "\n" );
		gtfout( "%s\n",																"COMMENT:						GTF Version1 Rev 1.0			Andy Morrish National Semiconductor 1/5/97" );

		return gtf->input_parameter_error[0] == 0;
}



void gtf_from_vertical_rate( gtf_t* gtf )
{
	double	required_refresh_rate_Hz;
	double	estimated_h_period_us;
	double	estimated_v_field_rate_Hz;

	gtfout( "%s\n",					"=============================================================" );
	gtfout( "%s\n",					"VERTICAL REFRESH RATE DRIVEN SCRATCH PAD:" );
	gtfout( "\n" );
	gtfout( "%-50s %10.3f\n",		"REQUIRED REFRESH RATE (Hz):",						required_refresh_rate_Hz = (IF(gtf->want_interlaced,gtf->timing_constraint*2,gtf->timing_constraint)) );
	gtfout( "%-50s %10d\n",			"Horizontal Pixels =",								gtf->pixels_rounded_to_character );
	gtfout( "%-50s %10d\n",			"Vertical Lines =",									gtf->lines_rounded );
	gtfout( "%-50s %10d\n",			"Number of lines for vertical sync =",				gtf->sync_lines_rounded );
	gtfout( "%-50s %10.3f\n",		"tgt time allowed for vertical flyback (us)=", 		gtf->minimum_v_sync_and_back_porch_for_flyback_us );
	gtfout( "%-50s %10d\n",			"front porch=",										gtf->frontporch_lines_rounded );
	gtfout( "%-50s %10d\n",			"CHAR. CELL GRANULARITY (PIXELS):",					gtf->character_cell_pixels_rounded );
	gtfout( "%-50s %10.3f\n",		"MARGIN WIDTH (%):",								gtf->margin_percent_of_height );
	gtfout( "%-50s %10.3f\n",		"SYNC WIDTH (% OF LINE TIME):",						gtf->sync_percent_of_line_time );
	gtfout( "\n" );
	gtfout( "\n" );
	gtfout( "%s\n",					"SPEC STEP #" );
	gtfout( "\n" );
	gtfout( "%s\n",					"    4	DETERMINE NUMBER OF LINES IN V MARGIN" );
	gtfout( "%-50s %10d\n",			"        Top margin (lines)=",						gtf->top_margin_lines_rounded = (IF( gtf->want_margins,ROUND((gtf->margin_percent_of_height/100*gtf->lines_rounded),0),0)) );
	gtfout( "%-50s %10d\n",			"    5   Bottom margin (lines)=",					gtf->bottom_margin_lines_rounded = (IF( gtf->want_margins,ROUND((gtf->margin_percent_of_height/100*gtf->lines_rounded),0),0)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    6	DETERMINE IF 1/2 LINE INTERLACE IS PRESENT" );
	gtfout( "%-50s %10.1f\n",		"        interlace (lines)=",						gtf->interlace_lines = (IF(gtf->want_interlaced,0.5,0)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    7	IF V RETRACE DRIVEN, (OPTION1), ESTIMATE HORIZ. PERIOD" );
	gtfout( "%-50s %10.3f\n",		"        Estimated H period (us)=",					estimated_h_period_us = ((1/required_refresh_rate_Hz)-gtf->minimum_v_sync_and_back_porch_for_flyback_us/1000000)/(gtf->lines_rounded+(2*gtf->top_margin_lines_rounded)+gtf->frontporch_lines_rounded+gtf->interlace_lines)*1000000 );
	gtfout( "\n" );
	gtfout( "%s\n",					"    8	FIND NUMBER OF LINES IN (SYNC + BACK PORCH)" );
	gtfout( "%-50s %10d\n",			"        Sync+bp (lines)=",							gtf->sync_and_bp_lines_rounded = ROUND((gtf->minimum_v_sync_and_back_porch_for_flyback_us/estimated_h_period_us),0) );
	gtfout( "%-50s %10.3f\n",		"        (Actual value =)", 						gtf->minimum_v_sync_and_back_porch_for_flyback_us/estimated_h_period_us );
	gtfout( "\n" );
	gtfout( "%s\n",					"    9	FIND NUMBER OF LINES IN BACK PORCH" );
	gtfout( "%-50s %10d\n",			"        Back porch (lines) =",						gtf->back_porch_lines_rounded = gtf->sync_and_bp_lines_rounded-gtf->sync_lines_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    10	FIND TOTAL NUMBER OF LINES IN VERTICAL FIELD" );
	gtfout( "%-50s %10.3f\n",		"        Total lines = ",							gtf->total_lines = gtf->lines_rounded+gtf->top_margin_lines_rounded+gtf->bottom_margin_lines_rounded+gtf->sync_and_bp_lines_rounded+gtf->interlace_lines+gtf->frontporch_lines_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    11	ESTIMATE VERTICAL FIELD RATE" );
	gtfout( "%-50s %10.3f\n",		"        Estimated vertical field rate (Hz)= ",		estimated_v_field_rate_Hz = 1/estimated_h_period_us/gtf->total_lines*1000000 );
	gtfout( "\n" );
	gtfout( "%s\n",					"    12	FIND ACTUAL HORIZONTAL PERIOD" );
	gtfout( "%-50s %10.3f\n",		"        Actual H period (us) = ",					gtf->h_total_time_us = estimated_h_period_us/(required_refresh_rate_Hz/estimated_v_field_rate_Hz) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    13	FIND ACTUAL VERTICAL FIELD FREQUENCY" );
	gtfout( "%-50s %10.3f\n",		"        Actual vertical field frequency (Hz)=",	gtf->v_field_rate_Hz = 1/gtf->h_total_time_us/gtf->total_lines*1000000 );
	gtfout( "\n" );
	gtfout( "%s\n",					"    14	FIND ACTUAL VERTICAL  FRAME FREQUENCY" );
	gtfout( "%-50s %10.3f\n",		"        Actual vertical frame frequency (Hz)=",	gtf->actual_v_frame_frequency_Hz = (IF(gtf->want_interlaced,gtf->v_field_rate_Hz/2,gtf->v_field_rate_Hz)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    15	DETERMINE NUMBER OF PIXELS IN H MARGIN" );
	gtfout( "%-50s %10d\n",			"        Left margin (pixels) =",					gtf->left_margin_pixels_rounded = (IF( gtf->want_margins,(ROUND( (gtf->pixels_rounded_to_character*gtf->margin_percent_of_height/100/gtf->character_cell_pixels_rounded),0))*gtf->character_cell_pixels_rounded,0)) );
	gtfout( "%-50s %10d\n",			"    16  Right margin (pixels) =",					gtf->right_margin_pixels_rounded = (IF( gtf->want_margins,(ROUND( (gtf->pixels_rounded_to_character*gtf->margin_percent_of_height/100/gtf->character_cell_pixels_rounded),0))*gtf->character_cell_pixels_rounded,0)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    17	FIND TOTAL NUMBER OF ACTIVE PIXELS (IMAGE + MARGIN)" );
	gtfout( "%-50s %10d\n",			"        Total active pixels = ",					gtf->total_active_pixels_rounded = gtf->pixels_rounded_to_character+gtf->left_margin_pixels_rounded+gtf->right_margin_pixels_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    18	FIND IDEAL BLANKING DUTY CYCLE FROM FORMULA" );
	gtfout( "%-50s %10.3f\n",		"        Ideal duty cycle (%)= ",					gtf->ideal_duty_cycle_percent = gtf->blank_equation_C -(gtf->blank_equation_M*gtf->h_total_time_us/1000) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    19	FIND BLANKING TIME (TO NEAREST CHAR CELL)" );
	gtfout( "%-50s %10d\n",			"        Blanking time (pixels)=",					gtf->h_blank_time_to_nearest_char_cell_pixels = (ROUND((gtf->total_active_pixels_rounded*gtf->ideal_duty_cycle_percent/(100-gtf->ideal_duty_cycle_percent)/(2*gtf->character_cell_pixels_rounded)),0))*(2*gtf->character_cell_pixels_rounded) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    20	FIND TOTAL NUMBER OF PIXELS IN A LINE:" );
	gtfout( "%-50s %10d\n",			"        Total number of pixels=",					gtf->total_h_pixels = gtf->total_active_pixels_rounded+gtf->h_blank_time_to_nearest_char_cell_pixels );
	gtfout( "\n" );
	gtfout( "%s\n",					"    21	FIND PIXEL FREQUENCY:" );
	gtfout( "%-50s %10.3f\n",		"        Pixel freq (MHz) = ",						gtf->pixel_freq_MHz = gtf->total_h_pixels/gtf->h_total_time_us );
	gtfout( "\n" );
	gtfout( "%s\n",					"    22	FIND ACTUAL HORIZONTAL FREQUENCY:" );
	gtfout( "%-50s %10.3f\n",		"        Horiz. freq (kHz) = ",						gtf->h_freq_kHz = 1000/gtf->h_total_time_us );
}



void gtf_from_horizontal_rate( gtf_t* gtf )
{
	gtfout( "%s\n",					"=============================================================" );
	gtfout( "%s\n",					"HORIZONTAL SCAN RATE DRIVEN SCRATCH PAD:" );
	gtfout( "\n" );
	gtfout( "%-50s %10.3f\n",		"REQUIRED HORIZONTAL RATE (kHz):",					gtf->h_freq_kHz = gtf->timing_constraint );
	gtfout( "%-50s %10d\n",			"Horizontal Pixels =",								gtf->pixels_rounded_to_character );
	gtfout( "%-50s %10d\n",			"Vertical Lines =",									gtf->lines_rounded );
	gtfout( "%-50s %10d\n",			"Number of lines for vertical sync =",				gtf->sync_lines_rounded );
	gtfout( "%-50s %10.3f\n",		"tgt time allowed for vertical flyback (us)=", 		gtf->minimum_v_sync_and_back_porch_for_flyback_us );
	gtfout( "%-50s %10d\n",			"front porch=",										gtf->frontporch_lines_rounded );
	gtfout( "%-50s %10d\n",			"CHAR. CELL GRANULARITY (PIXELS):",					gtf->character_cell_pixels_rounded );
	gtfout( "%-50s %10.3f\n",		"MARGIN WIDTH (%):",								gtf->margin_percent_of_height );
	gtfout( "%-50s %10.3f\n",		"SYNC WIDTH (% OF LINE TIME):",						gtf->sync_percent_of_line_time );
	gtfout( "\n" );
	gtfout( "\n" );
	gtfout( "%s\n",					"SPEC STEP #" );
	gtfout( "\n" );
	gtfout( "%s\n",					"    4	DETERMINE NUMBER OF LINES IN V MARGIN" );
	gtfout( "%-50s %10d\n",			"        Top margin (lines)=",						gtf->top_margin_lines_rounded = (IF( gtf->want_margins,ROUND((gtf->margin_percent_of_height/100*gtf->lines_rounded),0),0)) );
	gtfout( "%-50s %10d\n",			"    5   Bottom margin (lines)=",					gtf->bottom_margin_lines_rounded = (IF( gtf->want_margins,ROUND((gtf->margin_percent_of_height/100*gtf->lines_rounded),0),0)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    6	DETERMINE IF 1/2 LINE INTERLACE IS PRESENT" );
	gtfout( "%-50s %10.1f\n",		"        interlace (lines)=",						gtf->interlace_lines = (IF(gtf->want_interlaced,0.5,0)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    7	FIND NUMBER OF LINES IN (SYNC + BACK PORCH)" );
	gtfout( "%-50s %10d\n",			"        Sync+bp (lines)=",							gtf->sync_and_bp_lines_rounded = ROUND((gtf->minimum_v_sync_and_back_porch_for_flyback_us*gtf->h_freq_kHz/1000),0) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    8	FIND NUMBER OF LINES IN BACK PORCH" );
	gtfout( "%-50s %10d\n",			"        Back porch (lines) =",						gtf->back_porch_lines_rounded = gtf->sync_and_bp_lines_rounded-gtf->sync_lines_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    9	FIND TOTAL NUMBER OF LINES IN VERTICAL FIELD" );
	gtfout( "%-50s %10.3f\n",		"        Total lines = ",							gtf->total_lines = gtf->lines_rounded+gtf->top_margin_lines_rounded+gtf->bottom_margin_lines_rounded+gtf->interlace_lines+gtf->sync_and_bp_lines_rounded+gtf->frontporch_lines_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    10	FIND VERTICAL FIELD FREQUENCY" );
	gtfout( "%-50s %10.3f\n",		"        Vertical field rate (Hz)= ",				gtf->v_field_rate_Hz = gtf->h_freq_kHz/gtf->total_lines*1000 );
	gtfout( "\n" );
	gtfout( "%s\n",					"    11	FIND VERTICAL FRAME FREQUENCY" );
	gtfout( "%-50s %10.3f\n",		"        Actual vertical frame frequency (Hz)=",	gtf->actual_v_frame_frequency_Hz = (IF(gtf->want_interlaced,gtf->v_field_rate_Hz/2,gtf->v_field_rate_Hz)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    12	DETERMINE NUMBER OF PIXELS IN H MARGIN" );
	gtfout( "%-50s %10d\n",			"        Left margin (pixels) =",					gtf->left_margin_pixels_rounded = (IF( gtf->want_margins,(ROUND( (gtf->pixels_rounded_to_character*gtf->margin_percent_of_height/100/gtf->character_cell_pixels_rounded),0))*gtf->character_cell_pixels_rounded,0)) );
	gtfout( "%-50s %10d\n",			"    13  Right margin (pixels) =",					gtf->right_margin_pixels_rounded = (IF( gtf->want_margins,(ROUND( (gtf->pixels_rounded_to_character*gtf->margin_percent_of_height/100/gtf->character_cell_pixels_rounded),0))*gtf->character_cell_pixels_rounded,0)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    14	FIND TOTAL NUMBER OF ACTIVE PIXELS (IMAGE + MARGIN)" );
	gtfout( "%-50s %10d\n",			"        Total active pixels = ",					gtf->total_active_pixels_rounded = gtf->pixels_rounded_to_character+gtf->left_margin_pixels_rounded+gtf->right_margin_pixels_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    15	FIND IDEAL BLANKING DUTY CYCLE FROM FORMULA" );
	gtfout( "%-50s %10.3f\n",		"        Ideal duty cycle (%)= ",					gtf->ideal_duty_cycle_percent = gtf->blank_equation_C -(gtf->blank_equation_M/gtf->h_freq_kHz) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    16	FIND BLANKING TIME (TO NEAREST CHAR CELL)" );
	gtfout( "%-50s %10d\n",			"        Blanking time (pixels)=",					gtf->h_blank_time_to_nearest_char_cell_pixels = (ROUND((gtf->total_active_pixels_rounded*gtf->ideal_duty_cycle_percent/(100-gtf->ideal_duty_cycle_percent)/(2*gtf->character_cell_pixels_rounded)),0))*(2*gtf->character_cell_pixels_rounded) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    17	FIND TOTAL NUMBER OF PIXELS IN A LINE:" );
	gtfout( "%-50s %10d\n",			"        Total number of pixels=",					gtf->total_h_pixels = gtf->total_active_pixels_rounded+gtf->h_blank_time_to_nearest_char_cell_pixels );
	gtfout( "\n" );
	gtfout( "%s\n",					"    18	FIND HORIZONTALPERIOD" );
	gtfout( "%-50s %10.3f\n",		"        Total Horizontal time (us)=",				gtf->h_total_time_us = 1000/gtf->h_freq_kHz );
	gtfout( "\n" );
	gtfout( "%s\n",					"    19	FIND PIXEL FREQUENCY:" );
	gtfout( "%-50s %10.3f\n",		"        Pixel freq (MHz) = ",						gtf->pixel_freq_MHz = gtf->total_h_pixels*gtf->h_freq_kHz/1000 );
}



void gtf_from_pixel_rate( gtf_t* gtf )
{
	double	ideal_h_period;

	gtfout( "%s\n",					"=============================================================" );
	gtfout( "%s\n",					"PIXEL RATE DRIVEN SCRATCH PAD:" );
	gtfout( "\n" );
	gtfout( "%-50s %10.3f\n",		"REQUIRED PIXEL RATE (MHz):",						gtf->pixel_freq_MHz = gtf->timing_constraint );
	gtfout( "%-50s %10d\n",			"Horizontal Pixels =",								gtf->pixels_rounded_to_character );
	gtfout( "%-50s %10d\n",			"Vertical Lines =",									gtf->lines_rounded );
	gtfout( "%-50s %10d\n",			"Number of lines for vertical sync =",				gtf->sync_lines_rounded );
	gtfout( "%-50s %10.3f\n",		"tgt time allowed for vertical flyback (us)=", 		gtf->minimum_v_sync_and_back_porch_for_flyback_us );
	gtfout( "%-50s %10d\n",			"front porch=",										gtf->frontporch_lines_rounded );
	gtfout( "%-50s %10d\n",			"CHAR. CELL GRANULARITY (PIXELS):",					gtf->character_cell_pixels_rounded );
	gtfout( "%-50s %10.3f\n",		"MARGIN WIDTH (%):",								gtf->margin_percent_of_height );
	gtfout( "%-50s %10.3f\n",		"SYNC WIDTH (% OF LINE TIME):",						gtf->sync_percent_of_line_time );
	gtfout( "\n" );
	gtfout( "\n" );
	gtfout( "%s\n",					"SPEC STEP #" );
	gtfout( "\n" );
	gtfout( "%s\n",					"    4	DETERMINE NUMBER OF PIXELS IN H MARGIN" );
	gtfout( "%-50s %10d\n",			"        Left margin (pixels) =",					gtf->left_margin_pixels_rounded = (IF( gtf->want_margins,(ROUND( (gtf->pixels_rounded_to_character*gtf->margin_percent_of_height/100/gtf->character_cell_pixels_rounded),0))*gtf->character_cell_pixels_rounded,0)) );
	gtfout( "%-50s %10d\n",			"    5   Right margin (pixels) =",					gtf->right_margin_pixels_rounded = (IF( gtf->want_margins,(ROUND( (gtf->pixels_rounded_to_character*gtf->margin_percent_of_height/100/gtf->character_cell_pixels_rounded),0))*gtf->character_cell_pixels_rounded,0)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    6	FIND TOTAL NUMBER OF ACTIVE PIXELS (IMAGE + MARGIN)" );
	gtfout( "%-50s %10d\n",			"        Total active pixels = ",					gtf->total_active_pixels_rounded = gtf->pixels_rounded_to_character+gtf->left_margin_pixels_rounded+gtf->right_margin_pixels_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    7	FIND IDEAL HORIZONTAL PERIOD FROM FORMULA" );
	gtfout( "%-50s %10.3f\n",		"        Ideal horizontal period = ",				ideal_h_period = ((gtf->blank_equation_C-100)+(SQRT((SQR(100-gtf->blank_equation_C))+(0.4*gtf->blank_equation_M*(gtf->total_active_pixels_rounded+gtf->right_margin_pixels_rounded+gtf->left_margin_pixels_rounded)/gtf->pixel_freq_MHz))))/2/gtf->blank_equation_M*1000 );
	gtfout( "\n" );
	gtfout( "%s\n",					"    8	FIND IDEAL BLANKING DUTY CYCLE FROM FORMULA" );
	gtfout( "%-50s %10.3f\n",		"        Ideal duty cycle (%)= ",					gtf->ideal_duty_cycle_percent = gtf->blank_equation_C -(gtf->blank_equation_M*ideal_h_period/1000) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    9	FIND BLANKING TIME (TO NEAREST CHAR CELL)" );
	gtfout( "%-50s %10d\n",			"        Blanking time (pixels)=",					gtf->h_blank_time_to_nearest_char_cell_pixels = (ROUND((gtf->total_active_pixels_rounded*gtf->ideal_duty_cycle_percent/(100-gtf->ideal_duty_cycle_percent)/(2*gtf->character_cell_pixels_rounded)),0))*(2*gtf->character_cell_pixels_rounded) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    10	FIND TOTAL NUMBER OF PIXELS IN A LINE:" );
	gtfout( "%-50s %10d\n",			"        Total number of pixels=",					gtf->total_h_pixels = gtf->total_active_pixels_rounded+gtf->h_blank_time_to_nearest_char_cell_pixels );
	gtfout( "\n" );
	gtfout( "%s\n",					"    11	FIND ACTUAL HORIZONTAL FREQUENCY:" );
	gtfout( "%-50s %10.3f\n",		"        Horiz. freq (kHz) = ",						gtf->h_freq_kHz = gtf->pixel_freq_MHz/gtf->total_h_pixels*1000 );
	gtfout( "\n" );
	gtfout( "%s\n",					"    12	FIND ACTUAL HORIZONTALPERIOD" );
	gtfout( "%-50s %10.3f\n",		"        Actual total Horizontal period (us)=",		gtf->h_total_time_us = 1000/gtf->h_freq_kHz );
	gtfout( "\n" );
	gtfout( "%s\n",					"    13	DETERMINE NUMBER OF LINES IN V MARGIN" );
	gtfout( "%-50s %10d\n",			"        Top margin (lines)=",						gtf->top_margin_lines_rounded = (IF( gtf->want_margins,ROUND((gtf->margin_percent_of_height/100*gtf->lines_rounded),0),0)) );
	gtfout( "%-50s %10d\n",			"    14  Bottom margin (lines)=",					gtf->bottom_margin_lines_rounded = (IF( gtf->want_margins,ROUND((gtf->margin_percent_of_height/100*gtf->lines_rounded),0),0)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    15	DETERMINE IF 1/2 LINE INTERLACE IS PRESENT" );
	gtfout( "%-50s %10.1f\n",		"        interlace (lines)=",						gtf->interlace_lines = (IF(gtf->want_interlaced,0.5,0)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    16	FIND NUMBER OF LINES IN (SYNC + BACK PORCH)" );
	gtfout( "%-50s %10d\n",			"        Sync+bp (lines)=",							gtf->sync_and_bp_lines_rounded = ROUND((gtf->minimum_v_sync_and_back_porch_for_flyback_us*gtf->h_freq_kHz/1000),0) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    17	FIND NUMBER OF LINES IN BACK PORCH" );
	gtfout( "%-50s %10d\n",			"        Back porch (lines) =",						gtf->back_porch_lines_rounded = gtf->sync_and_bp_lines_rounded-gtf->sync_lines_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    18	FIND TOTAL NUMBER OF LINES IN VERTICAL FIELD" );
	gtfout( "%-50s %10.3f\n",		"        Total lines = ",							gtf->total_lines = gtf->lines_rounded+gtf->top_margin_lines_rounded+gtf->bottom_margin_lines_rounded+gtf->interlace_lines+gtf->sync_and_bp_lines_rounded+gtf->frontporch_lines_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    19	FIND VERTICAL FIELD RATE" );
	gtfout( "%-50s %10.3f\n",		"        Vertical field rate (Hz)= ",				gtf->v_field_rate_Hz = gtf->h_freq_kHz/gtf->total_lines*1000 );
	gtfout( "\n" );
	gtfout( "%s\n",					"    20	FIND VERTICAL FRAME FREQUENCY" );
	gtfout( "%-50s %10.3f\n",		"        Actual vertical frame frequency (Hz)=",	gtf->actual_v_frame_frequency_Hz = (IF(gtf->want_interlaced,gtf->v_field_rate_Hz/2,gtf->v_field_rate_Hz)) );
}



void gtf_derived_parameters( gtf_t* gtf )
{
	gtfout( "%s\n",					"=============================================================" );
	gtfout( "%s\n",					"DERIVED PARAMETERS" );
	gtfout( "\n" );
	gtfout( "%s\n",					"    1	FIND NUMBER OF VERT ADDR LINES PER FRAME" );
	gtfout( "%-50s %10d\n",			"        Addr lines per frame (lines) = ",			gtf->vert_addr_lines_per_frame = (IF(gtf->want_interlaced,gtf->lines_rounded*2,gtf->lines_rounded)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    2	FIND CHARACTER  TIME" );
	gtfout( "%-50s %10.3f\n",		"        Char time (ns) = ",						gtf->char_time_ns = gtf->character_cell_pixels_rounded/gtf->pixel_freq_MHz*1000 );
	gtfout( "%s\n",					"    3	FIND TOTAL NUMBER OF LINES IN VERTICAL FRAME" );
	gtfout( "%-50s %10.3f\n",		"        Total lines = ",							gtf->total_lines_in_v_frame = (IF(gtf->want_interlaced,2*(gtf->lines_rounded+gtf->top_margin_lines_rounded+gtf->bottom_margin_lines_rounded+gtf->sync_and_bp_lines_rounded+gtf->interlace_lines+gtf->frontporch_lines_rounded),(gtf->lines_rounded+gtf->top_margin_lines_rounded+gtf->bottom_margin_lines_rounded+gtf->sync_and_bp_lines_rounded+gtf->interlace_lines+gtf->frontporch_lines_rounded))) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    4	FIND HORIZONTAL TOTAL TIME (CHARS)" );
	gtfout( "%-50s %10d\n",			"        Total Horizontal time (chars)=",			gtf->total_h_chars_rounded = ROUND(gtf->total_h_pixels/gtf->character_cell_pixels_rounded,0) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    5	FIND HORIZONTAL ADDRESSABLE TIME" );
	gtfout( "%-50s %10.3f\n",		"        Addr. time (us)=",							gtf->h_addr_time_us = gtf->pixels_rounded_to_character/gtf->pixel_freq_MHz );
	gtfout( "\n" );
	gtfout( "%s\n",					"    6	FIND HORIZONTAL ADDRESSABLE TIME" );
	gtfout( "%-50s %10d\n",			"        Addr. time (chars)=",						gtf->addr_time_chars_rounded = ROUND(gtf->pixels_rounded_to_character/gtf->character_cell_pixels_rounded,0) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    7	FIND BLANKING TIME" );
	gtfout( "%-50s %10.3f\n",		"        Blanking time (us)=",						gtf->h_blank_time_us = gtf->h_blank_time_to_nearest_char_cell_pixels/gtf->pixel_freq_MHz );
	gtfout( "\n" );
	gtfout( "%s\n",					"    8	FIND BLANKING TIME (CHARS)" );
	gtfout( "%-50s %10d\n",			"        Blanking time (chars)=",					gtf->blank_time_chars_rounded = ROUND(gtf->h_blank_time_to_nearest_char_cell_pixels/gtf->character_cell_pixels_rounded,0) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    9	FIND BLANKING +MARGINS TIME" );
	gtfout( "%-50s %10.3f\n",		"        Blank + Margin time (us)=",				gtf->h_blank_and_margin_time_us = (gtf->h_blank_time_to_nearest_char_cell_pixels+gtf->left_margin_pixels_rounded+gtf->right_margin_pixels_rounded)/gtf->pixel_freq_MHz );
	gtfout( "\n" );
	gtfout( "%s\n",					"    10	FIND BLANKING +MARGINS TIME (CHARS)" );
	gtfout( "%-50s %10d\n",			"        Blank + Margin time (chars)=",				gtf->blank_and_margin_time_chars_rounded = ROUND((gtf->h_blank_time_to_nearest_char_cell_pixels+gtf->left_margin_pixels_rounded+gtf->right_margin_pixels_rounded)/gtf->character_cell_pixels_rounded,0) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    11	FIND ACTUAL BLANKING DUTY CYCLE" );
	gtfout( "%-50s %10.3f\n",		"        Blanking duty cycle = ",					gtf->actual_h_blank_duty_cycle_percent = gtf->blank_time_chars_rounded*100.0/gtf->total_h_chars_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    12	FIND ACTUAL BLANKING + MARGIN DUTY CYCLE" );
	gtfout( "%-50s %10.3f\n",		"        Blanking duty cycle = ",					gtf->act_h_blank_and_margin_duty_cycle_percent = gtf->blank_and_margin_time_chars_rounded*100.0/gtf->total_h_chars_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    13	FIND H LEFT MARGIN" );
	gtfout( "%-50s %10.3f\n",		"        H left margin (us) = ",					gtf->h_left_margin_us = gtf->left_margin_pixels_rounded/gtf->pixel_freq_MHz*1000 );
	gtfout( "\n" );
	gtfout( "%s\n",					"    14	FIND H LEFT MARGIN" );
	gtfout( "%-50s %10d\n",			"        H left margin (chars) = ",					gtf->h_left_margin_chars = gtf->left_margin_pixels_rounded/gtf->character_cell_pixels_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    15	FIND H RIGHT MARGIN" );
	gtfout( "%-50s %10.3f\n",		"        H right margin (us) = ",					gtf->h_right_margin_us = gtf->right_margin_pixels_rounded/gtf->pixel_freq_MHz*1000 );
	gtfout( "\n" );
	gtfout( "%s\n",					"    16	FIND H RIGHT MARGIN" );
	gtfout( "%-50s %10d\n",			"        H right margin (chars) = ",				gtf->h_right_margin_chars = gtf->right_margin_pixels_rounded/gtf->character_cell_pixels_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    17	FIND SYNC WIDTH (TO NEAREST CHAR CELL)" );
	gtfout( "%-50s %10d\n",			"        Sync width (pixels)=",						gtf->h_sync_width_to_nearest_char_cell_pixels = (ROUND((gtf->sync_percent_of_line_time/100*gtf->total_h_pixels/gtf->character_cell_pixels_rounded),0))*gtf->character_cell_pixels_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    18	FIND FRONT PORCH(TO NEAREST CHAR CELL)" );
	gtfout( "%-50s %10d\n",			"        Front porch (pixels)= ",					gtf->h_front_porch_to_nearest_char_cell_pixels = (gtf->h_blank_time_to_nearest_char_cell_pixels/2)-gtf->h_sync_width_to_nearest_char_cell_pixels );
	gtfout( "\n" );
	gtfout( "%s\n",					"    19	FIND BACK PORCH(TO NEAREST CHAR CELL)" );
	gtfout( "%-50s %10d\n",			"        Back porch (pixels) =",					gtf->h_back_porch_to_nearest_char_cell_pixels = gtf->h_front_porch_to_nearest_char_cell_pixels+gtf->h_sync_width_to_nearest_char_cell_pixels );
	gtfout( "\n" );
	gtfout( "%s\n",					"    20	FIND SYNC WIDTH (TO NEAREST CHAR CELL)" );
	gtfout( "%-50s %10d\n",			"        Sync width (chars)=",						gtf->h_sync_width_to_nearest_char_cell_chars = gtf->h_sync_width_to_nearest_char_cell_pixels/gtf->character_cell_pixels_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    21	FIND SYNC WIDTH (TO NEAREST CHAR CELL)" );
	gtfout( "%-50s %10.3f\n",		"        Sync width (us)=",							gtf->h_sync_width_to_nearest_char_cell_us = gtf->h_sync_width_to_nearest_char_cell_pixels/gtf->pixel_freq_MHz );
	gtfout( "\n" );
	gtfout( "%s\n",					"    22	FIND FRONT PORCH(TO NEAREST CHAR CELL)" );
	gtfout( "%-50s %10d\n",			"        Front porch (chars)= ",					gtf->h_front_porch_to_nearest_char_cell_chars = gtf->h_front_porch_to_nearest_char_cell_pixels/gtf->character_cell_pixels_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    23	FIND FRONT PORCH(TO NEAREST CHAR CELL)" );
	gtfout( "%-50s %10.3f\n",		"        Front porch (us)= ",						gtf->h_front_porch_to_nearest_char_cell_us = gtf->h_front_porch_to_nearest_char_cell_pixels/gtf->pixel_freq_MHz );
	gtfout( "\n" );
	gtfout( "%s\n",					"    24	FIND BACK PORCH(TO NEAREST CHAR CELL)" );
	gtfout( "%-50s %10d\n",			"        Back porch (chars) =",						gtf->h_back_porch_to_nearest_char_cell_chars = gtf->h_back_porch_to_nearest_char_cell_pixels/gtf->character_cell_pixels_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    25	FIND BACK PORCH(TO NEAREST CHAR CELL)" );
	gtfout( "%-50s %10.3f\n",		"        Back porch (us) =",						gtf->h_back_porch_to_nearest_char_cell_us = gtf->h_back_porch_to_nearest_char_cell_pixels/gtf->pixel_freq_MHz );
	gtfout( "\n" );
	gtfout( "\n" );
	gtfout( "%s\n",					"    26	FIND VERTICAL FRAME PERIOD" );
	gtfout( "%-50s %10.3f\n",		"        Frame period (ms) = ",						gtf->v_frame_period_ms = (IF(gtf->want_interlaced,gtf->total_lines*gtf->h_total_time_us/1000*2,gtf->total_lines*gtf->h_total_time_us/1000)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    27	FIND VERTICAL FIELD PERIOD" );
	gtfout( "%-50s %10.3f\n",		"        Field period (ms) = ",						gtf->v_total_time_per_field_ms = gtf->total_lines*gtf->h_total_time_us/1000 );
	gtfout( "\n" );
	gtfout( "%s\n",					"    28	FIND VERTICAL ADDR TIME PER FRAME" );
	gtfout( "%-50s %10.3f\n",		"        Ver addr time per frame (ms)- ",			gtf->v_addr_time_per_frame_ms = (IF(gtf->want_interlaced,gtf->lines_rounded*gtf->h_total_time_us/1000*2,gtf->lines_rounded*gtf->h_total_time_us/1000)) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    29	FIND VERTICAL ADDR TIME PER FIELD" );
	gtfout( "%-50s %10.3f\n",		"        Ver addr time per field (ms)- ",			gtf->v_addr_time_per_field_ms = gtf->lines_rounded*gtf->h_total_time_us/1000 );
	gtfout( "\n" );
	gtfout( "%s\n",					"    30	FIND ODD FIELD TOTAL VERTICAL BLANKING" );
	gtfout( "%-50s %10d\n",			"        Odd Ver Blanking time (lines) = ",			gtf->odd_field_total_v_blank_time_lines = gtf->sync_and_bp_lines_rounded+gtf->frontporch_lines_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    31	FIND ODD FIELD TOTAL VERTICAL BLANKING TIME" );
	gtfout( "%-50s %10.3f\n",		"        Odd Ver Blanking time (ms) = ",			gtf->odd_field_total_v_blank_time_ms = (gtf->sync_and_bp_lines_rounded+gtf->frontporch_lines_rounded)*gtf->h_total_time_us/1000 );
	gtfout( "\n" );
	gtfout( "%s\n",					"    32	FIND EVEN FIELD TOTAL VERTICAL BLANKING" );
	gtfout( "%-50s %10d\n",			"        Even Ver Blanking time (lines) = ",		gtf->even_field_total_v_blank_time_lines = gtf->sync_and_bp_lines_rounded+(2*gtf->interlace_lines)+gtf->frontporch_lines_rounded );
	gtfout( "\n" );
	gtfout( "%s\n",					"    33	FIND EVEN FIELD TOTAL VERTICAL BLANKING TIME" );
	gtfout( "%-50s %10.3f\n",		"        Even Ver Blanking time (ms) = ",			gtf->even_field_total_v_blank_time_ms = (gtf->sync_and_bp_lines_rounded+(2*gtf->interlace_lines)+gtf->frontporch_lines_rounded)/1000*gtf->h_total_time_us );
	gtfout( "\n" );
	gtfout( "%s\n",					"    34	FIND VERTICAL TOP MARGIN" );
	gtfout( "%-50s %10.3f\n",		"        Vertical top margin (us) =",				gtf->v_top_margin_us = gtf->top_margin_lines_rounded*gtf->h_total_time_us );
	gtfout( "\n" );
	gtfout( "%s\n",					"    35	FIND ODD FIELD VERTICAL FRONT PORCH TIME" );
	gtfout( "%-50s %10.3f\n",		"        Odd Ver f. porch (us) = ",					gtf->odd_v_front_porch_us = (gtf->frontporch_lines_rounded+gtf->interlace_lines)*gtf->h_total_time_us );
	gtfout( "\n" );
	gtfout( "%s\n",					"    36	FIND ODD FIELD VERTICAL FRONT PORCH" );
	gtfout( "%-50s %10.1f\n",		"        Odd Ver f. porch (lines) = ",				gtf->odd_v_front_porch_lines = (gtf->frontporch_lines_rounded+gtf->interlace_lines) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    37	FIND EVEN FIELD VERTICAL FRONT PORCH TIME" );
	gtfout( "%-50s %10.3f\n",		"        Even Ver f. porch (us) = ",				gtf->v_front_porch_even_field_us = gtf->frontporch_lines_rounded*gtf->h_total_time_us );
	gtfout( "\n" );
	gtfout( "%s\n",					"    38	FIND VERTICAL SYNC TIME" );
	gtfout( "%-50s %10.3f\n",		"        Vertical sync time (us)= ",				gtf->v_sync_time_us = gtf->sync_lines_rounded*gtf->h_total_time_us );
	gtfout( "\n" );
	gtfout( "%s\n",					"    39	FIND EVEN FIELD VERTICAL BACK PORCH TIME" );
	gtfout( "%-50s %10.3f\n",		"        Even Ver b. porch (us) = ",				gtf->even_v_back_porch_us = (gtf->back_porch_lines_rounded+gtf->interlace_lines)*gtf->h_total_time_us );
	gtfout( "\n" );
	gtfout( "%s\n",					"    40	FIND EVEN FIELD VERTICAL BACK PORCH" );
	gtfout( "%-50s %10.1f\n",		"        Even Ver b. porch (lines) = ",				gtf->even_v_back_porch_lines = (gtf->back_porch_lines_rounded+gtf->interlace_lines) );
	gtfout( "\n" );
	gtfout( "%s\n",					"    41	FIND ODD FIELD VERTICAL BACK PORCH TIME" );
	gtfout( "%-50s %10.3f\n",		"        Odd Ver b. porch (us) = ",					gtf->odd_v_back_porch_us = gtf->back_porch_lines_rounded*gtf->h_total_time_us );
	gtfout( "\n" );
	gtfout( "%s\n",					"    42	FIND VERTICAL BOTTOM MARGIN TIME" );
	gtfout( "%-50s %10.3f\n",		"        Vertical bot. margin (us) =",				gtf->v_bottom_margin_us = gtf->bottom_margin_lines_rounded*gtf->h_total_time_us );
}
