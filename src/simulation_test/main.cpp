#include <iostream>
#include "nbody.h"
#include "args.h"

int main(int argc, const char *argv[])
{
	args::ArgumentParser parser("This is a test program.", "This goes after the options.");
	args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
	args::Group group(parser, "\nChoose what GPU vendor:", args::Group::Validators::Xor);
	args::Flag amd(group, "AMD", "Use an AMD GPU", { 'a', "amd" });
	args::Flag nvidia(group, "NVIDIA", "Use a NVIDIA GPU", { 'n', "nvidia" });
	args::ValueFlag<int> particleCount(parser, "Particle Count", "Set the number of particles.", { 'p', "particles" });
	args::ValueFlag<int> stackCount(parser, "Stack Count", "Set the number of stacks within the particle geometry", { 's', "st", "stacks" });
	args::ValueFlag<int> sliceCount(parser, "Slice Count", "Set the number of slices within the particle geometry", { 'l', "sl", "slices" });

	args::Group group2(parser, "What mode the simulation uses:", args::Group::Validators::Xor);
	args::Flag mode1(group2, "Compute Only", "Run the simulation using normal compute.", { 'c', "compute" });
	args::Flag mode2(group2, "Async Transfer", "Run the simulation using Asynchronous Compute - Transfer Method", { 't', "transfer" });
	args::Flag mode3(group2, "Async Double Buffer", "Run the simulation using Asynchronous Compute - Double Buffering", { 'd', "double" });

	args::ValueFlag<int> expTime(parser, "Experiment Time", "Set how long in MINUTES to run the experiment for.", { 'm', "minutes", });

	args::CompletionFlag completion(parser, { "complete" });
	try
	{
		parser.ParseCLI(argc, argv);
	}
	catch (args::Completion e)
	{
		std::cout << e.what();
		return 0;
	}
	catch (args::Help)
	{
		std::cout << parser;
		return 0;
	}
	catch (args::ParseError e)
	{
		std::cerr << e.what() << std::endl;
		std::cerr << parser;
		return 1;
	}
	catch (args::ValidationError e)
	{
		std::cerr << e.what() << std::endl;
		std::cerr << parser;
		return 1;
	}

	if (particleCount) { std::cout << "particles: " << args::get(particleCount) << std::endl; }
	if (stackCount) { std::cout << "stacks: " << args::get(stackCount) << std::endl; }
	if (sliceCount) { std::cout << "slices: " << args::get(sliceCount) << std::endl; }


	//nbody simulation((1<<12)); // maximum in release so far with current res settings

	//try
	//{
	//	simulation.run();
	//}
	//catch (const std::runtime_error& e)
	//{
	//	std::cerr << e.what() << std::endl;
	//	return -1;
	//}

	return 0;
}
