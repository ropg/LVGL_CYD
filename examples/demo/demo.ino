#include <LVGL_CYD.h>

/* Takes position of USB connector relative to screen. These are synonyms
   as defined in LVGL_CYD.h:

      #define USB_DOWN  LV_DISPLAY_ROTATION_0
      #define USB_RIGHT LV_DISPLAY_ROTATION_90
      #define USB_UP    LV_DISPLAY_ROTATION_180
      #define USB_LEFT  LV_DISPLAY_ROTATION_270
*/ 
#define SCREEN_ORIENTATION USB_LEFT

lv_obj_t * btn_exit;
lv_obj_t * lbl_header;

void setup() {

  LVGL_CYD::begin(SCREEN_ORIENTATION);

  // LVGL has multiple layers, one below and two above the 'active screen'.
  // I use the 'bottom' layer for the background color and keep all further
  // objects on transparent background. I use the 'top' layer for the screen
  // header and the exit button.

  // bottom layer opaque and white
  lv_obj_set_style_bg_opa(lv_layer_bottom(), LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(lv_layer_bottom(), lv_color_white(), LV_PART_MAIN);

  // btn_exit is not really an LVGL button, just an obj made clickable.
  // It holds the little 'x' to exit back to main screen, but is bigger
  // because the little 'x' would be near impossible to hit.
  btn_exit = lv_obj_create(lv_layer_top());
  lv_obj_clear_flag(btn_exit, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(btn_exit, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(btn_exit, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(btn_exit, 0, LV_PART_MAIN);
  lv_obj_set_size(btn_exit, 40, 40);
  lv_obj_align(btn_exit, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_event_cb(btn_exit, [](lv_event_t * e) -> void {
    go_main();
  }, LV_EVENT_CLICKED, NULL);

  // holds actual little 'x', lives inside btn_exit
  lv_obj_t * lbl_exit_symbol = lv_label_create(btn_exit);
  lv_obj_set_style_text_font(lbl_exit_symbol, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_set_style_text_align(lbl_exit_symbol, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(lbl_exit_symbol, LV_SYMBOL_CLOSE);
  lv_obj_align(lbl_exit_symbol, LV_ALIGN_TOP_RIGHT, 5, -10);  

  // page header
  lbl_header = lv_label_create(lv_layer_top());
  lv_obj_set_style_text_font(lbl_header, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_align(lbl_header, LV_ALIGN_TOP_LEFT, 5, 3);

  go_main();

}

void loop() {
  lv_task_handler();  // let the GUI do its work
}


// creates a new obj to use as base screen, and set some properties, such as flex
// vertical arrangement, centered horizontally, transparent background, etc.
lv_obj_t * new_screen(lv_obj_t * parent) {

  lv_obj_t * obj = lv_obj_create(parent);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(obj, 20, LV_PART_MAIN);
  lv_obj_set_style_pad_row(obj, 10, LV_PART_MAIN);

  return obj;
}


//
// main screen
//

lv_obj_t * scr_main = nullptr;

void go_main() {
  if (!scr_main) {
    // if screen doesn't exist yet, create it and its children
    scr_main = new_screen(NULL);

    lv_obj_set_size(scr_main, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));

    lv_obj_t * btn_led = lv_button_create(scr_main);
    lv_obj_t * lbl_led = lv_label_create(btn_led);
    lv_label_set_text(lbl_led, "RGB LED color");
    lv_obj_add_event_cb(btn_led, [](lv_event_t * e) -> void {
      go_led();
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_bl = lv_button_create(scr_main);
    lv_obj_t * lbl_bl = lv_label_create(btn_bl);
    lv_label_set_text(lbl_bl, "Backlight brightness");
    lv_obj_add_event_cb(btn_bl, [](lv_event_t * e) -> void {
      go_bl();
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_touch = lv_button_create(scr_main);
    lv_obj_t * lbl_touch = lv_label_create(btn_touch);
    lv_label_set_text(lbl_touch, "Touch indicator");
    lv_obj_add_event_cb(btn_touch, [](lv_event_t * e) -> void {
      go_touch();
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_rotate = lv_button_create(scr_main);
    lv_obj_t * lbl_rotate = lv_label_create(btn_rotate);
    lv_label_set_text(lbl_rotate, "Rotate display");
    lv_obj_add_event_cb(btn_rotate, [](lv_event_t * e) -> void {
      go_rotate();
    }, LV_EVENT_CLICKED, NULL);
  
  }

  lv_label_set_text(lbl_header, "LVGL_CYD demo");   // screen header
  lv_obj_add_flag(btn_exit, LV_OBJ_FLAG_HIDDEN);    // disable exit, already at main screen
  lv_screen_load(scr_main);                         // Tell LVGL to load as active screen

}


//
// RGB LED color screen
//

lv_obj_t * scr_led;

// global because referenced in the callback
lv_obj_t * slider_led[3];
lv_obj_t * sw_led_true;

void go_led() {

  if (!scr_led) {
    scr_led = new_screen(NULL);

    // switch between true colors and max brightness
    lv_obj_t * sw_container = lv_obj_create(scr_led);
    lv_obj_set_style_bg_opa(sw_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(sw_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(sw_container, 0, 0);
    lv_obj_set_height(sw_container, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(sw_container, 0, 0);
    lv_obj_set_flex_flow(sw_container, LV_FLEX_FLOW_ROW);
    lv_obj_t * lbl_max = lv_label_create(sw_container);
    lv_label_set_text(lbl_max, "max");
    sw_led_true = lv_switch_create(sw_container);
    lv_obj_add_state(sw_led_true, LV_STATE_CHECKED);
    lv_obj_set_size(sw_led_true, 40, 20);
    lv_obj_add_event_cb(sw_led_true, scr_led_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t * lbl_true = lv_label_create(sw_container);
    lv_label_set_text(lbl_true, "true");
    lv_obj_set_style_pad_column(sw_container, 5, 0);

    // colors for the 4 sliders
    lv_color_t color[3] {
      lv_color_make(255, 0, 0),
      lv_color_make(0, 200, 0),  // Full-on green looks too yellow on display
      lv_color_make(0, 0, 255)
    };

    // set up the sliders
    for (int n = 0; n < 3; n++) {
      slider_led[n] = lv_slider_create(scr_led);
      lv_obj_set_width(slider_led[n], lv_pct(80));
      lv_obj_set_style_margin_top(slider_led[n], 25, LV_PART_MAIN);
      lv_obj_set_style_bg_color(slider_led[n], color[n], LV_PART_MAIN);  // set the track color
      lv_obj_set_style_bg_color(slider_led[n], color[n], LV_PART_INDICATOR); // set the indicator color
      lv_obj_set_style_bg_color(slider_led[n], lv_color_lighten(color[n], LV_OPA_30), LV_PART_KNOB); // set the knob color
      lv_slider_set_range(slider_led[n], 0, 255);
      lv_obj_add_event_cb(slider_led[n], scr_led_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

  }

  lv_label_set_text(lbl_header, "RGB LED color");
  lv_obj_clear_flag(btn_exit, LV_OBJ_FLAG_HIDDEN);
  lv_screen_load(scr_led);

}

// callback when any of the values change
static void scr_led_cb(lv_event_t * e) {
  LVGL_CYD::led(
    lv_slider_get_value(slider_led[0]),
    lv_slider_get_value(slider_led[1]),
    lv_slider_get_value(slider_led[2]),
    lv_obj_has_state(sw_led_true, LV_STATE_CHECKED)
  );
}


//
// LBacklight brightness screen
//

lv_obj_t * scr_bl;

// These need to be global as they are referenced in the callback.
lv_obj_t * slider_bl;

void go_bl() {

  if (!scr_bl) {
    scr_bl = new_screen(NULL);

    slider_bl = lv_slider_create(scr_bl);
    lv_obj_set_width(slider_bl, lv_pct(80));
    lv_slider_set_range(slider_bl, 0, 255);
    lv_obj_add_event_cb(slider_bl, [](lv_event_t * e) -> void {
      LVGL_CYD::backlight(lv_slider_get_value(slider_bl));
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // turn on backlight initially
    lv_slider_set_value(slider_bl, 255, LV_ANIM_OFF);

    // set backlight to initial slider value
    LVGL_CYD::backlight(lv_slider_get_value(slider_bl));
  }

  lv_label_set_text(lbl_header, "Backlight brightness");
  lv_obj_clear_flag(btn_exit, LV_OBJ_FLAG_HIDDEN);
  lv_screen_load(scr_bl);

}


//
// scr_touch
//

lv_obj_t * scr_touch;

// These need to be global as they are used in the callback.
lv_obj_t * horizontal;
lv_obj_t * vertical;

void go_touch() {

  if (!scr_touch) {

    scr_touch = new_screen(NULL);
    lv_obj_set_layout(scr_touch, LV_LAYOUT_NONE);         // need to place children (the crosshairs) manually
    lv_obj_set_style_pad_top(scr_touch, 0, LV_PART_MAIN);
    lv_obj_add_flag(scr_touch, LV_OBJ_FLAG_CLICKABLE);

    horizontal = lv_obj_create(scr_touch);
    lv_obj_set_style_border_width(horizontal, 0, 0);
    lv_obj_set_style_radius(horizontal, 0, LV_PART_MAIN);
    lv_obj_set_size(horizontal, lv_pct(100), 3);
    lv_obj_set_style_bg_color(horizontal, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(horizontal, LV_OBJ_FLAG_CLICKABLE);

    vertical = lv_obj_create(scr_touch);
    lv_obj_set_style_border_width(vertical, 0, 0);
    lv_obj_set_style_radius(vertical, 0, LV_PART_MAIN);
    lv_obj_set_size(vertical, 3, lv_pct(100));
    lv_obj_set_style_bg_color(vertical, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(vertical, LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_add_event_cb(scr_touch, [](lv_event_t * e) -> void {
      if (lv_event_get_code(e) == LV_EVENT_PRESSED || lv_event_get_code(e) == LV_EVENT_PRESSING) {
        lv_point_t point;
        lv_indev_get_point(lv_indev_get_act(), &point); // Get the current touch point
        lv_obj_set_y(horizontal, point.y);
        lv_obj_set_x(vertical, point.x);
        lv_obj_clear_flag(horizontal, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(vertical, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_exit, LV_OBJ_FLAG_HIDDEN);
      } else if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        lv_obj_add_flag(horizontal, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(vertical, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_exit, LV_OBJ_FLAG_HIDDEN);
      }
    }, LV_EVENT_ALL, NULL);
  }

  lv_obj_add_flag(horizontal, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(vertical, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(btn_exit, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(lbl_header, "Touch indicator");
  lv_screen_load(scr_touch);

}

//
// Rotate
//

void go_rotate() {

  lv_disp_t * display = lv_disp_get_default();
  lv_display_rotation_t rotation = lv_display_get_rotation(display);
  if (rotation == USB_LEFT) lv_display_set_rotation(display, USB_DOWN);
  else if (rotation == USB_DOWN) lv_display_set_rotation(display, USB_RIGHT);
  else if (rotation == USB_RIGHT) lv_display_set_rotation(display, USB_UP);
  else lv_display_set_rotation(display, USB_LEFT);

}
