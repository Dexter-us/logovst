
#include "logocore.h"

// the number of radians per degree
const FLONUM rads_per_degree = 3.141592653589793227020265931059839203954 / 180.0;

// the number of degrees per radian
const FLONUM degrees_per_rad = 180.0 / 3.141592653589793227020265931059839203954;

// LL is some kind of offset for the Y-axis when pasting bitmaps at the turtle's
// position.  I don't see why it's needed for the Y-axis and not the X-axis, since
// the math is symmetrical.  It doesn't follow the turtle's heading, so it's
// something purely in window coordinates.  I expect that it's really just an
// off-by-one bug which remains due to backward compatibility of programs which
// have compensated for it.
const int LL = 1;
