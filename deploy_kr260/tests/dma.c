#include "dma.h"

/**
 * @brief Writes a value to a specified memory offset in the DMA virtual address.
 * 
 * This function writes the given value to the specified offset of the DMA's virtual address.
 *
 * @param virtual_addr Pointer to the DMA virtual address.
 * @param offset The memory offset within the DMA address to write to.
 * @param value The value to write at the specified offset.
 * @return Returns 0 on success.
 */
unsigned int write_dma(unsigned int *virtual_addr, int offset, unsigned int value) {
    virtual_addr[offset >> 2] = value;
    return 0;
}

/**
 * @brief Reads a value from a specified memory offset in the DMA virtual address.
 * 
 * This function reads the value from the given memory offset in the DMA's virtual address.
 *
 * @param virtual_addr Pointer to the DMA virtual address.
 * @param offset The memory offset within the DMA address to read from.
 * @return The value read from the specified offset.
 */
unsigned int read_dma(unsigned int *virtual_addr, int offset) {
    return virtual_addr[offset >> 2];
}

/**
 * @brief Prints the current status of the MM2S (Memory-Mapped to Streaming) DMA channel.
 * 
 * This function reads the MM2S status register and prints its value.
 *
 * @param virtual_addr Pointer to the DMA virtual address.
 */
void dma_mm2s_status(unsigned int *virtual_addr) {
    unsigned int status = read_dma(virtual_addr, MM2S_STATUS_REGISTER);
    printf("MM2S Status: 0x%08x\n", status);
}

/**
 * @brief Prints the current status of the S2MM (Streaming to Memory-Mapped) DMA channel.
 * 
 * This function reads the S2MM status register and prints its value.
 *
 * @param virtual_addr Pointer to the DMA virtual address.
 */
void dma_s2mm_status(unsigned int *virtual_addr) {
    unsigned int status = read_dma(virtual_addr, S2MM_STATUS_REGISTER);
    printf("S2MM Status: 0x%08x\n", status);
}

/**
 * @brief Synchronizes the MM2S DMA channel, waiting for the operation to complete.
 * 
 * This function waits until the MM2S DMA channel indicates that the operation is complete
 * by checking the status register. The function uses either a `DEBUG` flag to print status 
 * updates or sleeps for 1 microsecond in the absence of debug output.
 *
 * @param virtual_addr Pointer to the DMA virtual address.
 * @return Returns 0 once synchronization is complete.
 */
int dma_mm2s_sync(unsigned int *virtual_addr) {
    unsigned int read;
    read = read_dma(virtual_addr, MM2S_STATUS_REGISTER);
    while (!(read & (IOC_IRQ_FLAG | IDLE_FLAG))) {
        #ifdef DEBUG
            dma_mm2s_status(virtual_addr);
        #else 
            usleep(1);
        #endif
        read = read_dma(virtual_addr, MM2S_STATUS_REGISTER);
    }
    return 0;
}

/**
 * @brief Synchronizes the S2MM DMA channel, waiting for the operation to complete.
 * 
 * This function waits until the S2MM DMA channel indicates that the operation is complete
 * by checking the status register. The function uses either a `DEBUG` flag to print status 
 * updates or sleeps for 1 microsecond in the absence of debug output.
 *
 * @param virtual_addr Pointer to the DMA virtual address.
 * @return Returns 0 once synchronization is complete.
 */
int dma_s2mm_sync(unsigned int *virtual_addr) {
    unsigned int read;
    read = read_dma(virtual_addr, S2MM_STATUS_REGISTER);
    while (!(read & (IOC_IRQ_FLAG | IDLE_FLAG))) {
        #ifdef DEBUG
            dma_s2mm_status(virtual_addr);
        #else 
            usleep(1);
        #endif
        read = read_dma(virtual_addr, S2MM_STATUS_REGISTER);
    }
    return 0;
}

/**
 * @brief Configures and runs the DMA operations for multiple DMA instances.
 * 
 * This function configures and starts the DMA operation by writing the necessary control 
 * registers, setting source addresses, and transferring lengths. It also waits for synchronization 
 * of the MM2S DMA channels before completing.
 *
 * @param virtual_DMA_addr Array of DMA virtual addresses.
 * @param TX_BUFFER_OFFSET Array of source buffer offsets.
 * @param transfer_length Array of transfer lengths.
 * @param DMA_COUNT Number of DMA instances to configure and run.
 */
void configure_and_run_dma( unsigned int **virtual_DMA_addr, const unsigned int *TX_BUFFER_OFFSET, 
                            const unsigned int *transfer_length, int DMA_COUNT) {
    
    #ifdef DEBUG
        printf("Reset, Halt, and Enable Interrupts in DMA.\n");
    #endif
    for (int i = 0; i < DMA_COUNT; i++) {
        write_dma(virtual_DMA_addr[i], MM2S_CONTROL_REGISTER, RESET_DMA);
        #ifdef DEBUG
            dma_mm2s_status(virtual_DMA_addr[i]);
        #endif

        write_dma(virtual_DMA_addr[i], MM2S_CONTROL_REGISTER, HALT_DMA);
        #ifdef DEBUG
            dma_mm2s_status(virtual_DMA_addr[i]);
        #endif

        write_dma(virtual_DMA_addr[i], MM2S_CONTROL_REGISTER, ENABLE_ALL_IRQ);
        #ifdef DEBUG
            dma_mm2s_status(virtual_DMA_addr[i]);
        #endif
    }

    #ifdef DEBUG
        printf("Writing source address of the data from MM2S in DDR...\n");
    #endif
    for (int i = 0; i < DMA_COUNT; i++) {
        write_dma(virtual_DMA_addr[i], MM2S_SRC_ADDRESS_REGISTER, TX_BUFFER_OFFSET[i]);
        #ifdef DEBUG
            dma_mm2s_status(virtual_DMA_addr[i]);
        #endif
    }

    #ifdef DEBUG
        printf("Run the MM2S channel.\n");
    #endif
    for (int i = 0; i < DMA_COUNT; i++) {
        write_dma(virtual_DMA_addr[i], MM2S_CONTROL_REGISTER, RUN_DMA);
        #ifdef DEBUG
            dma_mm2s_status(virtual_DMA_addr[i]);
        #endif
    }

    #ifdef DEBUG
        printf("Writing MM2S transfer length.\n");
    #endif
    for (int i = 0; i < DMA_COUNT; i++) {
        write_dma(virtual_DMA_addr[i], MM2S_TRNSFR_LENGTH_REGISTER, transfer_length[i]);
        #ifdef DEBUG
            dma_mm2s_status(virtual_DMA_addr[i]);
        #endif
    }

    #ifdef DEBUG
        printf("Waiting for MM2S synchronization...\n");
    #endif
    for (int i = 0; i < DMA_COUNT; i++) {
        dma_mm2s_sync(virtual_DMA_addr[i]);
    }
}

/**
 * @brief Waits for DMA synchronization of multiple DMA instances.
 * 
 * This function waits for the MM2S synchronization of all the provided DMA instances. 
 * It ensures that each DMA channel has completed its transfer before proceeding.
 *
 * @param virtual_DMA_addr Array of DMA virtual addresses.
 * @param DMA_COUNT Number of DMA instances to wait for.
 */
void wait_dma_send( unsigned int **virtual_DMA_addr,  int DMA_COUNT){
    #ifdef DEBUG
        printf("Waiting for MM2S synchronization...\n");
	#endif
    for (int i = 0; i < DMA_COUNT; i++) {
    	dma_mm2s_sync(virtual_DMA_addr[i]);
	}
}
    