#include <pebble.h>
#include "draw_arc.h"

#define MINUTES_CIRCLE_RADIUS 22
#define minutesColor GColorFromRGB(255, 255, 85)
#define BACKGROUND_COLOR GColorFromRGB(85, 85, 170)

static Window *s_main_window;
static Layer *s_canvas_layer;

static GPoint s_center;
static int s_radius = 45;

static char s_str_hours[3];
static char s_str_minutes[3];
static char s_str_day[3];
static char s_str_month[4];
static int s_current_minutes;
static int s_current_hours;
static int s_battery_percent;
static GColor s_battery_colors[5];

static GFont s_font_minutes;
static GFont s_font_hours;
static GFont s_font_small;

static bool s_bluetooth_connected = true;

extern int angle_90;


/************************************ UI **************************************/


static void tick_handler(struct tm *tick_time, TimeUnits changed)
{
	// Store time
	s_current_hours = tick_time->tm_hour;
	s_current_minutes = tick_time->tm_min;
	snprintf(s_str_hours, 3, "%02d", s_current_hours);
	snprintf(s_str_minutes, 3, "%02d", s_current_minutes);
	snprintf(s_str_day, 3, "%02d", tick_time->tm_mday);
	snprintf(s_str_month, 4, "/%02d", tick_time->tm_mon+1);

	// Redraw
	if(s_canvas_layer) {
		layer_mark_dirty(s_canvas_layer);
	}
}


static void draw_hours (GContext *ctx)
{
	graphics_context_set_text_color(ctx, BACKGROUND_COLOR);
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_circle(ctx, s_center, s_radius);
	graphics_draw_text(ctx, s_str_hours, s_font_hours, GRect(s_center.x-s_radius, s_center.y-33, s_radius*2, s_radius*2), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}


static void draw_minutes (GContext *ctx)
{
	graphics_context_set_text_color(ctx, BACKGROUND_COLOR);
	float minute_angle = TRIG_MAX_ANGLE * s_current_minutes / 60;
	graphics_draw_arc(ctx, s_center, s_radius+12, 10, -angle_90, minute_angle-angle_90, minutesColor);
	int min_x = s_center.x + sin_lookup(minute_angle) * (s_radius+5) / TRIG_MAX_RATIO;
	int min_y = s_center.y - cos_lookup(minute_angle) * (s_radius+5) / TRIG_MAX_RATIO;
	graphics_context_set_fill_color(ctx, minutesColor);
	graphics_fill_circle(ctx, GPoint(min_x, min_y), MINUTES_CIRCLE_RADIUS);
	graphics_draw_text(ctx, s_str_minutes, s_font_minutes, GRect(min_x-MINUTES_CIRCLE_RADIUS+1, min_y-MINUTES_CIRCLE_RADIUS+3, MINUTES_CIRCLE_RADIUS*2, MINUTES_CIRCLE_RADIUS*2), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}


static void draw_background (GContext *ctx)
{
	graphics_context_set_fill_color(ctx, BACKGROUND_COLOR); // TODO Settings
	graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
	graphics_context_set_antialiased(ctx, true);
}


static void draw_date (GContext *ctx)
{
	GRect dateRect = GRect(2, 136, 144, 32);
	GSize daySize = graphics_text_layout_get_content_size(s_str_day, s_font_minutes, dateRect, GTextOverflowModeWordWrap, GTextAlignmentLeft);
	graphics_context_set_text_color(ctx, GColorFromRGB(170, 170, 255));
	graphics_draw_text(ctx, s_str_day, s_font_minutes, dateRect, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
	graphics_draw_text(ctx, s_str_month, s_font_small, GRect(4+daySize.w, 146, 50, 50), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

static void draw_battery (GContext *ctx)
{
	int indicators = 0;
	if (s_battery_percent > 90) {
		indicators = 5;
	} else if (s_battery_percent > 70) {
		indicators = 4;
	} else if (s_battery_percent > 50) {
		indicators = 3;
	} else if (s_battery_percent > 30) {
		indicators = 2;
	} else if (s_battery_percent > 10) {
		indicators = 1;
	}
	for (int i=0 ; i<indicators ; i++)
	{
		graphics_context_set_fill_color(ctx, s_battery_colors[i]);
		graphics_fill_rect(ctx, GRect(i*29, 0, 27, 5), 0, GCornerNone);
	}	
}


static const GPathInfo BLUETOOTH_SYMBOL_PATH_INFO = {
  .num_points = 6,
  .points = (GPoint []) {{0, 5}, {10, 15}, {5, 20}, {5, 0}, {10, 5}, {0, 15}}
};
static GPath *s_bluetooth_path_ptr = NULL;

static void draw_bluetooth (GContext *ctx)
{
	if (s_bluetooth_connected)
		return;
	
	graphics_context_set_fill_color(ctx, GColorFromRGB(255, 0, 85));
	graphics_fill_circle(ctx, GPoint(20, 25), 16);
	graphics_context_set_stroke_color(ctx, GColorWhite);
	graphics_context_set_stroke_width(ctx, 2);
	gpath_move_to(s_bluetooth_path_ptr, GPoint(15, 14));
	gpath_draw_outline_open (ctx, s_bluetooth_path_ptr);
}


static void update_proc(Layer *layer, GContext *ctx)
{
	draw_background(ctx);
	draw_date(ctx);
	draw_battery(ctx);
	draw_bluetooth(ctx);
	draw_hours(ctx);
	draw_minutes(ctx);
}


static void handle_bluetooth (bool connected)
{
	s_bluetooth_connected = connected;
}


static void handle_battery (BatteryChargeState charge_state)
{
	/*
	if (charge_state.is_charging) {
		snprintf(battery_text, sizeof(battery_text), "charging");
	} else {
		charge_state.charge_percent;
	}
	text_layer_set_text(s_battery_layer, battery_text);
	*/
	s_battery_percent = charge_state.charge_percent;
}


static void window_load (Window *window)
{
	Layer *window_layer = window_get_root_layer(window);
	GRect window_bounds = layer_get_bounds(window_layer);

	s_center = grect_center_point(&window_bounds);
	s_canvas_layer = layer_create(window_bounds);
	layer_set_update_proc(s_canvas_layer, update_proc);
	layer_add_child(window_layer, s_canvas_layer);
	
	s_font_minutes = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RIGHTEOUS_30));
	s_font_hours = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RIGHTEOUS_54));
	s_font_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RIGHTEOUS_20));

	s_battery_colors[0] = GColorRed;
	s_battery_colors[1] = GColorOrange;
	s_battery_colors[2] = GColorChromeYellow;
	s_battery_colors[3] = GColorInchworm;
	s_battery_colors[4] = GColorGreen;
	
	s_bluetooth_path_ptr = gpath_create(&BLUETOOTH_SYMBOL_PATH_INFO);

	battery_state_service_subscribe(handle_battery);
	bluetooth_connection_service_subscribe(handle_bluetooth);
	handle_battery(battery_state_service_peek());
}


static void window_unload (Window *window)
{
	layer_destroy(s_canvas_layer);
	fonts_unload_custom_font(s_font_minutes);
	fonts_unload_custom_font(s_font_hours);
	fonts_unload_custom_font(s_font_small);

	battery_state_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();	
}


/** APP **/


static void init()
{
	srand(time(NULL));

	time_t t = time(NULL);
	struct tm *time_now = localtime(&t);
	tick_handler(time_now, MINUTE_UNIT);

	s_main_window = window_create();
	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});
	window_stack_push(s_main_window, true);
	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}


static void deinit()
{
	window_destroy(s_main_window);
}


int main()
{
	init();
	app_event_loop();
	deinit();
}
