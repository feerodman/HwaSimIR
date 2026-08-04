#include <cstdlib>
#include <iostream>

extern "C"
{
#include <rk_mpi.h>
#include <rk_venc_cfg.h>
#include <mpp_buffer.h>
}

int main()
{
	MppCtx context = nullptr;
	MppApi* api = nullptr;
	const MPP_RET createResult = mpp_create(&context, &api);
	if (createResult != MPP_OK || !context || !api)
	{
		std::cerr << "mpp_create failed: " << static_cast<int>(createResult) << '\n';
		return EXIT_FAILURE;
	}

	const MPP_RET initResult =
		mpp_init(context, MPP_CTX_ENC, MPP_VIDEO_CodingAVC);
	if (initResult != MPP_OK)
	{
		std::cerr << "mpp_init AVC encoder failed: "
			<< static_cast<int>(initResult) << '\n';
		mpp_destroy(context);
		return EXIT_FAILURE;
	}

	MppEncCfg config = nullptr;
	const MPP_RET configResult = mpp_enc_cfg_init(&config);
	if (configResult != MPP_OK || !config)
	{
		std::cerr << "mpp_enc_cfg_init failed: "
			<< static_cast<int>(configResult) << '\n';
		mpp_destroy(context);
		return EXIT_FAILURE;
	}

	mpp_enc_cfg_deinit(config);
	mpp_destroy(context);
	std::cout << "RKMPP AVC API check passed\n";
	return EXIT_SUCCESS;
}
