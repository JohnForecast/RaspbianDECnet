/*	CTERM structures
	Author:			Eduardo Marcelo Serrat
*/

#define CTRL_A	0x01
#define CTRL_B	0x02
#define CTRL_C	0x03
#define CTRL_D	0x04
#define CTRL_E	0x05
#define CTRL_F	0x06
#define CTRL_G	0x07
#define CTRL_H	0x08
#define BS	0x08
#define CTRL_I	0x09
#define CTRL_J	0x0A
#define CTRL_K	0x0B
#define CTRL_L	0x0C
#define CTRL_M	0x0D
#define CTRL_N	0x0E
#define CTRL_O	0x0F
#define CTRL_P	0x10
#define	CTRL_Q  0x11
#define CTRL_R	0x12
#define CTRL_S	0x13
#define	CTRL_T	0x14
//efine CTRL_U	0x15
#define CTRL_V	0x16
#define CTRL_W	0x17
#define CTRL_U	0x17
#define	CTRL_X	0x18
#define CTRL_Y	0x19
#define CTRL_Z	0x1A
#define ESC	0x1B
#define DEL	0x7F



static	char	BELL=0x07;
struct	logical_terminal_characteristics
{
	short			mode_writing_allowed;
	int			terminal_attributes;
	char			terminal_type[6];
	short			output_flow_control;
	short			output_page_stop;
	short			flow_character_pass_through;
	short			input_flow_control;
	short			loss_notification;
	int			line_width;
	int			page_length;
	int			stop_length;
	int			cr_fill;
	int			lf_fill;
	int			wrap;
	int			horizontal_tab;
	int			vertical_tab;
	int			form_feed;
};

struct	physical_terminal_characteristics
{
	int			input_speed;
	int			output_speed;
	int			character_size;
	short			parity_enable;
	int			parity_type;
	short			modem_present;
	short			auto_baud_detect;
	short			management_guaranteed;
	char			switch_char_1;
	char			switch_char_2;
	short			eigth_bit;
	short			terminal_management_enabled;
};

struct	handler_maintained_characteristics
{
	short			ignore_input;
	short			control_o_pass_through;
	short			raise_input;
	short			normal_echo;
	short			input_escseq_recognition;
	short			output_escseq_recognition;
	int			input_count_state;
	short			auto_prompt;
	short			error_processing;
};

