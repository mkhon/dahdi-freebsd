#ifndef _LINUX_IO_H_
#define _LINUX_IO_H_

#include <sys/types.h>
#include <sys/rman.h>
#include <machine/bus.h>

static inline void
memset_io(struct resource *mem, bus_size_t offset, u_int8_t value, bus_size_t count)
{
	bus_space_set_region_1(rman_get_bustag(mem), rman_get_bushandle(mem),
	    offset, value, count);
}

static inline void
memcpy_toio(struct resource *mem, bus_size_t offset, const void *datap, bus_size_t count)
{
	bus_space_write_region_1(rman_get_bustag(mem), rman_get_bushandle(mem),
	    offset, datap, count);
}

static inline void
memcpy_fromio(void *datap, struct resource *mem, bus_size_t offset, bus_size_t count)
{
	bus_space_read_region_1(rman_get_bustag(mem), rman_get_bushandle(mem),
	    offset, datap, count);
}

#endif /* _LINUX_IO_H_ */
