

struct bcm2711_gpio_registers
{
  uint32_t fsel[6];     // 0x00 
  uint32_t resvd1;      // 0x18
  uint32_t set[2];      // 0x1C
  uint32_t resvd2;      // 0x24
  uint32_t clr[2];      // 0x28
  uint32_t resvd3;      // 0x30
  uint32_t lev[2];      // 0x34
  uint32_t resvd4;      // 0x3c
  uint32_t eds[2];      // 0x40
  uint32_t resvd5;      // 0x48
  uint32_t ren[2];      // 0x4c
  uint32_t resvd6;      // 0x54
  uint32_t fen[2];      // 0x58
  uint32_t resvd7;      // 0x60
  uint32_t hen[2];      // 0x64
  uint32_t resvd8;      // 0x6c
  uint32_t len[2];      // 0x70
  uint32_t resvd9;      // 0x78
  uint32_t aren[2];     // 0x7c
  uint32_t resvd10;     // 0x84
  uint32_t afen[2];     // 0x88
  uint32_t resvd11[21];  // 0x90
  uint32_t pup_pdn_ctrl[4]; // 0xe4  
};

