
#ifndef BOARDS_RASPBERRY_PI_1_BOOTSTRAP_H
#define BOARDS_RASPBERRY_PI_1_BOOTSTRAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <machine/cheviot_hal.h>

void BootstrapKernel(vm_addr kernel_ceiling);

#endif
