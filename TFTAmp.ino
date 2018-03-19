#include <TEA5767.h>
#define USE_FAST_PINIO

#include <RTClib.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <SWTFT.h> // Hardware-specific library
#include <TouchScreen.h>
#include <TDA7313.h>
#include <stdio.h>


static int serial_fputchar(const char ch, FILE *stream) { Serial.write(ch); return ch; }
static FILE *serial_stream = fdevopen(serial_fputchar, NULL);

#define RGB565CONVERT(red, green, blue) \
(uint16_t)( (( red >> 3 ) << 11 ) | (( green >> 2 ) << 5 ) | ( blue >> 3 ) )

#define YP A1
#define XM A2
#define YM 7
#define XP 6

#define TS_MINX 100
#define TS_MINY 940
#define TS_MAXX 904
#define TS_MAXY 93

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
SWTFT tft;

#define LCD_CS A3
#define LCD_CD A2
#define LCD_WR A1
#define LCD_RD A0
#define LCD_RESET 10

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF


#define MINPRESSURE 10
#define MAXPRESSURE 1000

#define BG_COLOR BLACK

class Button {
	private:
		uint16_t      _outline_color, _fill_color, _text_color;
		char _label[10];
		int16_t       _x, _y; // Coordinates of top-left corner
		int16_t				_w, _h;
		uint8_t       _text_size;
		unsigned long  _pressTime;
	
	public:
	int tag = -1;
	boolean selected = false;
	boolean visible = false;
	boolean down = false;
	boolean	state = false;

	void init(char *label, int atag, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t outline, uint16_t fill, uint16_t textcolor, bool avisible=false){
		tag = atag;
		visible = avisible;
		_x = x; _y = y;  _w = w;	_h = h;
		_outline_color = outline; _fill_color = fill;	
		_text_color = textcolor;
		_text_size = 2;
		strncpy(_label, label, 9);
	}
	
	
	void draw() {
		uint16_t fill, outline, text;
		bool inverted =selected||state;
		if(!inverted) {
			fill    = _fill_color;
			outline = _outline_color;
			text    = _text_color;
		} else {
			fill    = _text_color;
			outline = _outline_color;
			text    = _fill_color;
		}

		visible = true;

		uint8_t r = min(_w, _h) / 6; 
	  tft.fillRoundRect(_x, _y, _w, _h, r, fill);
	  tft.drawRoundRect(_x, _y, _w, _h, r, outline);

		if (down){
			for (uint8_t i=1; i<=3; i++){
				tft.drawRoundRect(_x+i, _y+i, _w-i, _h-i, r, outline);
			}
			
		}

		tft.setCursor(_x + (_w/2) - (strlen(_label) * 3 * _text_size), _y + (_h/2) - (4 * _text_size));
		tft.setTextColor(text);
		tft.setTextSize(_text_size);
		tft.print(_label);
	}
	
	void gotoXY(int16_t x, int16_t y, bool do_show=false){
		if (x!=_x || y!=_y){
			if (visible)
				hide();
			_x = x;
			_y = y;
		}
		if (do_show){
			show();
		}
	}
	
	void show(bool select = false){
		selected = select;
		visible = true;
		draw();
	}
		

	void hide(){
		visible = false;
	  tft.fillRect(_x-1, _y-1, _w+2, _h+2,  BG_COLOR);
	}
	
	
	boolean contains(int16_t x, int16_t y) {
		return ((x >= _x) && (x < (_x + _w)) && (y >= _y) && (y < (_y + _h)));
}
	
	
	void click(uint8_t flag) {
		static unsigned long last_repeat_time;
		if (flag != state) {
			state=flag;
			if (state) {
					last_repeat_time = millis();
					draw();
					button_click(tag);
			} else {
				draw();
			}
		} else {
			if (state && (millis()-last_repeat_time)>50){
				last_repeat_time = millis();
				button_click(tag);
			
			}
		}
		_pressTime = millis();
	}
	
};



TDA7313 tda(0x44);
DS1307 rtc;
TEA5767 Radio;


	

/*
#define BTN_UP				0
#define BTN_DOWN			1
#define BTN_VOL				2
#define BTN_BASS			3
#define BTN_TREBLE		4
#define BTN_SETUP			5
#define BTN_SOUND_SET 6
#define BTN_LOUD			7


#define GUI_MODE_IDLE					0
#define GUI_MODE_SET_VOLUME		1
#define GUI_MODE_SET_BASS			2
#define GUI_MODE_SET_TREBLE		3
*/


enum GUI_BUTTONS {
		BTN_UP, BTN_DOWN, BTN_VOL, BTN_BASS, BTN_TREBLE, BTN_LOUD, BTN_EQ, BTN_SAVE,
		BTN_IN_1, BTN_IN_2, BTN_IN_3,
		BTN_IN_MUTE, BTN_SETUP,
		BUTTONS_COUNT	// -- Должна быть последняя!!
};
enum GUI_MODES {GUI_MODE_IDLE, GUI_MODE_SET_VOLUME, GUI_MODE_SET_BASS, GUI_MODE_SET_TREBLE};


class UI{
	private:
//		Adafruit_GFX *_gfx;
		uint8_t _gui_mode = GUI_MODE_IDLE;
		unsigned long _mode_time = 0;
	public:
		Button buttons[BUTTONS_COUNT];
		unsigned long last_click_time;
		uint8_t clock_h, clock_m, clock_s;
		DateTime now;
		
		UI(){
			clock_h = 0;
			clock_m = 0;
			clock_s = 0;
			buttons[BTN_UP].init(		(char*)"UP",				BTN_UP, 255, 0,  65, 70 , WHITE, BLUE, WHITE, true);
			buttons[BTN_DOWN].init(	(char*)"DN",				BTN_DOWN, 255, 100,  65, 70 , WHITE, BLUE, WHITE, true);

			buttons[BTN_IN_1].init(	(char*)"RADIO",			BTN_IN_1,		0,	210,  60, 30,	WHITE, RGB565CONVERT(100,200,100), WHITE, true);
			buttons[BTN_IN_2].init(	(char*)"IN 1",			BTN_IN_2,		65,	210,  60, 30,	WHITE, RGB565CONVERT(100,200,100), WHITE, true);
			buttons[BTN_IN_3].init(	(char*)"IN 2",			BTN_IN_3,		130,	210,  60, 30,	WHITE, RGB565CONVERT(100,200,100), WHITE, true);
			buttons[BTN_SETUP].init((char*)"SET",				BTN_SETUP,	195, 210,  60, 30 , WHITE, RED, WHITE, true); 
			buttons[BTN_EQ].init(		(char*)"EQ",				BTN_EQ, 255, 210,  65, 30,	WHITE, BLUE, WHITE, true); 
			


			buttons[BTN_VOL].init(	(char*)"VOL",				BTN_VOL,		0,	210,  60, 30,	WHITE, BLUE, WHITE);
			buttons[BTN_TREBLE].init((char*)"TREB",			BTN_TREBLE, 65,	210,  60, 30 , WHITE, BLUE, WHITE);
			buttons[BTN_BASS].init(	(char*)"BASS",			BTN_BASS,		130, 210,  60, 30,	WHITE, BLUE, WHITE);
			buttons[BTN_LOUD].init(	(char*)"LOUD",			BTN_LOUD,		195, 210,  60, 30 , RGB565CONVERT(150,150,150) , BLUE, WHITE);  buttons[BTN_LOUD].down = true;
			buttons[BTN_SAVE].init(	(char*)"SAVE",			BTN_SAVE,		260, 210,  60, 30 , WHITE, RED, WHITE);

		}
		
		uint8_t setMode(uint8_t mode){
			printf("Set mode from %u to %u\n", _gui_mode, mode);
			if (_gui_mode!=mode){
				if (mode==GUI_MODE_IDLE){
					buttons[BTN_VOL].hide();
					buttons[BTN_TREBLE].hide();
					buttons[BTN_BASS].hide();
					buttons[BTN_SAVE].hide();
					buttons[BTN_LOUD].hide();
					buttons[BTN_IN_1].show(1);
					buttons[BTN_IN_2].show();
					buttons[BTN_IN_3].show();
					buttons[BTN_SETUP].show();
					buttons[BTN_EQ].show();
				} else if (mode==GUI_MODE_SET_VOLUME){
					buttons[BTN_IN_1].hide();
					buttons[BTN_IN_2].hide();
					buttons[BTN_IN_3].hide();
					buttons[BTN_SETUP].hide();
					buttons[BTN_EQ].hide();
					buttons[BTN_VOL].show(1);	
					buttons[BTN_TREBLE].show();	
					buttons[BTN_BASS].show();	
					buttons[BTN_LOUD].show();
					buttons[BTN_SAVE].show();
				} else if (mode==GUI_MODE_SET_TREBLE){
					buttons[BTN_IN_1].hide();
					buttons[BTN_IN_2].hide();
					buttons[BTN_IN_3].hide();
					buttons[BTN_SETUP].hide();
					buttons[BTN_EQ].hide();
					buttons[BTN_VOL].show();
					buttons[BTN_TREBLE].show(1);
					buttons[BTN_BASS].show();	
					buttons[BTN_LOUD].show();
					buttons[BTN_SAVE].show();
				} else if (mode==GUI_MODE_SET_BASS){
					buttons[BTN_IN_1].hide();
					buttons[BTN_IN_2].hide();
					buttons[BTN_IN_3].hide();
					buttons[BTN_SETUP].hide();
					buttons[BTN_EQ].hide();
					buttons[BTN_VOL].show();	
					buttons[BTN_TREBLE].show();	
					buttons[BTN_BASS].show(1);	
					buttons[BTN_LOUD].show();
					buttons[BTN_SAVE].show();
				}
				_gui_mode = mode;
				_mode_time = millis();
			}
			return mode;
		}

		uint8_t getMode(){
			return _gui_mode;
		}
		
	
		void show_clock(){
			char stime[10];
			tft.setTextSize(3); 
			tft.setTextColor(WHITE, BG_COLOR);
			sprintf(stime, "%02u:%02u", now.hour(),now.minute());
			tft.setCursor(5,20);
			tft.print(stime);
			
		}
		
		void update(){	
			tft.fillScreen(BLACK);
			tft.fillScreen(BLACK);
			for (uint8_t _i = 0; _i<BUTTONS_COUNT; _i++){
				if (buttons[_i].visible){
					buttons[_i].draw();
				}
			}
		}

		void idle(){
			static uint32_t show_time;
			if (millis()-show_time>1000){
				now = rtc.now();
				show_time=millis();
				show_clock();
			}
			if ((millis()-_mode_time)>10000){
				switch (_gui_mode){
					case GUI_MODE_SET_VOLUME:
					case GUI_MODE_SET_TREBLE:
					case GUI_MODE_SET_BASS:
						setMode(GUI_MODE_IDLE);
						break;
					
				}
			}
		}
		
		int16_t getButtonIndexByXY(int16_t x, int16_t y){
			for (uint8_t i=0; i<BUTTONS_COUNT; i++){
				if (buttons[i].visible && buttons[i].contains(x, y)){
					return i;
				}
			}
			return -1;
		}

	
		void unClickAll(){
			for (int16_t i=0; i<BUTTONS_COUNT; i++){
				if (buttons[i].state){
					printf("unclick %u\n", i);
					buttons[i].click(0);
				}
			}
		}
			
	void update_time(){
		_mode_time = millis();
	}
				
};

//UI ui(&tft);
UI ui;

void button_click(uint8_t tag){
	ui.last_click_time = millis();
	printf("Click button %u\n", tag);
	uint8_t current_mode = ui.getMode();
	if (tag==BTN_EQ){																		// EQ
		ui.setMode(GUI_MODE_SET_VOLUME);
	} else if (tag==BTN_UP){														// UP
		if (current_mode==GUI_MODE_IDLE || current_mode==GUI_MODE_SET_VOLUME) {
			tda.vol_plus();
		} else if (current_mode==GUI_MODE_SET_TREBLE) {
			tda.treble_plus();
		} else if (current_mode==GUI_MODE_SET_BASS) {
			tda.bass_plus();
		}
	} else if (tag==BTN_DOWN){											//	DOWN
		if (current_mode==GUI_MODE_IDLE || current_mode==GUI_MODE_SET_VOLUME) {
			tda.vol_minus();
		} else if (current_mode==GUI_MODE_SET_TREBLE) {
			tda.treble_minus();
		} else if (current_mode==GUI_MODE_SET_BASS){
			tda.bass_minus();
		}
	} else if (tag==BTN_VOL) {							// Vol
		ui.setMode(GUI_MODE_SET_VOLUME);
	} else if (tag==BTN_TREBLE) {						// Treble
		ui.setMode(GUI_MODE_SET_TREBLE);
	} else if (tag==BTN_BASS) {							// Bass
		ui.setMode(GUI_MODE_SET_BASS);
	}
	ui.update_time();
}


void setup(void) {
	stdout = serial_stream;
	Serial.begin(115200);
	printf("Init...\n");
	Wire.begin();

	rtc.begin();
	if (! rtc.isrunning()) {
			Serial.println("RTC is NOT running!");
			// following line sets the RTC to the date & time this sketch was compiled
			rtc.adjust(DateTime(__DATE__, __TIME__));
	}	
	tda.update();
  Radio.init();
  Radio.set_frequency(89.5); 


	digitalWrite(LCD_RESET, HIGH);
	pinMode(LCD_RESET, OUTPUT);
	digitalWrite(LCD_RESET, LOW);
	delayMicroseconds(1000);
	digitalWrite(LCD_RESET, HIGH);

	tft.reset();

	//uint16_t identifier = tft.readID();
	//tft.begin(identifier);
	tft.begin(0x9341);
	tft.setRotation(1);

	ui.setMode(GUI_MODE_IDLE);
	ui.update();
	
	Serial.println("Run");
	
}

unsigned long ts_time;

void loop() {
	TSPoint p = ts.getPoint();
	pinMode(XP, OUTPUT);
	pinMode(XM, OUTPUT);
	pinMode(YP, OUTPUT);
	pinMode(YM, OUTPUT);

	if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
		ts_time= millis();
		
		int x = p.y; int y = p.x;
		p.x = map(x, TS_MINX, TS_MAXX, 0, 320);
		p.y = map(y, TS_MINY, TS_MAXY, 0, 240);
		
		int idx = ui.getButtonIndexByXY(p.x, p.y);
		if (idx!=-1){
				ui.buttons[idx].click(1);
		}
	} else {  // Не нажато - отжимаем кнопки
		if ((millis()-ts_time) > 50){
			ui.unClickAll();
		} 
		
	}
	ui.idle();
}


