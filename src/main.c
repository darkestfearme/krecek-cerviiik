/*
 * Copyright (C) Jan Hamal Dvořák <mordae@anilinux.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <pico/multicore.h>
#include <pico/stdio_usb.h>
#include <pico/stdlib.h>

#include <hardware/adc.h>
#include <hardware/pwm.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <task.h>
#include <tft.h>

#define TFT_LED_PWM 6

#define BTN_PIN 15

#define P1L_PIN 9
#define P1R_PIN 10
#define P2L_PIN 18
#define P2R_PIN 19
#define P3L_PIN 21
#define P3R_PIN 20
#define P4L_PIN 13
#define P4R_PIN 12

#define JOY_BTN_PIN 22
#define JOY_X_PIN 27
#define JOY_Y_PIN 28

#define RED 240
#define YELLOW 242
#define GREEN 244
#define BLUE 250
#define GRAY 8
#define WHITE 15

#define ANGLE_DELTA (5.0f / 180.0f * M_PI)

static int input_joy_x = 0;
static int input_joy_y = 0;
static int input_joy_btn = 0;

static int p1l_btn = 0;
static int p1r_btn = 0;
static int p2l_btn = 0;
static int p2r_btn = 0;
static int p3l_btn = 0;
static int p3r_btn = 0;
static int p4l_btn = 0;
static int p4r_btn = 0;


static void stats_task(void);
static void tft_task(void);
static void input_task(void);

struct worm {
	float x, y;
	float angle;
	float speed;
	uint8_t color;
	bool alive;
};

#define WIDTH 160
#define HEIGHT 120
static int8_t grid[WIDTH][HEIGHT];

#define NUM_WORMS 4
static struct worm worms[NUM_WORMS];
static struct worm worms_init[NUM_WORMS] = {
	{
		.x = WIDTH / 2 - 5,
		.y = HEIGHT / 2 - 5,
		.angle = 225.0f / 180.0f * M_PI,
		.speed = 0.5f,
		.color = RED,
		.alive = true,
	},
	{
		.x = WIDTH / 2 + 5,
		.y = HEIGHT / 2 + 5,
		.angle = 45.0f / 180.0f * M_PI,
		.speed = 0.5f,
		.color = GREEN,
		.alive = true,
	},
	{
		.x = WIDTH / 2 + 5,
		.y = HEIGHT / 2 - 5,
		.angle = 315.0f / 180.0f * M_PI,
		.speed = 0.5f,
		.color = BLUE,
		.alive = true,
	},
	{
		.x = WIDTH / 2 - 5,
		.y = HEIGHT / 2 + 5,
		.angle = 135.0f / 180.0f * M_PI,
		.speed = 0.5f,
		.color = YELLOW,
		.alive = true,
	},

};

/*
 * Tasks to run concurrently:
 */
task_t task_avail[NUM_CORES][MAX_TASKS] = {
	{
		/* On the first core: */
		MAKE_TASK(4, "stats", stats_task),
		MAKE_TASK(1, "input", input_task),
		NULL,
	},
	{
		/* On the second core: */
		MAKE_TASK(1, "tft", tft_task),
		NULL,
	},
};

/*
 * Reports on all running tasks every 10 seconds.
 */
static void stats_task(void)
{
	while (true) {
		task_sleep_ms(10 * 1000);

		for (unsigned i = 0; i < NUM_CORES; i++)
			task_stats_report_reset(i);
	}
}

/*
 * Processes joystick and button inputs.
 */
static void input_task(void)
{
	gpio_init(BTN_PIN);
	gpio_pull_up(BTN_PIN);

	gpio_init(JOY_BTN_PIN);
	gpio_pull_up(JOY_BTN_PIN);

	// p1
	gpio_init(P1L_PIN);
	gpio_pull_up(P1L_PIN);

	gpio_init(P1R_PIN);
	gpio_pull_up(P1R_PIN);

	// p2
	gpio_init(P2L_PIN);
	gpio_pull_up(P2L_PIN);

	gpio_init(P2R_PIN);
	gpio_pull_up(P2R_PIN);

	// p3
	gpio_init(P3L_PIN);
	gpio_pull_up(P3L_PIN);

	gpio_init(P3R_PIN);
	gpio_pull_up(P3R_PIN);
	
	// p4
	gpio_init(P4L_PIN);
	gpio_pull_up(P4L_PIN);

	gpio_init(P4R_PIN);
	gpio_pull_up(P4R_PIN);


	int joy_x_avg = 0;
	int joy_y_avg = 0;

	while (true) {
		int joy_btn = !gpio_get(JOY_BTN_PIN);

		p1l_btn = !gpio_get(P1L_PIN);
		p1r_btn = !gpio_get(P1R_PIN);
		p2l_btn = !gpio_get(P2L_PIN);
		p2r_btn = !gpio_get(P2R_PIN);
		p3l_btn = !gpio_get(P3L_PIN);
		p3r_btn = !gpio_get(P3R_PIN);
		p4l_btn = !gpio_get(P4L_PIN);
		p4r_btn = !gpio_get(P4R_PIN);


		adc_select_input(JOY_X_PIN - 26);

		int joy_x = 0;
		int joy_y = 0;

		for (int i = 0; i < 256; i++)
			joy_x += -(adc_read() - 2048);

		adc_select_input(JOY_Y_PIN - 26);

		for (int i = 0; i < 256; i++)
			joy_y += -(adc_read() - 2048);

		srand(joy_x + joy_y + time_us_32());

		joy_x_avg = (joy_x_avg * 7 + joy_x) / 8;
		joy_y_avg = (joy_y_avg * 7 + joy_y) / 8;

		input_joy_x = joy_x_avg / 256;
		input_joy_y = joy_y_avg / 256;
		input_joy_btn = joy_btn;

		//printf("joy: btn=%i, x=%5i, y=%5i\n", input_joy_btn, input_joy_x, input_joy_y);

		task_sleep_ms(10);
	}
}

inline static int clamp(int x, int lo, int hi)
{
	if (x < lo)
		return lo;

	if (x > hi)
		return hi;

	return x;
}

static void reset_game(void)
{
	for (int x = 0; x < WIDTH; x++) {
		for (int y = 0; y < HEIGHT; y++) {
			grid[x][y] = -1;
		}
	}

	memcpy(worms, worms_init, sizeof worms);
}

/*
 * Outputs stuff to the screen as fast as possible.
 */
static void tft_task(void)
{
	uint32_t last_sync = time_us_32();
	int fps = 0;

	reset_game();

	while (true) {
		tft_fill(2);

		for (int x = 0; x < WIDTH; x++) {
			for (int y = 0; y < HEIGHT; y++) {
				int8_t owner = grid[x][y];

				if (owner < 0)
					continue;

				int8_t age = owner & 0x0f;
				owner >>= 4;

				if (owner >= 0)
					tft_draw_pixel(x, y, worms[owner].color);

				if (age)
					grid[x][y] = (owner << 4) | (age - 1);
			}
		}

		if (p1r_btn)
			worms[0].angle -= ANGLE_DELTA;

		if (p1l_btn)
			worms[0].angle += ANGLE_DELTA;

		if (p2r_btn)
			worms[1].angle -= ANGLE_DELTA;

		if (p2l_btn)
			worms[1].angle += ANGLE_DELTA;
		
		if (p3r_btn)
			worms[2].angle -= ANGLE_DELTA;

		if (p3l_btn)
			worms[2].angle += ANGLE_DELTA;
		
		if (p4r_btn)
			worms[3].angle -= ANGLE_DELTA;

		if (p4l_btn)
			worms[3].angle += ANGLE_DELTA;



		bool anybody_alive = false;

		for (int i = 0; i < NUM_WORMS; i++) {
			anybody_alive |= worms[i].alive;

			if (!worms[i].alive)
				continue;

			float hspd = worms[i].speed * cosf(worms[i].angle);
			float vspd = worms[i].speed * sinf(worms[i].angle);

			float x = worms[i].x += hspd;
			float y = worms[i].y += vspd;

			int nx[4] = {
				clamp(x + 0.5f, 0, WIDTH - 1),
				clamp(x + 0.5f, 0, WIDTH - 1),
				clamp(x - 0.5f, 0, WIDTH - 1),
				clamp(x - 0.5f, 0, WIDTH - 1),
		       	};

			int ny[4] = {
				clamp(y + 0.5f, 0, HEIGHT - 1),
				clamp(y - 0.5f, 0, HEIGHT - 1),
				clamp(y + 0.5f, 0, HEIGHT - 1),
				clamp(y - 0.5f, 0, HEIGHT - 1),
		       	};

			for (int j = 0; j < 4; j++) {
				int8_t owner = grid[nx[j]][ny[j]];
				int8_t age = owner & 0x0f;
				owner >>= 4;

				if ((owner >= 0) && (owner == i) && !age) {
					worms[i].alive = false;
					continue;
				}

				if ((owner >= 0) && (owner != i)) {
					worms[i].alive = false;
					continue;
				}

				grid[nx[j]][ny[j]] = (i << 4) | 0x0f;
			}

			if (worms[i].x < 0 || worms[i].x > WIDTH)
				worms[i].alive = false;

			if (worms[i].y < 0 || worms[i].y > HEIGHT)
				worms[i].alive = false;
		}

		if (!anybody_alive)
			reset_game();

		char buf[64];

		snprintf(buf, sizeof buf, "%i", fps);
		tft_draw_string_right(tft_width - 1, 0, GRAY, buf);

		tft_swap_buffers();
		task_sleep_ms(3);
		tft_sync();

		uint32_t this_sync = time_us_32();
		uint32_t delta = this_sync - last_sync;
		fps = 1 * 1000 * 1000 / delta;
		last_sync = this_sync;
	}
}

static void backlight_init(void)
{
	int slice = pwm_gpio_to_slice_num(TFT_LED_PWM);
	int chan = pwm_gpio_to_channel(TFT_LED_PWM);

	pwm_config conf = pwm_get_default_config();
	pwm_init(slice, &conf, false);
	pwm_set_clkdiv_int_frac(slice, 1, 0);
	pwm_set_wrap(slice, 99);
	pwm_set_chan_level(slice, chan, 100);
	pwm_set_enabled(slice, true);

	gpio_init(TFT_LED_PWM);
	gpio_set_dir(TFT_LED_PWM, GPIO_OUT);
	gpio_set_function(TFT_LED_PWM, GPIO_FUNC_PWM);
}

int main()
{
	stdio_usb_init();
	task_init();

	for (int i = 0; i < 30; i++) {
		if (stdio_usb_connected())
			break;

		sleep_ms(100);
	}

	adc_init();
	adc_gpio_init(JOY_X_PIN);
	adc_gpio_init(JOY_Y_PIN);
	adc_select_input(JOY_X_PIN);

	for (int i = 0; i < 16; i++)
		srand(adc_read() + random());

	backlight_init();
	tft_init();

	printf("Hello, have a nice and productive day!\n");

	multicore_launch_core1(task_run_loop);
	task_run_loop();
}
