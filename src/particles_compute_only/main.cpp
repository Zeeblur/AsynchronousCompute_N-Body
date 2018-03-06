#include <iostream>
#include "nbody.h"

int main() {

	nbody simulation((1<<13)); // maximum in release so far with current res settings

	try
	{
		simulation.run();
	}
	catch (const std::runtime_error& e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}

	return 0;
}
