// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/******************************************************************************
 *
 * Module Name: exregion - ACPI default op_region (address space) handlers
 *
 * Copyright (C) 2000 - 2019, Intel Corp.
 *
 *****************************************************************************/

#include <acpi/acpi.h>
#include "accommon.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exregion")

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_system_memory_space_handler
 *
 * PARAMETERS:  function            - Read or Write operation
 *              address             - Where in the space to read or write
 *              bit_width           - Field width in bits (8, 16, or 32)
 *              value               - Pointer to in or out value
 *              handler_context     - Pointer to Handler's context
 *              region_context      - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the System Memory address space (Op Region)
 *
 ******************************************************************************/
acpi_status
acpi_ex_system_memory_space_handler(u32 function,
				    acpi_physical_address address,
				    u32 bit_width,
				    u64 *value,
				    void *handler_context, void *region_context)
{
	acpi_status status = AE_OK;
	void *logical_addr_ptr = NULL;
	struct acpi_mem_space_context *mem_info = region_context;
	u32 length;
	acpi_size map_length;

#ifdef ACPI_MISALIGNMENT_NOT_SUPPORTED
	u32 remainder;
#endif

	ACPI_FUNCTION_TRACE(ex_system_memory_space_handler);

	/* Validate and translate the bit width */

	switch (bit_width) {
	case 8:

		length = 1;
		break;

	case 16:

		length = 2;
		break;

	case 32:

		length = 4;
		break;

	case 64:

		length = 8;
		break;

	default:

		ACPI_ERROR((AE_INFO, "Invalid SystemMemory width %u",
			    bit_width));
		return_ACPI_STATUS(AE_AML_OPERAND_VALUE);
	}

#ifdef ACPI_MISALIGNMENT_NOT_SUPPORTED
	/*
	 * Hardware does not support non-aligned data transfers, we must verify
	 * the request.
	 */
	(void)acpi_ut_short_divide((u64) address, length, NULL, &remainder);
	if (remainder != 0) {
		return_ACPI_STATUS(AE_AML_ALIGNMENT);
	}
#endif

	/*
	 * Does the request fit into the cached memory mapping?
	 * Is 1) Address below the current mapping? OR
	 *    2) Address beyond the current mapping?
	 */
	if ((address < mem_info->mapped_physical_address) ||
	    (((u64) address + length) > ((u64)
					 mem_info->mapped_physical_address +
					 mem_info->mapped_length))) {
		/*
		 * The request cannot be resolved by the current memory mapping;
		 * Delete the existing mapping and create a new one.
		 */
		if (mem_info->mapped_length) {

			/* Valid mapping, delete it */

			acpi_os_unmap_memory(mem_info->mapped_logical_address,
					     mem_info->mapped_length);
		}

		/*
		 * October 2009: Attempt to map from the requested address to the
		 * end of the region. However, we will never map more than one
		 * page, nor will we cross a page boundary.
		 */
		map_length = (acpi_size)
		    ((mem_info->address + mem_info->length) - address);

		if (map_length > ACPI_DEFAULT_PAGE_SIZE)
			map_length = ACPI_DEFAULT_PAGE_SIZE;

		/* Create a new mapping starting at the address given */

		mem_info->mapped_logical_address =
		    acpi_os_map_memory(address, map_length);
		if (!mem_info->mapped_logical_address) {
			ACPI_ERROR((AE_INFO,
				    "Could not map memory at 0x%8.8X%8.8X, size %u",
				    ACPI_FORMAT_UINT64(address),
				    (u32)map_length));
			mem_info->mapped_length = 0;
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		/* Save the physical address and mapping size */

		mem_info->mapped_physical_address = address;
		mem_info->mapped_length = map_length;
	}

	/*
	 * Generate a logical pointer corresponding to the address we want to
	 * access
	 */
	logical_addr_ptr = mem_info->mapped_logical_address +
	    ((u64) address - (u64) mem_info->mapped_physical_address);

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			  "System-Memory (width %u) R/W %u Address=%8.8X%8.8X\n",
			  bit_width, function, ACPI_FORMAT_UINT64(address)));

	/*
	 * Perform the memory read or write
	 *
	 * Note: For machines that do not support non-aligned transfers, the target
	 * address was checked for alignment above. We do not attempt to break the
	 * transfer up into smaller (byte-size) chunks because the AML specifically
	 * asked for a transfer width that the hardware may require.
	 */
	switch (function) {
	case ACPI_READ:

		*value = 0;
		switch (bit_width) {
		case 8:

			*value = (u64)ACPI_GET8(logical_addr_ptr);
			break;

		case 16:

			*value = (u64)ACPI_GET16(logical_addr_ptr);
			break;

		case 32:

			*value = (u64)ACPI_GET32(logical_addr_ptr);
			break;

		case 64:

			*value = (u64)ACPI_GET64(logical_addr_ptr);
			break;

		default:

			/* bit_width was already validated */

			break;
		}
		break;

	case ACPI_WRITE:

		switch (bit_width) {
		case 8:

			ACPI_SET8(logical_addr_ptr, *value);
			break;

		case 16:

			ACPI_SET16(logical_addr_ptr, *value);
			break;

		case 32:

			ACPI_SET32(logical_addr_ptr, *value);
			break;

		case 64:

			ACPI_SET64(logical_addr_ptr, *value);
			break;

		default:

			/* bit_width was already validated */

			break;
		}
		break;

	default:

		status = AE_BAD_PARAMETER;
		break;
	}

	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_system_io_space_handler
 *
 * PARAMETERS:  function            - Read or Write operation
 *              address             - Where in the space to read or write
 *              bit_width           - Field width in bits (8, 16, or 32)
 *              value               - Pointer to in or out value
 *              handler_context     - Pointer to Handler's context
 *              region_context      - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the System IO address space (Op Region)
 *
 ******************************************************************************/

acpi_status
acpi_ex_system_io_space_handler(u32 function,
				acpi_physical_address address,
				u32 bit_width,
				u64 *value,
				void *handler_context, void *region_context)
{
	acpi_status status = AE_OK;
	u32 value32;

	ACPI_FUNCTION_TRACE(ex_system_io_space_handler);

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			  "System-IO (width %u) R/W %u Address=%8.8X%8.8X\n",
			  bit_width, function, ACPI_FORMAT_UINT64(address)));

	/* Decode the function parameter */

	switch (function) {
	case ACPI_READ:

		status = acpi_hw_read_port((acpi_io_address)address,
					   &value32, bit_width);
		*value = value32;
		break;

	case ACPI_WRITE:

		status = acpi_hw_write_port((acpi_io_address)address,
					    (u32)*value, bit_width);
		break;

	default:

		status = AE_BAD_PARAMETER;
		break;
	}

	return_ACPI_STATUS(status);
}

#ifdef ACPI_PCI_CONFIGURED
/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_pci_config_space_handler
 *
 * PARAMETERS:  function            - Read or Write operation
 *              address             - Where in the space to read or write
 *              bit_width           - Field width in bits (8, 16, or 32)
 *              value               - Pointer to in or out value
 *              handler_context     - Pointer to Handler's context
 *              region_context      - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the PCI Config address space (Op Region)
 *
 ******************************************************************************/

acpi_status
acpi_ex_pci_config_space_handler(u32 function,
				 acpi_physical_address address,
				 u32 bit_width,
				 u64 *value,
				 void *handler_context, void *region_context)
{
	acpi_status status = AE_OK;
	struct acpi_pci_id *pci_id;
	u16 pci_register;

	ACPI_FUNCTION_TRACE(ex_pci_config_space_handler);

	/*
	 *  The arguments to acpi_os(Read|Write)pci_configuration are:
	 *
	 *  pci_segment is the PCI bus segment range 0-31
	 *  pci_bus     is the PCI bus number range 0-255
	 *  pci_device  is the PCI device number range 0-31
	 *  pci_function is the PCI device function number
	 *  pci_register is the Config space register range 0-255 bytes
	 *
	 *  value - input value for write, output address for read
	 *
	 */
	pci_id = (struct acpi_pci_id *)region_context;
	pci_register = (u16) (u32) address;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			  "Pci-Config %u (%u) Seg(%04x) Bus(%04x) "
			  "Dev(%04x) Func(%04x) Reg(%04x)\n",
			  function, bit_width, pci_id->segment, pci_id->bus,
			  pci_id->device, pci_id->function, pci_register));

	switch (function) {
	case ACPI_READ:

		*value = 0;
		status =
		    acpi_os_read_pci_configuration(pci_id, pci_register, value,
						   bit_width);
		break;

	case ACPI_WRITE:

		status =
		    acpi_os_write_pci_configuration(pci_id, pci_register,
						    *value, bit_width);
		break;

	default:

		status = AE_BAD_PARAMETER;
		break;
	}

	return_ACPI_STATUS(status);
}
#endif

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_cmos_space_handler
 *
 * PARAMETERS:  function            - Read or Write operation
 *              address             - Where in the space to read or write
 *              bit_width           - Field width in bits (8, 16, or 32)
 *              value               - Pointer to in or out value
 *              handler_context     - Pointer to Handler's context
 *              region_context      - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the CMOS address space (Op Region)
 *
 ******************************************************************************/

acpi_status
acpi_ex_cmos_space_handler(u32 function,
			   acpi_physical_address address,
			   u32 bit_width,
			   u64 *value,
			   void *handler_context, void *region_context)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_cmos_space_handler);

	return_ACPI_STATUS(status);
}

#ifdef ACPI_PCI_CONFIGURED
/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_pci_bar_space_handler
 *
 * PARAMETERS:  function            - Read or Write operation
 *              address             - Where in the space to read or write
 *              bit_width           - Field width in bits (8, 16, or 32)
 *              value               - Pointer to in or out value
 *              handler_context     - Pointer to Handler's context
 *              region_context      - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the PCI bar_target address space (Op Region)
 *
 ******************************************************************************/

acpi_status
acpi_ex_pci_bar_space_handler(u32 function,
			      acpi_physical_address address,
			      u32 bit_width,
			      u64 *value,
			      void *handler_context, void *region_context)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_pci_bar_space_handler);

	return_ACPI_STATUS(status);
}
#endif

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_data_table_space_handler
 *
 * PARAMETERS:  function            - Read or Write operation
 *              address             - Where in the space to read or write
 *              bit_width           - Field width in bits (8, 16, or 32)
 *              value               - Pointer to in or out value
 *              handler_context     - Pointer to Handler's context
 *              region_context      - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the Data Table address space (Op Region)
 *
 ******************************************************************************/

acpi_status
acpi_ex_data_table_space_handler(u32 function,
				 acpi_physical_address address,
				 u32 bit_width,
				 u64 *value,
				 void *handler_context, void *region_context)
{
	ACPI_FUNCTION_TRACE(ex_data_table_space_handler);

	/*
	 * Perform the memory read or write. The bit_width was already
	 * validated.
	 */
	switch (function) {
	case ACPI_READ:

		memcpy(ACPI_CAST_PTR(char, value),
		       ACPI_PHYSADDR_TO_PTR(address), ACPI_DIV_8(bit_width));
		break;

	case ACPI_WRITE:

		memcpy(ACPI_PHYSADDR_TO_PTR(address),
		       ACPI_CAST_PTR(char, value), ACPI_DIV_8(bit_width));
		break;

	default:

		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	return_ACPI_STATUS(AE_OK);
}
