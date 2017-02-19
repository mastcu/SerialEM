#pragma once

class Smoother {
	int mRingSize;		//# of values to average
	int mNumInRing;	//# actually in the ring so far
	double mRingSum;	//current sum
	double mRingAvg;	//current average
	double *mRingStrt;	//Pointer to start of ring
	double *mRingp;		//Pointer to current spot in ring
	//double mScale;		//scaling factor to integer readout
	double mThresh1;	//Threshold |value - average| for value to govern
	double mThresh2;	//Threshold |average - governor| for average to govern
	double mGovernor;	//Last value that governed the readout

public:
	Smoother();
	~Smoother();
	int Setup(int navg, double thresh1, double thresh2 /*, double scale*/);
	double Readout(double value);
};
