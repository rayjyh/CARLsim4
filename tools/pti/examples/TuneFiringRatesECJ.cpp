/*! \brief
 *
 */
#include "PTI.h"
#include <carlsim.h>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <vector>

using namespace std;

class TuneFiringRatesECJExperiment : public Experiment {
public:
	// Decay constants
	static const float COND_tAMPA=5.0, COND_tNMDA=150.0, COND_tGABAa=6.0, COND_tGABAb=150.0;
	
	// Neurons
	static const int NUM_NEURONS = 10;

	// Simulation time in seconds
	static const int runTime = 2;

	// Target rates for the objective function
	static const float INPUT_TARGET_HZ = 30.0f;
	static const float EXC_TARGET_HZ   = 10.0f;
	static const float INH_TARGET_HZ   = 20.0f;

    const LoggerMode verbosity;

	TuneFiringRatesECJExperiment(const LoggerMode verbosity): verbosity(verbosity) {}

	void run(const ParameterInstances &parameters, std::ostream &outputStream) const {
		// Izhikevich parameters
		const float REG_IZH[] = { 0.02f, 0.2f, -65.0f, 8.0f };
		const float FAST_IZH[] = { 0.1f, 0.2f, -65.0f, 2.0f };

		int indiNum = parameters.getNumInstances();

		int poissonGroup[indiNum];
		int excGroup[indiNum];
		int inhGroup[indiNum];
		SpikeMonitor* excMonitor[indiNum];
		SpikeMonitor* inhMonitor[indiNum];
		float excHz[indiNum];
		float inhHz[indiNum];
		float excError[indiNum];
		float inhError[indiNum];
		float fitness[indiNum];

		/** construct a CARLsim network on the heap. */
		CARLsim* const network = new CARLsim("tuneFiringRatesECJ", CPU_MODE, verbosity);

		assert(parameters.getNumParameters() >= 4);
		for(unsigned int i = 0; i < parameters.getNumInstances(); i++) {
			/** Decode a genome*/
			poissonGroup[i] = network->createSpikeGeneratorGroup("poisson", NUM_NEURONS, EXCITATORY_NEURON);
			excGroup[i] = network->createGroup("exc", NUM_NEURONS, EXCITATORY_NEURON);
			inhGroup[i] = network->createGroup("inh", NUM_NEURONS, INHIBITORY_NEURON);

			network->setNeuronParameters(excGroup[i], REG_IZH[0], REG_IZH[1], REG_IZH[2], REG_IZH[3]);
			network->setNeuronParameters(inhGroup[i], FAST_IZH[0], FAST_IZH[1], FAST_IZH[2], FAST_IZH[3]);
			network->setConductances(true,COND_tAMPA,COND_tNMDA,COND_tGABAa,COND_tGABAb);

			network->connect(poissonGroup[i], excGroup[i], "random", RangeWeight(parameters.getParameter(i,0)), 0.5f, RangeDelay(1));
			network->connect(excGroup[i], excGroup[i], "random", RangeWeight(parameters.getParameter(i,1)), 0.5f, RangeDelay(1));
			network->connect(excGroup[i], inhGroup[i], "random", RangeWeight(parameters.getParameter(i,2)), 0.5f, RangeDelay(1));
			network->connect(inhGroup[i], excGroup[i], "random", RangeWeight(parameters.getParameter(i,3)), 0.5f, RangeDelay(1));

		}

		// can't call setupNetwork() multiple times in the loop
		network->setupNetwork();

		// it's unnecessary to do this in the loop
		PoissonRate* const in = new PoissonRate(NUM_NEURONS);
		/*for (int k=0;k<NUM_NEURONS;k++)*/
			/*in->rates[k] = INPUT_TARGET_HZ;*/
		in->setRates(INPUT_TARGET_HZ);

		for(unsigned int i = 0; i < parameters.getNumInstances(); i++) {
			network->setSpikeRate(poissonGroup[i],in);

			excMonitor[i] = network->setSpikeMonitor(excGroup[i], "/dev/null");
			inhMonitor[i] = network->setSpikeMonitor(inhGroup[i], "/dev/null");

			excMonitor[i]->startRecording();
			inhMonitor[i]->startRecording();

			// initialize all the error and fitness variables
			excHz[i]=0; inhHz[i]=0;
			excError[i]=0; inhError[i]=0;
			fitness[i]=0;
		}
		// again, we can't call this more than once.
		network->runNetwork(runTime,0);

		for(unsigned int i = 0; i < parameters.getNumInstances(); i++) {

			excMonitor[i]->stopRecording();
			inhMonitor[i]->stopRecording();

			excHz[i] = excMonitor[i]->getPopMeanFiringRate();
			inhHz[i] = inhMonitor[i]->getPopMeanFiringRate();

			excError[i] = fabs(excHz[i] - EXC_TARGET_HZ);
			inhError[i] = fabs(inhHz[i] - INH_TARGET_HZ);

			fitness[i] = 1/(excError[i] + inhError[i]);
			outputStream << fitness[i] << endl;
		}
		delete network;
		delete in;
	}
};

/** Returns true iff the command-line arguments contain "-parameter". */
const bool hasOpt(int argc, const char * const argv[], const char * const parameter) {
  assert(argc >= 0);
  assert(argv != NULL);
  assert(parameter != NULL);

  for (int i = 1; i < argc; i++) {
    char dashParam[strlen(parameter) + 1];
    strcpy(dashParam, "-");
    strcat(dashParam, parameter);
    if (0 == strcmp(dashParam, argv[i]))
         return true;
  }
  return false;
}

int main(int argc, char* argv[]) {
	/* First we Initialize an Experiment and a PTI object.  The PTI parses CLI
	* arguments, and then loads the Parameters from a file (if one has been
	* specified by the user) or else from a default istream (std::cin here). */

  	const LoggerMode verbosity = hasOpt(argc, argv, "v") ? USER : SILENT;
	const TuneFiringRatesECJExperiment experiment(verbosity);
	const PTI pti(argc, argv, std::cout, std::cin);

	/* The PTI will now cheerfully iterate through all the Parameter sets and
	* run your Experiment on it, printing the results to the specified
	* ostream (std::cout here). */
	pti.runExperiment(experiment);

	return 0;
}
