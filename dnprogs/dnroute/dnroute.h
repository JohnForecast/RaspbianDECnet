
struct routeinfo
{
	struct routeinfo *next; /* List of routes to this node/area */
	unsigned short cost;
	unsigned short hops;
	unsigned short router;
	unsigned char valid;
	unsigned char manual;
	unsigned char priority;
};
