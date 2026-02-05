#include <Arduino.h>
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <Dot_matrix_Data.h>



/* ===== CLOCK PINS ===== */
#define CLK_U1 PC4   // Columns: DIS1,4,7 + shared DIS2,5,8
#define CLK_U3 PC3   // Columns: DIS3,6,9 + shared DIS2,5,8
#define CLK_U2 PC2   // Rows: DIS1,2,3
#define CLK_U4 PC1   // Rows: DIS4,5,6
#define CLK_U5 PC0   // Rows: DIS7,8,9
#define LED_PIN PC7 // On-board LED
/* ===== OUTPUT ENABLE ===== */
#define OE_COL PE2   // OE for U1 & U3 (columns only)

inline void pulse(uint8_t pin) {
 
  PORTC &= ~(1 << pin);
  _delay_us(1);
  PORTC |=  (1 << pin);
  _delay_us(1);
  
}

inline void columnsOff() {
  PORTE |= (1 << OE_COL);   // OE HIGH = columns OFF
}

inline void columnsOn() {
  PORTE &= ~(1 << OE_COL);  // OE LOW = columns ON
}

void intPort(){
    DDRA = 0xFF;                 // Data bus
    DDRC = 0b00011111;           // Clocks
    DDRE |= (1 << OE_COL);       // OE pin
}

///////////////////////////////////////////

#define REFRESH_PERIOD_US 25
#define DISPLAY_DIGITS 9
#define FONT_WIDTH 5
#define FONT_HEIGHT 5
#define SCROLL_INTERVAL_MS 250  // Adjust scroll speed here
#define SCROLL_START_DIGIT 0
#define SCROLL_END_DIGIT   (DISPLAY_DIGITS - STATIC_DIGITS - 1)
#define BLINK_PERIOD_MS 500
#define STATIC_DIGITS 4   // left side


volatile uint32_t millis_counter = 0;
uint32_t lastRefresh = 0;
int scrollChar = 0; // current character

uint16_t scrollCol = 0;       // global scroll position
unsigned long lastScrollTime = 0;

uint8_t activeDigit = 1;   // D1..D9
uint8_t value = 0;         // 0..9
unsigned long lastTick = 0;

//Global blink control
bool blinkEnabled = false;
bool blinkState = true;
uint32_t lastBlink = 0;

// Call this in a timer interrupt, e.g., every 1 ms
void SysTick_Handler(void) {
    millis_counter++;
}

uint8_t digitBrightness[DISPLAY_DIGITS] = {
    255,255,255,255,255,255,255,255,255
};



/* ===== DISPLAY BUFFER ===== */
uint8_t frameBuf[DISPLAY_DIGITS][FONT_WIDTH];

void scanDigit(uint8_t digit, uint8_t col, uint8_t fontByte)
{
    
    columnsOff();
    // clear row latches
    PORTA = 0x00;
    pulse(CLK_U2);
    pulse(CLK_U4);
    pulse(CLK_U5);

    // select row latch for this digit
    uint8_t rowLatch;
    if (digit <= 3)      rowLatch = CLK_U2;
    else if (digit <= 6) rowLatch = CLK_U4;
    else                 rowLatch = CLK_U5;
   
    // load row data
    PORTA = fontByte;
    pulse(rowLatch);

    byte u1 = 0xFF;
    byte u3 = 0xFF;

    // column selection
    if (digit == 1 || digit == 4 || digit == 7)
        u1 = column[0][col];
    else if (digit == 3 || digit == 6 || digit == 9)
        u3 =column[1][col];
    else {
      u1=  column_shared_u1[col];  
     u3=  column_shared_u3[col];
    }

    PORTA = u1; pulse(CLK_U1);
    PORTA = u3; pulse(CLK_U3);

    columnsOn();
    //uint8_t b = digitBrightness[digit - 1];
//_delay_us((uint16_t)b);   // ON time
//columnsOff();
//_delay_us((uint16_t)(255 - b)); // OFF time
    _delay_us(25);
}


uint8_t getFontByte(char ch, uint8_t col)
{
    if (ch < 0x20 || ch > 0x7E) ch = ' ';
    return pgm_read_byte(&DotMatrix_5X7_FontData[ch - 0x20][col]);

}


void refreshDisplay()
{
    static uint8_t digit = 1;
    static uint8_t col   = 0;
    scanDigit(digit, FONT_WIDTH - 1 - col, frameBuf[digit-1][col]);
    col++;
    if (col >= FONT_WIDTH) {
        col = 0;
        digit++;
        if (digit > DISPLAY_DIGITS)
            digit = 1;
    }

  //  if (blinkEnabled && !blinkState)
   // scanDigit(digit, col, 0x00); // blank
//else
    //scanDigit(digit, col, frameBuf[digit-1][col]);
}


void updateBlink(void)
{
    if (!blinkEnabled) return;

    if (millis() - lastBlink > BLINK_PERIOD_MS) {
        lastBlink = millis();
        blinkState = !blinkState;
    }
}


void scrollText(const char *text)
{
    static uint16_t scrollPos = 0;
    static uint32_t lastScroll = 0;

    if (millis() - lastScroll < SCROLL_INTERVAL_MS)
        return;
    lastScroll = millis();

    // Shift RIGHT: D1 → D9
    for (int d = DISPLAY_DIGITS - 1; d >= 0; d--) {
        for (int c = FONT_WIDTH - 1; c > 0; c--) {
            frameBuf[d][c] = frameBuf[d][c - 1];
        }

        if (d > 0)
            frameBuf[d][0] = frameBuf[d - 1][FONT_WIDTH - 1];
        else
            frameBuf[0][0] = 0x00;
    }

    // Inject at D1 (rightmost)
    uint16_t charIndex = scrollPos / 6;
    uint8_t  colIndex  = scrollPos % 6;

    if (text[charIndex] && colIndex < 5)
        frameBuf[0][0] = getFontByte(text[charIndex], colIndex);
    else
        frameBuf[0][0] = 0x00;

    scrollPos++;
    if (scrollPos >= strlen(text) * 6)
        scrollPos = 0;
}


void displayText(const char *text)
{
    // Clear framebuffer
    for (uint8_t d = 0; d < DISPLAY_DIGITS; d++) {
        for (uint8_t c = 0; c < FONT_WIDTH; c++) {
            frameBuf[d][c] = 0x00;
        }
    }

    uint8_t len = strlen(text);
    if (len > DISPLAY_DIGITS)
        len = DISPLAY_DIGITS;

    /*
      frameBuf index:
      0 = D1 (rightmost)
      8 = D9 (leftmost)

      We want text from D9 → D1
    */
    for (uint8_t i = 0; i < len; i++) {
        char ch = text[i];

        uint8_t digit = (DISPLAY_DIGITS - 1) - i; // D9 → D1

        for (uint8_t col = 0; col < FONT_WIDTH; col++) {
            // reverse column here to cancel scanDigit flip
            frameBuf[digit][col] = getFontByte(ch, FONT_WIDTH - 1 - col);
        }
    }
}

void displayTextCentered(const char *text)
{
    // clear framebuffer
    for (uint8_t d = 0; d < DISPLAY_DIGITS; d++)
        for (uint8_t c = 0; c < FONT_WIDTH; c++)
            frameBuf[d][c] = 0x00;

    uint8_t len = strlen(text);
    if (len > DISPLAY_DIGITS) len = DISPLAY_DIGITS;

    // starting digit so text is centered
    int8_t start = (DISPLAY_DIGITS - len) / 2;

    for (uint8_t i = 0; i < len; i++) {
        uint8_t digit = (DISPLAY_DIGITS - 1) - (start + i); // D9 → D1
        if (digit >= DISPLAY_DIGITS) continue;

        for (uint8_t col = 0; col < FONT_WIDTH; col++) {
            frameBuf[digit][col] =
                getFontByte(text[i], FONT_WIDTH - 1 - col);
        }
    }
}



void displayStaticLeft(const char *text)
{
    uint8_t len = strlen(text);
    if (len > STATIC_DIGITS) len = STATIC_DIGITS;

    for (uint8_t i = 0; i < len; i++) {
        uint8_t digit = (DISPLAY_DIGITS - 1) - i; // leftmost
        for (uint8_t col = 0; col < FONT_WIDTH; col++)
            frameBuf[digit][col] =
                getFontByte(text[i], FONT_WIDTH - 1 - col);
    }
}



////////////////////////////////////
//this scroling buttom to top
#define DIGIT_WIDTH 5
#define GLYPH_WIDTH 5
#define SCROLL_MAX (GLYPH_WIDTH + 1)  // scroll distance (5 columns + 1 space)

void scroll_one_digit(uint8_t digit)
{
    static uint8_t value = 0;      // test character index ('A')
    static uint8_t scrollPos = 0;   // scroll position inside the digit
    static unsigned long scrollTimer = 0;
    static uint8_t pauseCounter = 0;
    const uint8_t PAUSE_STEPS = 5;  // pause when fully visible

    unsigned long now = millis();

    // ---- SCROLL TIMING ----
    if (now - scrollTimer >= 150) {
        scrollTimer = now;

        if (pauseCounter > 0) {
            pauseCounter--;
        } else {
            scrollPos++;
            if (scrollPos == DIGIT_WIDTH)
                pauseCounter = PAUSE_STEPS;

            if (scrollPos >= SCROLL_MAX)
                scrollPos = 0;
        }
    }
    columnsOff();

    // clear row latches
    PORTA = 0x00;
    pulse(CLK_U2);
    pulse(CLK_U4);
    pulse(CLK_U5);
    PORTA = 0xff;
    pulse(CLK_U1);
    pulse(CLK_U3);
    columnsOn();
    // ---- SELECT ROW LATCH ----
    uint8_t rowLatch;
    if (digit <= 3) rowLatch = CLK_U2;
    else if (digit <= 6) rowLatch = CLK_U4;
    else rowLatch = CLK_U5;

    // ---- ROW SCAN ----
    for (uint8_t row = 0; row < 5; row++) {
        columnsOff();

        // Get font byte for this row
        uint8_t glyph =column[0][row]; 
          int shift = scrollPos - GLYPH_WIDTH;
        // ---- SCROLL MAPPING ----
        uint8_t colData = 0xff;
        if (shift < 0) //(scrollPos < GLYPH_WIDTH)
           
            colData =  glyph >> (-shift) ; //
        else
            
            colData =  glyph >> (-shift);
        
         PORTA =pgm_read_byte(&DotMatrix_5X7_FontData[value][row]);//rowData;
        pulse(rowLatch);
        // ---- MAP TO U1/U3 ----
        uint8_t u1 = 0xFF;
        uint8_t u3 = 0xFF;

        if (digit == 1 || digit == 4 || digit == 7)
            u1 =colData; //column[0][row];
        else if (digit == 3 || digit == 6 || digit == 9)
            u3 = column[1][row];
        else
            u1 = column_shared_u1[row], u3 = column_shared_u3[row]; 

        // ---- SEND TO SHIFT REGISTERS ----
        PORTA = u1; pulse(CLK_U1);
        PORTA = u3; pulse(CLK_U3);

        columnsOn();
        _delay_us(400);
    }
} 







