// ddm_userland_interface.h

#ifndef _DISK_DEVICE_MANAGER_USERLAND_INTERFACE_H
#define _DISK_DEVICE_MANAGER_USERLAND_INTERFACE_H

#include "disk_device_manager.h"

// userland partition representation
struct user_partition_data {
	partition_id		id;
	off_t				offset;
	off_t				size;
	uint32				block_size;
	uint32				status;
	uint32				flags;
	dev_t				volume;
	int32				change_counter;	// needed?
	char				*name;
	char				*content_name;
	char				*type;
	char				*content_type;
	char				*parameters;
	char				*content_parameters;
	void				*user_data;
	int32				child_count;
	user_partition_data	*children[1];
};

// userland disk device representation
struct user_disk_device_data {
	uint32				device_flags;
	char				*path;
	user_partition_data	device_partition_data;
};

// userland partitionable space representation
struct user_partitionable_space_data {
	off_t	offset;
	off_t	size;
};

// userland disk device job representation
struct user_disk_device_job_info {
	disk_job_id		id;
	uint32			type;
	partition_id	partition;
	char			desription[256];
};

// iterating, retrieving device/partition data
partition_id get_next_disk_device_id(int32 *cookie, size_t neededSize = NULL);
status_t get_disk_device_data(partition_id deviceID, bool shadow,
							  user_disk_device_data *buffer,
							  size_t bufferSize, size_t *neededSize);
status_t get_partition_data(partition_id partitionID, bool shadow,
							user_partition_data *buffer,
							size_t bufferSize, size_t *neededSize);
	// Dangerous?!
status_t get_partitionable_spaces(partition_id partitionID, bool shadow,
								  user_partitionable_space_data *buffer,
								  size_t bufferSize, size_t *neededSize);
	// Pass the partition change counter? If GetPartitionInfo() is only
	// allowed, when the device is locked, then we wouldn't need it.

// disk systems
status_t find_disk_system(const char *name, disk_system_id *id);
status_t get_next_disk_system(disk_system_id *id, char *name, int32 *cookie);
bool supports_partition_operation(uint32 operation, void *parameters);
bool validate_partition_operation(uint32 operation, void *parameters);
	// TODO: Sorry, I was too lazy: supports_validates_parameters.h is only
	// for kernel internal use. There needs to be something similar for these
	// functions.

// partition modification
status_t prepare_disk_device_modifications(partition_id device);
status_t commit_disk_device_modifications(partition_id device, port_id port,
										  int32 token, bool completeProgress);
status_t cancel_disk_device_modifications(partition_id device);
bool is_disk_device_modified(partition_id device);

status_t defragment_partition(partition_id partition);
status_t repair_partition(partition_id partition, bool checkOnly);
status_t resize_partition(partition_id partition, off_t size);
status_t move_partition(partition_id partition, off_t offset);
status_t set_partition_parameters(partition_id partition,
								  const char *parameters,
								  const char *contentParameters);
status_t initialize_partition(partition_id partition, const char *diskSystem,
							  const char *parameters);
	// Note: There is also fs_initialize_volume(), which is not compatible
	// with this function, for it is more general with respect to how the
	// volume to be initialized is specified (though this might be solved
	// by providing an API for registering files as disk devices), and more
	// specific regarding the other parameters (flags and volumeName).
status_t create_child_partition(partition_id partition, off_t offset,
								off_t size, const char *parameters,
								partition_id *child);
status_t delete_partition(partition_id partition);

// jobs
status_t get_next_disk_device_job_info(user_disk_device_job_info *info,
									   int32 *cookie);
status_t get_disk_device_job_info(disk_job_id id,
								  user_disk_device_job_info *info);
status_t get_disk_device_job_status(disk_job_id id, uint32 *status,
									float *progress);

// watching
status_t start_disk_device_watching(port_id, int32 token, uint32 flags);
status_t start_disk_device_job_watching(disk_job_id job, port_id, int32 token,
										uint32 flags);
status_t stop_disk_device_watching(port_id, int32 token);

#endif	// _DISK_DEVICE_MANAGER_USERLAND_INTERFACE_H
