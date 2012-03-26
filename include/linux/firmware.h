#ifndef _LINUX_FIRMWARE_H_
#define _LINUX_FIRMWARE_H_

#include <sys/firmware.h>

struct device;

int request_firmware(const struct firmware **firmware_p, const char *name, struct device *device);
void release_firmware(const struct firmware *firmware);

#endif /* _LINUX_FIRMWARE_H_ */
