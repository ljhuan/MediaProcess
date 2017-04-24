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
				mps_.emplace_back(mp);
				BaseMediaElement* mpPtr = mp.get();
				size_t count = mp->getInputCount();
				for (int i = 0; i < count; ++i) {
					funcs.emplace_back([mpPtr, i](std::shared_ptr<BaseMediaElement> me)->void {
						return mpPtr->input(i, me);
					});
				}
			}

			if (funcs.size() != prevOutputCount_) {
				throw (std::runtime_error("previous output not match current input"));
			}

			size_t j = 0;
			for (auto mp : mpsPrev_) {
				size_t count = mp->getOutputCount();
				for (int i = 0; i < count; ++i) {
					mp->setOutputHandler(i, funcs[j]);
					++j;
				}
			}
		}

		// save to prev
		mpsPrev_.clear();
		prevOutputCount_ = 0;
		for (auto mp : mps) {
			mpsPrev_.emplace_back(mp);
			prevOutputCount_ += mp->getOutputCount();
		}
		return init(level+1, args ...);
	}

	template <typename...Args>
	void init(size_t level) {
		// last
		size_t j = 0;
		for (auto mp : mpsPrev_) {
			size_t count = mp->getOutputCount();
			outputCount_ += count;
			for (int i = 0; i < count; ++i) {
				mp->setOutputHandler(i, [this, j](std::shared_ptr<BaseMediaElement> me)->void {
					if (this->outputHandlers_.find(j) != this->outputHandlers_.end()) {
						this->outputHandlers_[j](me);
					}
				});
				++j;
			}
		}
	}

private:
	size_t inputCount_ = 0;
	size_t outputCount_ = 0;

	std std::vector<std::shared_ptr<BaseMediaProcess> > mpsPrev_;
	size_t prevOutputCount_ = 0;

	std::vector<std::shared_ptr<BaseMediaProcess> > mps_;
	std::function<bool(const std::exception &)> errorHandler_;
	std::map<size_t, std::function<void(std::shared_ptr<BaseMediaElement>)> > outputHandlers_;

	// input handler can only changed in self or derived class.
	std::map<size_t, std::function<void(std::shared_ptr<BaseMediaElement>)> > inputHandlers_;

	// generator proxy
	std::function<bool()> generator_;
};

class BaseMediaProcessPipe: public BaseMediaProcess {
public:
	BaseMediaProcessPipe() {}

	template <typename...Args>
	BaseMediaProcessPipe(Args...args): BaseMediaProcess(args...) {
		assert(getOutputCount() == 1);
		assert(getInputCount() == 1);
	}

	virtual const MediaProcessType getType() const {
		return MediaProcessTypePipe;
	}

	virtual bool generate() {
		throw std::runtime_error("not support.");
	}
};

class BaseMediaProcessJoin: public BaseMediaProcess {
public:
	BaseMediaProcessJoin() {}
	
	template <typename...Args>
	BaseMediaProcessPipe(Args...args): BaseMediaProcess(args...) {
		assert(getOutputCount() == 1);
	}

	virtual const MediaProcessType getType() const {
		return MediaProcessTypeJoin;
	}

	virtual bool generate() {
		throw std::runtime_error("not support.");
	}
};

class BaseMediaProcessSplit: public BaseMediaProcess {
public:
	BaseMediaProcessSplit() {}
	
	template <typename...Args>
	BaseMediaProcessPipe(Args...args): BaseMediaProcess(args...) {
		assert(getInputCount() == 1);
	}

	virtual const MediaProcessType getType() const {
		return MediaProcessTypeSplit;
	}

	virtual bool generate() {
		throw std::runtime_error("not support.");
	}
};

class BaseMediaProcessMultiplex: public BaseMediaProcess {
public:
	BaseMediaProcessMultiplex() {}
	
	template <typename...Args>
	BaseMediaProcessPipe(Args...args): BaseMediaProcess(args...) {
	}

	virtual const MediaProcessType getType() const {
		return MediaProcessTypeMultiplex;
	}

	virtual bool generate() {
		throw std::runtime_error("not support.");
	}
};

// base 1 output generator
class BaseMediaProcessGenerator: public BaseMediaProcess {
public:
	BaseMediaProcessGenerator() {}
	
	template <typename...Args>
	BaseMediaProcessPipe(Args...args): BaseMediaProcess(args...) {
		assert(getInputCount() == 0);
	}

	virtual const MediaProcessType getType() const {
		return MediaProcessTypeGenerator;
	}

	virtual void input(const std::string &name, const std::shared_ptr<BaseMediaElement> &mediaElement) {
		throw std::runtime_error("not support.");
	}

	virtual bool generate() {
		return false;
	}
};

class BaseMediaProcessCollapsar: public BaseMediaProcess {
public:
	BaseMediaProcessCollapsar() {}
	
	template <typename...Args>
	BaseMediaProcessPipe(Args...args): BaseMediaProcess(args...) {
		assert(getOutputCount() == 0);
	}

	virtual const MediaProcessType getType() const {
		return MediaProcessTypeCollapsar;
	}

	virtual bool generate() {
		throw std::runtime_error("not support.");
	}
};

class BaseMediaProcessRunloop: public BaseMediaProcess {
public:
	BaseMediaProcessRunloop() {}
	
	template <typename...Args>
	BaseMediaProcessPipe(Args...args): BaseMediaProcess(args...) {
		assert(getOutputCount() == 0);
		assert(getInputCount() == 0);
	}

	virtual const MediaProcessType getType() const {
		return MediaProcessTypeRunloop;
	}

	virtual void run() {
		{
			std::unique_lock<std::mutex> lock(mutex_);
			running_ = true;
		}

		while (running_ && generate()) {
		}

		{
			std::unique_lock<std::mutex> lock(mutex_);
			running_ = false;
		}
	}

	virtual void start() {
		std::unique_lock<std::mutex> lock(mutex_);
		if (!running_) {
			std::thread t(&BaseMediaProcessRunloop::run, this);
			proc_.swap(t);
		}
	}

	virtual void stop() {
		{
			std::unique_lock<std::mutex> lock(mutex_);
			if (!running_) {
				return;
			} else {
				running_ = false;
			}
		}

		interrupt();
		if (proc_.joinable()) {
			proc_.join();
		}
	}

protected:
	std::mutex mutex_;
	std::thread proc_;
	bool running_ = false;
};

#endif // MEDIA_PROCESS_H_