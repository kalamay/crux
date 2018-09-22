#ifndef CRUX_H
#define CRUX_H

#include "crux/version.h"
#include "crux/def.h"
#include "crux/err.h"
#include "crux/clock.h"
#include "crux/buf.h"
#include "crux/num.h"
#include "crux/value.h"

XEXTERN int
xinit(void);

XEXTERN int
xinit_thread(void);

#endif

