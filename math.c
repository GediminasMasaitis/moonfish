#include "moonfish.h"

/*
our approximation:
tanh(x) ~ (x**5/945 + x**3/9 + x)/(x**4/63 + (4 * x**2)/9 + 1)
*/

int moonfish_tanh(int x)
{
	long int x5, x4, x3, x2, x1, r;
	
	x5 = 1, x4 = 1, x3 = 1, x2 = 1, x1 = 1;
	
	x5 *= x; x5 *= x; x5 *= x; x5 *= x; x5 *= x;
	x4 *= x; x4 *= x; x4 *= x; x4 *= x;
	x3 *= x; x3 *= x; x3 *= x;
	x2 *= x; x2 *= x;
	x1 *= x;
	
	x5 /= 945;
	x4 /= 63;
	x3 /= 9;
	x2 *= 4;
	x2 /= 9;
	x1 *= 127;
	
	x5 /= 127 * 127 * 127;
	x4 /= 127 * 127 * 127;
	x3 /= 127;
	x2 /= 127;
	
	r = (x5 + x3 + x1) / (x4 + x2 + 127);
	if (r > 127) return 127;
	return r;
}
