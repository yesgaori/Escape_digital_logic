/******************************************************************************
 * DouKnowKimchee SoC - FINAL CORRECTED VERSION
 * 
 * CRITICAL FIX: GPIO 채널 할당이 반대였음!
 * 
 * Hardware Configuration (Vivado Block Design 기준):
 * - GPIO  (Channel 1) = push_buttons_4bits  (BTN0~BTN3)
 * - GPIO2 (Channel 2) = dip_switches_16bits (SW0~SW15)
 * 
 * MODE SELECTION (Switches):
 * - SW0 (bit 0): Clock Mode
 * - SW1 (bit 1): Kitchen Timer Mode  
 * - SW2 (bit 2): Stopwatch Mode
 * - All OFF: "TIME IS GOLD" display
 * 
 * FUNCTION CONTROL (Buttons - mode dependent):
 * Clock Mode (SW0=ON):
 *   - BTN0: Toggle Edit mode
 *   - BTN1: Select field (HH/MM/SS)
 *   - BTN2: Increment selected field
 * 
 * Kitchen Timer Mode (SW1=ON):
 *   - BTN0: Start/Stop timer
 *   - BTN1: Increment minutes
 *   - BTN2: Increment seconds
 * 
 * Stopwatch Mode (SW2=ON):
 *   - BTN0: Start/Stop
 *   - BTN1: Lap time record
 *   - BTN2: Clear/Reset
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xil_io.h"
#include "sleep.h"
#include "xiic.h"
#include "xgpio.h"
#include "xintc.h"

// =============================================================================
// Hardware Address Definitions
// =============================================================================

// Custom IP Base Addresses
#define KITCHEN_TIMER_BASE  XPAR_MYIP_KITCHEN_TIMER_C_0_BASEADDR  // 0x44a10000
#define CLOCK_BASE          XPAR_MYIP_FORIPTIME_0_BASEADDR        // 0x44a30000
#define STOPWATCH_BASE      XPAR_MYIP_STOPWATCH_0_BASEADDR        // 0x44a20000

// Kitchen Timer Registers
#define KTIMER_REG_STATUS   0x00
#define KTIMER_REG_STATE    0x04
#define KTIMER_REG_IRQ_CLR  0x08
#define KTIMER_REG_START    0x0C
#define KTIMER_REG_MIN_INC  0x10
#define KTIMER_REG_SEC_INC  0x14

// Clock Registers
#define CLOCK_REG_TIME      0x00
#define CLOCK_REG_STATUS    0x04
#define CLOCK_REG_BTN_EDIT  0x0C
#define CLOCK_REG_BTN_FIELD 0x10
#define CLOCK_REG_BTN_INC   0x14

// Stopwatch Registers
#define STOPWATCH_REG_TIME  0x00
#define STOPWATCH_REG_STATE 0x04
#define STOPWATCH_REG_START 0x0C
#define STOPWATCH_REG_LAP   0x10
#define STOPWATCH_REG_CLEAR 0x14

// =============================================================================
// GPIO Configuration - CORRECTED!
// =============================================================================
// Vivado Block Design 확인 결과:
// - GPIO  (Channel 1) = push_buttons_4bits  (4-bit)
// - GPIO2 (Channel 2) = dip_switches_16bits (16-bit)

#define GPIO_DEVICE_ID      0
#define GPIO_BASEADDR       XPAR_AXI_GPIO_0_BASEADDR  // 0x40000000

#define GPIO_BUTTONS_CH     1  // Channel 1 = Buttons (4-bit)
#define GPIO_SWITCHES_CH    2  // Channel 2 = Switches (16-bit)

#define IIC_DEVICE_ID       0
#define IIC_BASEADDR        XPAR_AXI_IIC_0_BASEADDR

#define INTC_DEVICE_ID      0
#define INTC_BASEADDR       XPAR_MICROBLAZE_RISCV_0_AXI_INTC_BASEADDR
#define KTIMER_IRQ_ID       0

#define LCD_ADDR            0x27
#define LCD_BL_ON           0x08
#define LCD_EN              0x04
#define LCD_RS              0x01

// =============================================================================
// Global Variables
// =============================================================================

XIic IicInstance;
XGpio GpioInstance;
XIntc IntcInstance;

volatile int kitchen_timer_alarm_flag = 0;

typedef enum {
    MODE_IDLE = 0,
    MODE_CLOCK = 1,          // SW0
    MODE_KITCHEN_TIMER = 2,  // SW1
    MODE_STOPWATCH = 3       // SW2
} SystemMode;

SystemMode current_mode = MODE_IDLE;
u32 prev_buttons = 0;

// =============================================================================
// I2C LCD Functions
// =============================================================================

int I2C_Write_Byte(u8 data) {
    u8 buffer[1];
    buffer[0] = data;
    return XIic_Send(IicInstance.BaseAddress, LCD_ADDR, buffer, 1, XIIC_STOP);
}

void LCD_Send_Nibble(u8 nibble, u8 mode) {
    u8 data = (nibble & 0xF0) | mode | LCD_BL_ON;
    I2C_Write_Byte(data);
    I2C_Write_Byte(data | LCD_EN);
    usleep(50);
    I2C_Write_Byte(data & ~LCD_EN);
    usleep(50);
}

void LCD_Send_Byte(u8 byte, u8 mode) {
    LCD_Send_Nibble(byte & 0xF0, mode);
    LCD_Send_Nibble((byte << 4) & 0xF0, mode);
}

void LCD_Init() {
    usleep(50000);
    LCD_Send_Nibble(0x30, 0); usleep(5000);
    LCD_Send_Nibble(0x30, 0); usleep(150);
    LCD_Send_Nibble(0x30, 0); usleep(150);
    LCD_Send_Nibble(0x20, 0); usleep(150);
    LCD_Send_Byte(0x28, 0);
    LCD_Send_Byte(0x0C, 0);
    LCD_Send_Byte(0x01, 0); usleep(2000);
    LCD_Send_Byte(0x06, 0);
}

void LCD_Set_Cursor(u8 row, u8 col) {
    u8 addr = (row == 0) ? 0x80 : 0xC0;
    addr += col;
    LCD_Send_Byte(addr, 0);
}

void LCD_Print(const char *str) {
    while (*str) {
        LCD_Send_Byte(*str++, 1);
    }
}

void LCD_Clear() {
    LCD_Send_Byte(0x01, 0);
    usleep(2000);
}

void LCD_Print_Line(u8 row, const char *str) {
    char buffer[17];
    snprintf(buffer, sizeof(buffer), "%-16s", str);
    LCD_Set_Cursor(row, 0);
    LCD_Print(buffer);
}

// =============================================================================
// Kitchen Timer IRQ Handler
// =============================================================================

void KitchenTimerIRQHandler(void *CallbackRef) {
    kitchen_timer_alarm_flag = 1;
    Xil_Out32(KITCHEN_TIMER_BASE + KTIMER_REG_IRQ_CLR, 0x01);
    xil_printf("[IRQ] Kitchen Timer Alarm!\r\n");
}

// =============================================================================
// Button Edge Detection
// =============================================================================

u32 Get_Button_Edge() {
    u32 current = XGpio_DiscreteRead(&GpioInstance, GPIO_BUTTONS_CH);
    u32 edge = current & (~prev_buttons);
    prev_buttons = current;
    return edge;
}

// =============================================================================
// Mode-Specific Functions
// =============================================================================

void Display_Idle_Screen() {
    LCD_Print_Line(0, " TIME IS GOLD  ");
    LCD_Print_Line(1, "SW0/1/2: Select");
}

void Handle_Clock() {
    static int last_display_update = 0;
    u32 time_reg, status_reg, button_edge;
    u8 hh, mm, ss;
    char line1[17], line2[17];
    
    time_reg = Xil_In32(CLOCK_BASE + CLOCK_REG_TIME);
    status_reg = Xil_In32(CLOCK_BASE + CLOCK_REG_STATUS);
    
    hh = (time_reg >> 0)  & 0xFF;
    mm = (time_reg >> 8)  & 0xFF;
    ss = (time_reg >> 16) & 0xFF;
    
    int edit_mode = (status_reg >> 3) & 0x01;
    int field_sel = (status_reg >> 4) & 0x03;
    
    button_edge = Get_Button_Edge();
    
    if (button_edge & 0x01) {
        Xil_Out32(CLOCK_BASE + CLOCK_REG_BTN_EDIT, 0x01);
        xil_printf("[Clock] Edit toggle\r\n");
    }
    if (button_edge & 0x02) {
        Xil_Out32(CLOCK_BASE + CLOCK_REG_BTN_FIELD, 0x01);
        xil_printf("[Clock] Field change\r\n");
    }
    if (button_edge & 0x04) {
        Xil_Out32(CLOCK_BASE + CLOCK_REG_BTN_INC, 0x01);
        xil_printf("[Clock] Increment\r\n");
    }
    
    if (++last_display_update >= 10) {
        last_display_update = 0;
        
        snprintf(line1, sizeof(line1), "Time: %02d:%02d:%02d", hh, mm, ss);
        
        if (edit_mode) {
            const char *fields[] = {"HH", "MM", "SS"};
            snprintf(line2, sizeof(line2), "Edit: %s", 
                     field_sel < 3 ? fields[field_sel] : "??");
        } else {
            snprintf(line2, sizeof(line2), "Running...");
        }
        
        LCD_Print_Line(0, line1);
        LCD_Print_Line(1, line2);
    }
}

void Handle_Kitchen_Timer() {
    static int last_display_update = 0;
    u32 state_reg, button_edge;
    u16 time_bcd;
    u8 min_10, min_1, sec_10, sec_1;
    char line1[17], line2[17];
    
    state_reg = Xil_In32(KITCHEN_TIMER_BASE + KTIMER_REG_STATE);
    time_bcd = state_reg & 0xFFFF;
    int alarm_on = (state_reg >> 16) & 0x01;
    
    min_10 = (time_bcd >> 12) & 0xF;
    min_1  = (time_bcd >> 8)  & 0xF;
    sec_10 = (time_bcd >> 4)  & 0xF;
    sec_1  = (time_bcd >> 0)  & 0xF;
    
    button_edge = Get_Button_Edge();
    
    if (button_edge & 0x01) {
        Xil_Out32(KITCHEN_TIMER_BASE + KTIMER_REG_START, 0x01);
        xil_printf("[KTimer] Start/Stop\r\n");
    }
    if (button_edge & 0x02) {
        Xil_Out32(KITCHEN_TIMER_BASE + KTIMER_REG_MIN_INC, 0x01);
        xil_printf("[KTimer] Min++\r\n");
    }
    if (button_edge & 0x04) {
        Xil_Out32(KITCHEN_TIMER_BASE + KTIMER_REG_SEC_INC, 0x01);
        xil_printf("[KTimer] Sec++\r\n");
    }
    
    if (++last_display_update >= 10) {
        last_display_update = 0;
        
        snprintf(line1, sizeof(line1), "Timer: %d%d:%d%d", 
                 min_10, min_1, sec_10, sec_1);
        
        if (alarm_on || kitchen_timer_alarm_flag) {
            snprintf(line2, sizeof(line2), "!! ALARM ON !!");
        } else {
            snprintf(line2, sizeof(line2), "BTN: Start/M/S");
        }
        
        LCD_Print_Line(0, line1);
        LCD_Print_Line(1, line2);
    }
    
    if (kitchen_timer_alarm_flag) {
        kitchen_timer_alarm_flag = 0;
    }
}

void Handle_Stopwatch() {
    static int last_display_update = 0;
    u32 time_reg, state_reg, button_edge;
    u8 min, sec, csec;
    char line1[17], line2[17];
    
    time_reg = Xil_In32(STOPWATCH_BASE + STOPWATCH_REG_TIME);
    state_reg = Xil_In32(STOPWATCH_BASE + STOPWATCH_REG_STATE);
    
    min  = (time_reg >> 0)  & 0xFF;
    sec  = (time_reg >> 8)  & 0xFF;
    csec = (time_reg >> 16) & 0xFF;
    
    int running = (state_reg >> 3) & 0x01;
    int lap_mode = (state_reg >> 4) & 0x01;
    
    button_edge = Get_Button_Edge();
    
    if (button_edge & 0x01) {
        Xil_Out32(STOPWATCH_BASE + STOPWATCH_REG_START, 0x01);
        xil_printf("[Stopwatch] Start/Stop\r\n");
    }
    if (button_edge & 0x02) {
        Xil_Out32(STOPWATCH_BASE + STOPWATCH_REG_LAP, 0x01);
        xil_printf("[Stopwatch] Lap\r\n");
    }
    if (button_edge & 0x04) {
        Xil_Out32(STOPWATCH_BASE + STOPWATCH_REG_CLEAR, 0x01);
        xil_printf("[Stopwatch] Clear\r\n");
    }
    
    if (++last_display_update >= 10) {
        last_display_update = 0;
        
        snprintf(line1, sizeof(line1), "%02d:%02d.%02d %s", 
                 min, sec, csec, running ? "RUN" : "STP");
        
        if (lap_mode) {
            snprintf(line2, sizeof(line2), "LAP Mode");
        } else {
            snprintf(line2, sizeof(line2), "BTN: S/L/C");
        }
        
        LCD_Print_Line(0, line1);
        LCD_Print_Line(1, line2);
    }
}

// =============================================================================
// Initialization Functions
// =============================================================================

int Init_IIC() {
    if (XIic_Initialize(&IicInstance, IIC_DEVICE_ID) != XST_SUCCESS) {
        xil_printf("[ERROR] IIC Init Failed\r\n");
        return XST_FAILURE;
    }
    XIic_Start(&IicInstance);
    return XST_SUCCESS;
}

int Init_GPIO() {
    if (XGpio_Initialize(&GpioInstance, GPIO_DEVICE_ID) != XST_SUCCESS) {
        xil_printf("[ERROR] GPIO Init Failed\r\n");
        return XST_FAILURE;
    }
    
    // Channel 1: Buttons (Input)
    XGpio_SetDataDirection(&GpioInstance, GPIO_BUTTONS_CH, 0xFFFFFFFF);
    
    // Channel 2: Switches (Input)
    XGpio_SetDataDirection(&GpioInstance, GPIO_SWITCHES_CH, 0xFFFFFFFF);
    
    xil_printf("[INFO] GPIO Init OK (CH1=Buttons, CH2=Switches)\r\n");
    
    // 초기 상태 테스트
    u32 btn_test = XGpio_DiscreteRead(&GpioInstance, GPIO_BUTTONS_CH);
    u32 sw_test = XGpio_DiscreteRead(&GpioInstance, GPIO_SWITCHES_CH);
    xil_printf("[TEST] Initial BTN=0x%08X, SW=0x%08X\r\n", btn_test, sw_test);
    
    return XST_SUCCESS;
}

int Init_Interrupt() {
    if (XIntc_Initialize(&IntcInstance, INTC_DEVICE_ID) != XST_SUCCESS) {
        xil_printf("[ERROR] Interrupt Controller Init Failed\r\n");
        return XST_FAILURE;
    }
    
    if (XIntc_Connect(&IntcInstance, KTIMER_IRQ_ID,
                      (XInterruptHandler)KitchenTimerIRQHandler,
                      NULL) != XST_SUCCESS) {
        xil_printf("[ERROR] IRQ Connect Failed\r\n");
        return XST_FAILURE;
    }
    
    XIntc_Enable(&IntcInstance, KTIMER_IRQ_ID);
    
    if (XIntc_Start(&IntcInstance, XIN_REAL_MODE) != XST_SUCCESS) {
        xil_printf("[ERROR] Interrupt Start Failed\r\n");
        return XST_FAILURE;
    }
    
    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
                                  (Xil_ExceptionHandler)XIntc_InterruptHandler,
                                  &IntcInstance);
    Xil_ExceptionEnable();
    
    xil_printf("[INFO] Kitchen Timer IRQ Enabled\r\n");
    return XST_SUCCESS;
}

// =============================================================================
// Main Function
// =============================================================================

int main() {
    init_platform();
    
    xil_printf("\r\n");
    xil_printf("╔════════════════════════════════════╗\r\n");
    xil_printf("║   DouKnowKimchee SoC System        ║\r\n");
    xil_printf("║   3-in-1: Timer/Clock/Stopwatch    ║\r\n");
    xil_printf("╚════════════════════════════════════╝\r\n\r\n");
    
    if (Init_IIC() != XST_SUCCESS) return XST_FAILURE;
    if (Init_GPIO() != XST_SUCCESS) return XST_FAILURE;
    if (Init_Interrupt() != XST_SUCCESS) return XST_FAILURE;
    
    LCD_Init();
    Display_Idle_Screen();
    
    xil_printf("[INFO] System Ready\r\n\r\n");
    xil_printf("=== MODE SELECTION (Switches) ===\r\n");
    xil_printf("  SW0 = Clock\r\n");
    xil_printf("  SW1 = Kitchen Timer\r\n");
    xil_printf("  SW2 = Stopwatch\r\n\r\n");
    xil_printf("=== CONTROL (Buttons - mode dependent) ===\r\n");
    xil_printf("Clock:         BTN0=Edit, BTN1=Field, BTN2=Inc\r\n");
    xil_printf("Kitchen Timer: BTN0=Start/Stop, BTN1=Min++, BTN2=Sec++\r\n");
    xil_printf("Stopwatch:     BTN0=Start/Stop, BTN1=Lap, BTN2=Clear\r\n\r\n");
    
    SystemMode previous_mode = MODE_IDLE;
    int debug_counter = 0;
    
    while (1) {
        // =====================================================================
        // STEP 1: 스위치로 모드 선택 (Channel 2 = Switches)
        // =====================================================================
        u32 switches = XGpio_DiscreteRead(&GpioInstance, GPIO_SWITCHES_CH);
        
        // 디버그: 5초마다 스위치 상태 출력
        if (++debug_counter >= 500) {  // 10ms * 500 = 5초
            debug_counter = 0;
            xil_printf("[DEBUG] SW=0x%04X (SW0=%d SW1=%d SW2=%d)\r\n",
                       switches & 0xFFFF,
                       (switches & 0x01) ? 1 : 0,
                       (switches & 0x02) ? 1 : 0,
                       (switches & 0x04) ? 1 : 0);
        }
        
        // 모드 우선순위: SW0 > SW1 > SW2
        if (switches & 0x01) {
            current_mode = MODE_CLOCK;          // SW0 = Clock
        } else if (switches & 0x02) {
            current_mode = MODE_KITCHEN_TIMER;  // SW1 = Kitchen Timer
        } else if (switches & 0x04) {
            current_mode = MODE_STOPWATCH;      // SW2 = Stopwatch
        } else {
            current_mode = MODE_IDLE;           // All OFF
        }
        
        // =====================================================================
        // STEP 2: 모드 변경 감지
        // =====================================================================
        if (current_mode != previous_mode) {
            xil_printf("[Mode Change] %d -> %d\r\n", previous_mode, current_mode);
            LCD_Clear();
            previous_mode = current_mode;
        }
        
        // =====================================================================
        // STEP 3: 모드별 처리 (버튼으로 각 모드 조작 - Channel 1 = Buttons)
        // =====================================================================
        switch (current_mode) {
            case MODE_CLOCK:
                Handle_Clock();
                break;
                
            case MODE_KITCHEN_TIMER:
                Handle_Kitchen_Timer();
                break;
                
            case MODE_STOPWATCH:
                Handle_Stopwatch();
                break;
                
            case MODE_IDLE:
            default:
                Display_Idle_Screen();
                usleep(100000);
                break;
        }
        
        usleep(10000);  // 10ms
    }
    
    cleanup_platform();
    return 0;
}
