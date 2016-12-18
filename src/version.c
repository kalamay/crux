#include "../include/crux/version.h"

struct xversion
xversion (void)
{
	return (struct xversion) { VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH };
}
