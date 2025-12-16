
Changes in V12 compared to V11:

1: Removed qsys-related components. The qsys built-in SDRAM controller has relatively low efficiency and only supports single read/write operations.
2: Implemented a simple SDRAM controller that supports 4-burst reads.
3: Implemented a dual-master port bus architecture, with each master port having an 8-byte cache.
4: Since the CDC buffer is now on the STM32 side, the STM32 is now responsible for filling the CDC FIFO.

