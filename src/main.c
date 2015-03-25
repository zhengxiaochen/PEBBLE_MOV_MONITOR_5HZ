#include <pebble.h>  
#include <stdlib.h>
#define BFSIZE 100 //max 57
static const uint32_t AXIS_LOG_TAG = 0x5; 
enum {
  TODO_KEY_APPEND,
  TODO_KEY_DELETE,
  TODO_KEY_MOVE,
  TODO_KEY_TOGGLE_STATE,
  TODO_KEY_FETCH,
};
static Window *window;
static TextLayer *text_layer;
DictionaryIterator *iter;
Tuple *text_tuple;
char text_buffer[100];
char data_buffer[BFSIZE];
char *p;
char *ctime(const time_t *timep); 
int x[1024];
int y[1024];
int z[1024];
void out_sent_handler(DictionaryIterator *sent, void *context);
void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context);
void in_received_handler(DictionaryIterator *received, void *context);
void in_dropped_handler(AppMessageResult reason, void *context);
char ax[2];
char ay[2];
char az[2];
char aa[2];
char at[8];

//Define data structure for sending data. 
typedef struct {  
  uint32_t tag;                               //tag of the log
  DataLoggingSessionRef logging_session;      // logging session
  char value[];                               //string to write into the log
} AxisData;
static AxisData axis_datas; 

//Writing data to loggign session
static void count_axis(AxisData *axis_data, char v[]) {  
  strcpy(axis_data->value, v);  
  data_logging_log(axis_data->logging_session, &axis_data->value,1);
}

//collect data from accelerometer
int i=0;
static void data_handler(AccelData *data, uint32_t num_samples) {
  if (i%2!=0){
    i++;
  } else{
    int16_t angle = atan2_lookup(data[0].z, data[0].x) * 360 / TRIG_MAX_ANGLE - 270; // Calculate arm angle  
    time_t t = data[0].timestamp/1000;   //Change millisecond to readable data time string
    struct tm * timeinfo=gmtime(&t);
    char timebuffer[BFSIZE];
    char timebuf[15];
    char timebuffer_HHMM[5];
    char timebuffer_SS[2];
    char timebuffer_MMDD[25];
  
    int  end = data[0].timestamp%1000;   
    strftime( timebuffer,BFSIZE,"%H:%M:%S %Y/%m/%d", timeinfo); //"%Y/%m/%d %H:%M:%S"
    strftime( timebuf,15,"%Y%m%d%H%M%S", timeinfo); //"%Y/%m/%d %H:%M:%S"
  
    strftime(timebuffer_HHMM,10,"%H:%M", timeinfo);
    strftime(timebuffer_SS,5,"%S", timeinfo);
    strftime(timebuffer_MMDD,25,"%b %d", timeinfo);
  
    //Text buffer for  showing on the screen
    //snprintf(text_buffer, 100, "Time: %s \n \n Acc_X: %d mG \n Acc_Y: %d mG \n Acc_Z: %d mG \n Arm Angle: %d ", timebuffer, data[0].x, data[0].y, data[0].z, angle); 
    snprintf(text_buffer, 50, "%s \n \n %s \n %s ", timebuffer_MMDD, timebuffer_HHMM,timebuffer_SS); //hide data and show time only  
    //snprintf(text_buffer, sizeof(text_buffer), "N X,Y,Z\nxxxxxxxxxxx");
    snprintf(data_buffer, BFSIZE, "{'x':%d,'y':%d,'z':%d,'a':%d,'t':\"%s%03d\"}", data[0].x, data[0].y, data[0].z, angle, timebuf,end);
    
    AxisData *axis_data0 = &axis_datas;          
    count_axis(axis_data0,data_buffer);             //Writing data to logging session  
    text_layer_set_text(text_layer, text_buffer);   //Show data on the screen  
    if (i==300){
      //Finish a logging session and creat a new one
      AxisData *axis_data = &axis_datas;
      data_logging_finish(axis_data->logging_session); //Finish the old session
      axis_data->logging_session = data_logging_create(AXIS_LOG_TAG, DATA_LOGGING_BYTE_ARRAY, BFSIZE, false); //Create a new session every one minute
      i=0;
    }else{
      i++;
    }    
  }
  
}

static void init_axis_datas(Window *window) { 
    AxisData *axis_data = &axis_datas;   
    axis_data->logging_session = data_logging_create(AXIS_LOG_TAG, DATA_LOGGING_BYTE_ARRAY, BFSIZE, false);   
}

static void deinit_axis_datas(void) {  
  AxisData *axis_data = &axis_datas;
  data_logging_finish(axis_data->logging_session);    
}

static void window_load(Window *window) {
  init_axis_datas(window);  
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);  
  //text_layer = text_layer_create((GRect) { .origin = { 5, 5 }, .size = { bounds.size.w-0, 100 } });  
  text_layer = text_layer_create((GRect) { .origin = { 5, 5 }, .size = { bounds.size.w, bounds.size.h } });  
  text_layer_set_overflow_mode(text_layer, GTextOverflowModeWordWrap);
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);  
  //set text layer format. hide the data and show time only
  //text_layer_set_size (text_layer, {bounds.size.w, bounds.size.h});
  text_layer_set_background_color (text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorWhite);
  text_layer_set_font (text_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  //***till here
  layer_add_child(window_layer, text_layer_get_layer(text_layer));     
}

static void window_unload(Window *window) { 
  text_layer_destroy(text_layer);
  deinit_axis_datas();
}

static void init(void) {
  window = window_create(); 
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  }); 
  ///*******hown time only
   window_set_background_color (window, GColorBlack); 
  //*****
  
  const bool animated = true;
  window_stack_push(window, animated);
   app_message_register_inbox_received(in_received_handler);
   app_message_register_inbox_dropped(in_dropped_handler);
   app_message_register_outbox_sent(out_sent_handler);
   app_message_register_outbox_failed(out_failed_handler);
   app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());  
   // Subscribe to the accelerometer data service
   int num_samples = 1;
   accel_data_service_subscribe(num_samples, data_handler);
   // Choose update rate
   accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
}

static void deinit(void) {
  accel_data_service_unsubscribe();  
  //window_destroy(window);  //avoid free memory twice.
}

int main(void) {
  init();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
  app_event_loop();
  deinit();
}

//from APPMESSAGE.C
 void out_sent_handler(DictionaryIterator *sent, void *context) {
   APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message worked!");
   text_buffer[0] = 0;
   p = text_buffer;
 }
 void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
   APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Failed to Send!");
 }
 void in_received_handler(DictionaryIterator *received, void *context) {
   text_tuple = dict_find(received, TODO_KEY_APPEND);         
   if(text_tuple){
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Text: %s", text_tuple->value->cstring);
   }
 }
 void in_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Dropped!");
 }


//*************************************************BACKUP CODES**************

/*Send byte array only***
  ax[0] = (accel.x >> 8) & 0xFF;
  ax[1] = accel.x & 0xFF;
  ay[0] = (accel.y >> 8) & 0xFF;
  ay[1] = accel.y & 0xFF;
  az[0] = (accel.z >> 8) & 0xFF;
  az[1] = accel.z & 0xFF;
  aa[0] = (angle >> 8) & 0xFF;
  aa[1] = angle & 0xFF;
  at[0] = (accel.timestamp >> 56) & 0xFF;
  at[1] = (accel.timestamp >> 48) & 0xFF;  
  at[2] = (accel.timestamp >> 40) & 0xFF;
  at[3] = (accel.timestamp >> 32) & 0xFF;   
  at[4] = (accel.timestamp >> 24) & 0xFF;
  at[5] = (accel.timestamp >> 16) & 0xFF;
  at[6] = (accel.timestamp >> 8) & 0xFF;
  at[7] = accel.timestamp & 0xFF;
  //Copy arrays in individually.
  memcpy(data_buffer, ax, 2);
  memcpy(data_buffer + 2, ay, 2);
  memcpy(data_buffer + 4, az, 2);
  memcpy(data_buffer + 6, aa, 2);
  memcpy(data_buffer + 8, at, 8);  
*/

//void timer_sendlog(void *data){   //Function for defining the period of sending logging session
//  AxisData *axis_data = &axis_datas;
//  data_logging_finish(axis_data->logging_session); //Finish the old session
//  axis_data->logging_session = data_logging_create(AXIS_LOG_TAG, DATA_LOGGING_BYTE_ARRAY, BFSIZE, false); //Create a new session every one minute
//  timerl = app_timer_register(30000, timer_sendlog, NULL); 
//}
