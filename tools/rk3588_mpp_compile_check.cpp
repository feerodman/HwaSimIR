#include <iostream>
#include "VideoEncoder.h"

int main()
{
	H264MppEncoder encoder;
	encoder.requestKeyFrame();
	std::cout << "RKMPP production encoder API link check passed: "
		<< encoder.name() << '\n';
	return encoder.isAvailable() ? 0 : 1;
}
