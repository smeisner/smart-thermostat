/*
*
*
*/

#include "thermostat.h"

int main()
{
	while (1)
	{
		get_sensor_readings();
		sleep(2);
	}
	return 1;
}
