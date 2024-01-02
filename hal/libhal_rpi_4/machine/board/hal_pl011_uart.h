



struct bcm2711_uart_registers
{
  uint32_t data;      // 0x00
  uint32_t rsrecr;    // 0x04
  uint32_t resvd1[4]; // 0x08  
  uint32_t flags;     // 0x18
  uint32_t resvd2;    // 0x1C
  uint32_t ilpr;      // 0x20
  uint32_t ibrd;      // 0x24
  uint32_t fbrd;      // 0x28
  uint32_t lcrh;      // 0x2C
  uint32_t ctrl;      // 0x30
  uint32_t ifls;      // 0x34
  uint32_t imsc;      // 0x38
  uint32_t ris;       // 0x3C
  uint32_t mis;       // 0x40
  uint32_t icr;       // 0x44
};

