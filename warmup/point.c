#include <assert.h>
#include "common.h"
#include "point.h"
#include <math.h>

void
point_translate(struct point *p, double x, double y)
{
	p->x += x;
	p->y += y;
}

double
point_distance(const struct point *p1, const struct point *p2)
{
	return sqrt(pow(p1->x - p2->x, 2) + pow(p1->y - p2->y, 2));
}

int
point_compare(const struct point *p1, const struct point *p2)
{
	struct point origin;
	origin.x = 0;
	origin.y = 0;
	double p1_length = point_distance(p1, &origin);
	double p2_length = point_distance(p2, &origin);
	
	if (p1_length > p2_length) return 1;
	else if (p1_length == p2_length) return 0;
	else return -1;
}
