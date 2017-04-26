#include <string>
#include <iostream>
#include <chrono>
#include <memory>
#include <fstream>
#include "media_element.h"
#include "media_process.h"
#include "media_task.h"
#include "boost/filesystem.hpp"

using namespace std;
using namespace boost::filesystem;

void usage(const char *cmd) {
	std::cout << "usage: " << cmd << " image-file-path" << std::endl;
}

class MPProductor: public BaseMediaProcessGenerator {
public:
	MPProductor() {}
	virtual const size_t getInputCount() const {
		return 0;
	}
	
	virtual const size_t getOutputCount() const {
		return 1;
	}

	virtual bool generate() {
		if (--count_) {
			auto me = std::make_shared<BaseMediaElement>();
			me->setMetadata<size_t>("count", count_);
			me->setMetadata<size_t>("step", 0);
			outputHandlers_[0](me);
			return true;
		}
		return false;
	}
private:
	size_t count_ = 10;
};

class MPShow: public BaseMediaProcessCollapsar {
public:
	MPShow() {}
	virtual const size_t getInputCount() const {
		return 1;
	}
	
	virtual const size_t getOutputCount() const {
		return 0;
	}

	virtual void input(const size_t &index, const std::shared_ptr<BaseMediaElement> &mediaElement) {
		auto count = mediaElement->getMetadata<size_t>("count");
		std::cout << "count:" << count << std::endl;
	}
};

/*class Task001: public BaseMediaProcessRunloop {
public:
	Task001(): BaseMediaProcessRunloop(
		std::make_shared<MPProductor>(),
		std::make_shared<MPShow>()
		) {}
};*/

int main(int argc, char const *argv[]) {
	std::cout << "hello media process!" << std::endl;
	if (argc != 2) {
        usage(argv[0]);
        std::exit(1);
    }
    string path = argv[1];
	Task001 t1(path);
	t1.run();
	return 0;
}
