#include "../include/crux.h"
#include "../include/crux/hub.h"
#include "../include/crux/readline.h"

#include <stdlib.h>
#include <string.h>

int
main(void)
{
	xinit();
	for (;;) {
		char *l = xreadline("test> ");
		if (l == NULL) { break; }
		xwriten(2, l, strlen(l), -1);
		xwriten(2, "\n", 1, -1);
		free(l);
	}
}

