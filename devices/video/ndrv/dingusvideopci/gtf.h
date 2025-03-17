/*
	Joe van Tunen
	Nov 4, 2005
*/

#ifndef __GTF_H
#define __GTF_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum timing_constraint_choice_t {
	timing_from_vertical_refresh_rate = 1,
	timing_from_horizontal_scan_rate = 2,
	timing_from_pixel_clock = 3
} timing_constraint_choice_t;



typedef struct gtf_t {
	/* INPUTS */

	int		h_pixels;
	int		v_pixels;
	int		want_margins;
	int		want_interlaced;
	double	timing_constraint;
	timing_constraint_choice_t timing_constraint_choice;


	/* DEFAULT PARAMETER VALUES */

	double	margin_percent_of_height;
	int		character_cell_pixels;
	int		minimum_porch_lines;
	int		v_sync_lines;
	double	sync_percent_of_line_time;
	double	minimum_v_sync_and_back_porch_for_flyback_us;
	double	M_gradient_percent_per_kHz;
	double	C_offset_percent;
	double	K_blank_time_scaling_factor;
	double	J_scaling_factor_weighting;


	/* OUTPUTS */

	int		pixels_rounded_to_character;
/*	int		v_pixels; */
	double	actual_v_frame_frequency_Hz;
	double	v_field_rate_Hz;
	double	h_freq_kHz;
	double	pixel_freq_MHz;
	const char*	input_parameter_error;


	/* WORK AREA */

	int		character_cell_pixels_rounded;
	int		frontporch_lines_rounded;
	int		sync_lines_rounded;
	double	K_blank_time_scaling_factor_adjusted;
	double	blank_equation_M;
	double	blank_equation_C;
/*	double	pixels_rounded_to_character; */
	int		lines_rounded;


	/* COMMON SCRATCH PAD: */

	int		top_margin_lines_rounded;
	int		bottom_margin_lines_rounded;
	double	interlace_lines;
	int		sync_and_bp_lines_rounded;
	int		back_porch_lines_rounded;
	double	total_lines;
	double	h_total_time_us;
/*	double	v_field_rate_Hz; */
/*	double	actual_v_frame_frequency_Hz; */
	int		left_margin_pixels_rounded;
	int		right_margin_pixels_rounded;
	double	ideal_duty_cycle_percent;
	int		h_blank_time_to_nearest_char_cell_pixels;
	int		total_h_pixels;
/*	double	h_freq_kHz; */
/*	double	pixel_freq_MHz; */
	int		total_active_pixels_rounded;


	/* DERIVED PARAMETERS */

	int		vert_addr_lines_per_frame;
	double	char_time_ns;
	double	total_lines_in_v_frame;
	int		total_h_chars_rounded;
	double	h_addr_time_us;
	int		addr_time_chars_rounded;
	double	h_blank_time_us;
	int		blank_time_chars_rounded;
	double	h_blank_and_margin_time_us;
	int		blank_and_margin_time_chars_rounded;
	double	actual_h_blank_duty_cycle_percent;
	double	act_h_blank_and_margin_duty_cycle_percent;
	double	h_left_margin_us;
	int		h_left_margin_chars;
	double	h_right_margin_us;
	int		h_right_margin_chars;
	int		h_sync_width_to_nearest_char_cell_pixels;
	int		h_front_porch_to_nearest_char_cell_pixels;
	int		h_back_porch_to_nearest_char_cell_pixels;
	int		h_sync_width_to_nearest_char_cell_chars;
	double	h_sync_width_to_nearest_char_cell_us;
	int		h_front_porch_to_nearest_char_cell_chars;
	double	h_front_porch_to_nearest_char_cell_us;
	int		h_back_porch_to_nearest_char_cell_chars;
	double	h_back_porch_to_nearest_char_cell_us;
	double	v_frame_period_ms;
	double	v_total_time_per_field_ms;
	double	v_addr_time_per_frame_ms;
	double	v_addr_time_per_field_ms;
	int		odd_field_total_v_blank_time_lines;
	double	odd_field_total_v_blank_time_ms;
	int		even_field_total_v_blank_time_lines;
	double	even_field_total_v_blank_time_ms;
	double	v_top_margin_us;
	double	odd_v_front_porch_us;
	double	odd_v_front_porch_lines;
	double	v_front_porch_even_field_us;
	double	v_sync_time_us;
	double	even_v_back_porch_us;
	double	even_v_back_porch_lines;
	double	odd_v_back_porch_us;
	double	v_bottom_margin_us;
} gtf_t;


void gtf_init( gtf_t* gtf );
int gtf_main( gtf_t* gtf );

#ifdef __cplusplus
}
#endif

#endif
