/*
 * Copyright 2006, Jérôme Duval. All rights reserved.
 *
 * Distributed under the terms of the MIT License.
 */

#ifndef __ACPI_PRIV_H__
#define __ACPI_PRIV_H__

#include <device_manager.h>
#include <KernelExport.h>
#include <ACPI.h>

// name of ACPI root module
#define ACPI_ROOT_MODULE_NAME 	"bus_managers/acpi/root/driver_v1"

// name of ACPI device modules
#define ACPI_DEVICE_MODULE_NAME "bus_managers/acpi/driver_v1"

// name of the ACPI namespace device
#define ACPI_NS_DUMP_DEVICE_MODULE_NAME "bus_managers/acpi/namespace/device_v1"


extern device_manager_info *gDeviceManager;




// ACPI root.
typedef struct acpi_root_info {
	driver_module_info info;
	
	/* Fixed Event Management */
	
	void				(*enable_fixed_event) (uint32 event);
	void				(*disable_fixed_event) (uint32 event);
	
	uint32				(*fixed_event_status) (uint32 event);
						/* Returns 1 if event set, 0 otherwise */
	void				(*reset_fixed_event) (uint32 event);
	
	status_t			(*install_fixed_event_handler)	(uint32 event, interrupt_handler *handler, void *data); 
	status_t			(*remove_fixed_event_handler)	(uint32 event, interrupt_handler *handler); 

	/* Namespace Access */
	
	status_t			(*get_next_entry) (uint32 object_type, const char *base, char *result, size_t len, void **counter);
	status_t			(*get_device) (const char *hid, uint32 index, char *result);
	
	status_t			(*get_device_hid) (const char *path, char *hid);
	uint32				(*get_object_type) (const char *path);
	status_t			(*get_object) (const char *path, acpi_object_type **return_value);
	status_t			(*get_object_typed) (const char *path, acpi_object_type **return_value, uint32 object_type);
	
	/* Control method execution and data acquisition */
	
	status_t			(*evaluate_object) (const char *object, acpi_object_type *return_value, size_t buf_len);
	status_t			(*evaluate_method) (const char *object, const char *method, acpi_object_type *return_value, size_t buf_len, acpi_object_type *args, int num_args);
} acpi_root_info;


extern struct acpi_module_info acpi_module;

extern struct device_module_info acpi_ns_dump_module;

extern acpi_device_module_info gACPIDeviceModule;


void enable_fixed_event (uint32 event);
void disable_fixed_event (uint32 event);

uint32 fixed_event_status (uint32 event);
void reset_fixed_event (uint32 event);

status_t install_fixed_event_handler	(uint32 event, interrupt_handler *handler, void *data); 
status_t remove_fixed_event_handler	(uint32 event, interrupt_handler *handler); 


status_t get_next_entry (uint32 object_type, const char *base, char *result, size_t len, void **counter);
status_t get_device (const char *hid, uint32 index, char *result);

status_t get_device_hid (const char *path, char *hid);
uint32 get_object_type (const char *path);
status_t get_object(const char *path, acpi_object_type **return_value);
status_t get_object_typed(const char *path, acpi_object_type **return_value, uint32 object_type);

status_t evaluate_object (const char *object, acpi_object_type *return_value, size_t buf_len);
status_t evaluate_method (const char *object, const char *method, acpi_object_type *return_value, size_t buf_len, acpi_object_type *args, int num_args);

status_t enter_sleep_state (uint8 state);


#endif	/* __ACPI_PRIV_H__ */
