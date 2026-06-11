#include "HwaSimIR.h"

// ========== main函数 ==========
int main(int argc, char *argv[]) {
	// 创建主应用实例
	HwaSimIR app(argc, argv);

	// 启动应用
	app.run();

	// 析构自动清理资源
	return 0;
}
