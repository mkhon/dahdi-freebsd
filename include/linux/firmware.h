#ifndef _LINUX_FIRMWARE_H_
#define _LINUX_FIRMWARE_H_

#include <sys/firmware.h>

int request_firmware(const struct firmware **firmware_p, const char *name, device_t *device);
void release_firmware(const struct firmware *firmware);

#endif /* _LINUX_FIRMWARE_H_ */
