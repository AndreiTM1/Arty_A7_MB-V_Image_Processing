#include "bitmap.h"
#include "PmodOLEDrgb.h"
#include "sleep.h"
#include "xil_cache.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xuartlite_l.h" 

#define XPAR_PMODOLEDRGB_0_AXI_LITE_GPIO_BASEADDR 0x80000
#define XPAR_PMODOLEDRGB_0_AXI_LITE_SPI_BASEADDR 0x100000
#define SWITCH_GPIO_BASEADDR XPAR_XGPIO_0_BASEADDR
#define UART_BASEADDR XPAR_XUARTLITE_0_BASEADDR

void DemoInitialize();
void DemoRun();
void DemoCleanup();
void EnableCaches();
void DisableCaches();
void ApplyFilter(u8 switch_state);

PmodOLEDrgb oledrgb;
XGpio switches;

// The OLED display is 96x64 pixels (16-bit RGB565 per pixel)
#define NUM_PIXELS (96 * 64)

// TWO buffers: One for the raw UART data, one for the filtered output
u16 base_image[NUM_PIXELS];     
u16 filtered_image[NUM_PIXELS]; 

int main(void) {
   DemoInitialize();
   DemoRun();
   DemoCleanup();
   return 0;
}

void DemoInitialize() {
   EnableCaches();
   
   OLEDrgb_begin(&oledrgb, XPAR_PMODOLEDRGB_0_AXI_LITE_GPIO_BASEADDR,
         XPAR_PMODOLEDRGB_0_AXI_LITE_SPI_BASEADDR);

   XGpio_Initialize(&switches, SWITCH_GPIO_BASEADDR);
   XGpio_SetDataDirection(&switches, 1, 0xFFFFFFFF); 
}

void ApplyFilter(u8 switch_state) {
    u8* base_bytes = (u8*)base_image;
    u8* filtered_bytes = (u8*)filtered_image;

    for (int i = 0; i < NUM_PIXELS; i++) {
        
        u8 high = base_bytes[i*2];
        u8 low  = base_bytes[i*2 + 1];
        u16 pixel = (high << 8) | low;

        u8 r = (pixel >> 11) & 0x1F;
        u8 g = (pixel >> 5)  & 0x3F;
        u8 b = pixel         & 0x1F;

        u8 luma = ((r << 3) + (g << 2) + (b << 3)) / 3; 

        if (switch_state & 0x01) { 
            r = luma >> 3; 
            g = luma >> 2; 
            b = luma >> 3;
        }

        if (switch_state & 0x02) {
            if (luma > 127) {
                r = 31;
                g = 63;
                b = 31;
            } else {
                r = 0;
                g = 0;
                b = 0;
            }
        }

        if (switch_state & 0x04) { 
            r = 31 - r; 
            g = 63 - g; 
            b = 31 - b;
        }

        if (switch_state & 0x08) {
            r = 0;
        }

        u16 new_pixel = (r << 11) | (g << 5) | b;
        
        filtered_bytes[i*2] = (new_pixel >> 8) & 0xFF;
        filtered_bytes[i*2 + 1] = new_pixel & 0xFF;
    }
}

void DemoRun() {
   u8* uart_rx_buffer = (u8*)base_image; 
   u32 bytes_received = 0;
   u32 total_bytes_needed = NUM_PIXELS * 2;
   
   u8 current_switch_state = 0;
   u8 last_switch_state = 255; 
   u8 image_ready = 0; // Flag to prevent filtering before an image exists

   // Startup Text
   OLEDrgb_SetCursor(&oledrgb, 1, 1);
   OLEDrgb_SetFontColor(&oledrgb, OLEDrgb_BuildRGB(255, 255, 255));
   OLEDrgb_PutString(&oledrgb, "Waiting for");
   OLEDrgb_SetCursor(&oledrgb, 1, 3);
   OLEDrgb_PutString(&oledrgb, "Python Script...");

   while (1) {
      // Listen for incoming Python Image Data
      if (!XUartLite_IsReceiveEmpty(UART_BASEADDR)) {
          u8 rx_byte = XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
          uart_rx_buffer[bytes_received] = rx_byte;
          bytes_received++;

          if (bytes_received >= total_bytes_needed) {
              bytes_received = 0;
              image_ready = 1; 
              last_switch_state = 255; // Force a filter/draw right now
          }
      }

      // Watch Switches & Update Screen
      if (image_ready) {
          current_switch_state = XGpio_DiscreteRead(&switches, 1);

          if (current_switch_state != last_switch_state) {
              
              ApplyFilter(current_switch_state);
              
              OLEDrgb_DrawBitmap(&oledrgb, 0, 0, 95, 63, (u8*)filtered_image);
              
              last_switch_state = current_switch_state;
          }
      }
   }
}

void DemoCleanup() { DisableCaches(); }
void EnableCaches() {
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_ICACHE
   Xil_ICacheEnable();
#endif
#ifdef XPAR_MICROBLAZE_USE_DCACHE
   Xil_DCacheEnable();
#endif
#endif
}
void DisableCaches() {
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_DCACHE
   Xil_DCacheDisable();
#endif
#ifdef XPAR_MICROBLAZE_USE_ICACHE
   Xil_ICacheDisable();
#endif
#endif
}