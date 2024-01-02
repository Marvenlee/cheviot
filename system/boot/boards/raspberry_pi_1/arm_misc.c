
/*!
*/
__attribute__((naked)) void MemoryBarrier() {
  __asm("mov r0, #0");
  __asm("mcr p15, #0, r0, c7, c10, #5");
  __asm("mov pc, lr");
}

__attribute__((naked)) void DataCacheFlush() {
  __asm("mov r0, #0");
  __asm("mcr p15, #0, r0, c7, c14, #0");
  __asm("mov pc, lr");
}

__attribute__((naked)) void SynchronisationBarrier() {
  __asm("mov r0, #0");
  __asm("mcr p15, #0, r0, c7, c10, #4");
  __asm("mov pc, lr");
}

__attribute__((naked)) void DataSynchronisationBarrier() {
  __asm("stmfd sp!, {r0-r8,r12,lr}");
  __asm("mcr p15, #0, ip, c7, c5, #0");
  __asm("mcr p15, #0, ip, c7, c5, #6");
  __asm("mcr p15, #0, ip, c7, c10, #4");
  __asm("mcr p15, #0, ip, c7, c10, #4");
  __asm("ldmfd sp!, {r0-r8,r12,pc}");
}



