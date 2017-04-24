#ifndef MEDIA_PROCESS_H_
#define MEDIA_PROCESS_H_

#include <cstddef>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include "media_element.h"

using MediaProcessType = enum {
	MediaProcessTypePipe = 1,
	MediaProcessTypeJoin = 2,
	MediaProcessTypeSplit = 3,
	MediaProcessTypeMultiplex = 4,
	MediaProcessTypeGenerator = 5,
	MediaProcessTypeCollapsar = 6,
	MediaProcessTypeRunloop = 7,
};

class MediaProcessInterface {
public:
	MediaProcessInterface() {}
	virtual const MediaProcessType getType() const = 0;
	virtual const size_t getInputCount() const = 0;
	virtual const size_t getOutputCount() const = 0;
	virtual void input(const size_t &index, const std::shared_ptr<BaseMediaElement> &mediaElement) = 0;
	virtual void setOutputHandler(const size_t &index,
		std::function<void(std::shared_ptr<BaseMediaElement>)> outputHandler) = 0;
	virtual bool generate() = 0;
	virtual void interrupt() = 0;
	virtual void setErrorHandler(std::function<bool(const std::exception &)> errorHandler) = 0;
};

class BaseMediaProcess: public MediaProcessInterface {
public:
	BaseMediaProcess(): errorHandler_(nullptr), generator_(nullptr) {};

	template <typename...Args>
	BaseMediaProcess(Args... args):BaseMediaProcess() {
		init(0, args...);
	}

	virtual const size_t getInputCount() const {
		return inputCount_;
	}

	virtual const size_t getOutputCount() const {
		return outputCount_;
	}

	virtual void setErrorHandler(const size_t &index,
		std::function<bool(const std::exception &)> errorHandler) {
		errorHandler_ = errorHandler;
	}

	virtual bool generate() {
		if (generator_) {
			return generator_();
		} else {
			throw std::runtime_error("not impl.");
		}
	}

	virtual void interrupt() {
		auto it = mps_.rbegin();
		auto itEnd = mps_.rend();

		while(it != itEnd) {
			it->get()->interrupt();
			++it;
		}
	}

private:
	template <typename...Args>
	void init(size_t level, std::shared_ptr<BaseMediaProcess> mp, Args... args) {
		if (level == 0) {
			// check if start with a generator
			if (mp->getInputCount() == 0) {
				BaseMediaProcess *mpPtr = mp.get();
				generator_ = [mpPtr]()->bool {
					return mpPtr->generate();
				};
			}
		}
		std::vector<std::shared_ptr<BaseMediaProcess> > mps({mp});
		return init(level, mps, args ...);
	}

	template <typename...Args>
	void init(size_t level, std::vector<std::shared_ptr<BaseMediaProcess> > mps, Args... args) {
		if (level == 0) {
			// collect input
			for (auto mp : mps) {
				// hold it in memory
				mps_.emplace_back(mp);
				BaseMediaProcess *mpPtr = mp.get();
				size_t count = mp->getInputCount();

				for (int i = 0; i < count; ++i) {
					inputHandlers_[inputCount_] = [mpPtr, i](std::shared_ptr<BaseMediaElement> me)->void {
						return mpPtr->input(i, me);
					}
					++inputCount_;
				}
			}
		} else {
			// prev.output -> curr.input
			std::vector<std::function<void(std::shared_ptr<BaseMediaElement>)>> funcs;
			for (auto mp : mps) {
				// hold it in memory
			}
		}
	}
};


#endif // MEDIA_PROCESS_H_