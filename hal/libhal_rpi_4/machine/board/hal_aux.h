

#define AUX_UART_CLOCK 500000000

#define AUX_MU_BAUD(baud) ((AUX_UART_CLOCK/(baud*8))-1)


struct bcm2711_aux_registers
{
  uint32_t aux_irq;
  uint32_t aux_enables;
  uint32_t resvd1[14];
  uint32_t aux_mu_io_reg;
  uint32_t aux_mu_ier_reg;
  uint32_t aux_mu_iir_reg;
  uint32_t aux_mu_lcr_reg;
  uint32_t aux_mu_mcr_reg;
  uint32_t aux_mu_lsr_reg;
  uint32_t aux_mu_cntl_reg;
  uint32_t aux_mu_baud_reg;
};



