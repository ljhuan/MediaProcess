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
    MediaProcessInterface() {};

    virtual const MediaProcessType getType() const = 0;

    virtual const size_t getInputCount() const = 0;

    virtual const size_t getOutputCount() const = 0;

    virtual void input(const size_t &index, const std::shared_ptr<BaseMediaElement> &mediaElement) = 0;

    virtual void setOutputHandler(const size_t &index,
                                  std::function<void(std::shared_ptr<BaseMediaElement>)> outputHandler) = 0;

    // for generator only, return continue flag, if true continue, else break.
    virtual bool generate() = 0;

    // interrupt current generate/input
    virtual void interrupt() = 0;

    // error handle for generate/input.
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

    virtual void setErrorHandler(std::function<bool(const std::exception &)> errorHandler) {
        errorHandler_ = errorHandler;
    }

    virtual void input(const size_t &index, const std::shared_ptr<BaseMediaElement> &mediaElement) {
        return inputHandlers_[index](mediaElement);
    }

    virtual void setOutputHandler(const size_t &index,
                                  std::function<void(std::shared_ptr<BaseMediaElement>)> outputHandler) {
        outputHandlers_[index] = outputHandler;
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

        while (it != itEnd) {
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
                generator_ = [mpPtr]() -> bool {
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
                // hold it in memory.
                mps_.emplace_back(mp);
                BaseMediaProcess *mpPtr = mp.get();
                size_t count = mp->getInputCount();

                for (size_t i = 0; i < count; ++i) {
                    inputHandlers_[inputCount_] = [mpPtr, i] (std::shared_ptr<BaseMediaElement> me) -> void {
                        return mpPtr->input(i, me);
                    };
                    ++inputCount_;
                }
            }
        } else {
            // prev.output -> curr.input
            std::vector<std::function<void(std::shared_ptr<BaseMediaElement>)>> funcs;
            for (auto mp : mps) {
                // hold it in memory
                mps_.emplace_back(mp);
                BaseMediaProcess *mpPtr = mp.get();
                size_t count = mp->getInputCount();
                for (size_t i = 0; i < count; ++i) {
                    funcs.emplace_back([mpPtr, i](std::shared_ptr<BaseMediaElement> me) -> void {
                        mpPtr->input(i, me);
                    });
                }
            }

            if (funcs.size() != prevOutputCount_) {
                throw(std::runtime_error("previous output not match current input."));
            }

            size_t j = 0;
            for (auto mp : mpsPrev_) {
                size_t count = mp->getOutputCount();
                for (size_t i = 0; i < count; ++i) {
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
        return init(level+1, args...);
    }

    template <typename...Args>
    void init(size_t level) {
        // last
        size_t j = 0;
        for (auto mp : mpsPrev_) {
            size_t count = mp->getOutputCount();
            outputCount_ += count;
            for (size_t i = 0; i < count; ++i) {
                mp->setOutputHandler(i, [this, j](std::shared_ptr<BaseMediaElement> me) -> void {
                    if (this->outputHandlers_.find(j) != this->outputHandlers_.end()) {
                        this->outputHandlers_[j](me);
                    }
                });
                ++j;
            }
        }
    }


 protected:
    size_t inputCount_ = 0;
    size_t outputCount_ = 0;


    std::vector<std::shared_ptr<BaseMediaProcess> > mpsPrev_;
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

    virtual bool generate()  {
        throw std::runtime_error("not support.");
    }
};


class BaseMediaProcessJoin: public BaseMediaProcess {
 public:
    BaseMediaProcessJoin() {}

    template <typename...Args>
    BaseMediaProcessJoin(Args...args): BaseMediaProcess(args...) {
            assert(getOutputCount() == 1);
    }

    virtual const MediaProcessType getType() const {
        return MediaProcessTypeJoin;
    }

    virtual bool generate()  {
        throw std::runtime_error("not support.");
    }
};

class BaseMediaProcessSplit: public BaseMediaProcess {
public:
    BaseMediaProcessSplit() {}

    template <typename...Args>
    BaseMediaProcessSplit(Args...args): BaseMediaProcess(args...) {
            assert(getInputCount() == 1);
    }

    virtual const MediaProcessType getType() const {
        return MediaProcessTypeSplit;
    }

    virtual bool generate()  {
        throw std::runtime_error("not support.");
    }
};


class BaseMediaProcessMultiplex: public BaseMediaProcess {
 public:
    BaseMediaProcessMultiplex() {}

    template <typename...Args>
    BaseMediaProcessMultiplex(Args...args): BaseMediaProcess(args...) {
    }

    virtual const MediaProcessType getType() const {
        return MediaProcessTypeMultiplex;
    }

    virtual bool generate()  {
        throw std::runtime_error("not support.");
    }
};


// base 1 output generator
class BaseMediaProcessGenerator: public BaseMediaProcess {
public:
    BaseMediaProcessGenerator() {}

    template <typename...Args>
    BaseMediaProcessGenerator(Args...args): BaseMediaProcess(args...) {
        assert(getInputCount() == 0);
    }

    virtual const MediaProcessType getType() const {
        return MediaProcessTypeGenerator;
    }

    virtual void input(const std::string &name, const std::shared_ptr<BaseMediaElement> &mediaElement) {
        throw std::runtime_error("not support.");
    }

    virtual bool generate()  {
        return false;
    }
};


class BaseMediaProcessCollapsar: public BaseMediaProcess {
 public:
    BaseMediaProcessCollapsar() {}

    template <typename...Args>
    BaseMediaProcessCollapsar(Args...args): BaseMediaProcess(args...) {
        assert(getOutputCount() == 0);
    }

    virtual const MediaProcessType getType() const {
        return MediaProcessTypeCollapsar;
    }

    virtual bool generate()  {
        throw std::runtime_error("not support.");
    }
};


class BaseMediaProcessRunloop: public BaseMediaProcess {
public:
    BaseMediaProcessRunloop() {}

    template <typename...Args>
    BaseMediaProcessRunloop(Args...args): BaseMediaProcess(args...) {
        assert(getInputCount() == 0);
        assert(getOutputCount() == 0);
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
    bool running_  = false;
};


class BaseMediaProcessThreadedPipe : public BaseMediaProcessPipe {
public:
    explicit BaseMediaProcessThreadedPipe(const uint8_t count = 1) : count_(count) {
    }

    ~BaseMediaProcessThreadedPipe() {
        stop(true);
        wait();
    }

    virtual const size_t getInputCount() const {
        return 1;
    }

    virtual const size_t getOutputCount() const {
        return 1;
    }

    virtual void input(const size_t &index, const std::shared_ptr<BaseMediaElement> &mediaElement) {
        boost::unique_lock<boost::mutex> lock(mutex_);
        while (running_) {
            if (me_) {
                // wait out event
                meCondOut_.wait(lock);
            }
            if ((!me_) && running_) {
                me_ = mediaElement;
                meCondIn_.notify_one();
                return;
            }
        }
    }

    virtual void process(const std::shared_ptr<BaseMediaElement> &mediaElement) {
        throw std::runtime_error("not impl.");
    }

    virtual void start() {
        reset();
        boost::unique_lock<boost::mutex> lock(mutex_);
        running_ = true;
        for (uint8_t i = 0; i < count_; ++i) {
            threads_.emplace_back(&BaseMediaProcessThreadedPipe::run_, this);
        }
    }

    virtual void stop(bool graceful = true) {
        boost::unique_lock<boost::mutex> lock(mutex_);
        stopGraceful_ = graceful;
        running_ = false;
        cond_.notify_all();
        meCondOut_.notify_all();
        meCondIn_.notify_all();
    }

    virtual void wait() {
        std::vector<boost::thread> ts;
        {
            boost::unique_lock<boost::mutex> lock(mutex_);
            ts.swap(threads_);
        }

        std::vector<boost::thread>::iterator tEnd = ts.end();
        std::vector<boost::thread>::iterator t = ts.begin();
        while (t != tEnd) {
            if (t->joinable()) {
                t->join();
            }
            ++t;
        }
    }

    virtual void reset() {
        stop(true);
        wait();

        assert(threads_.empty());
        boost::unique_lock<boost::mutex> lock(mutex_);
        me_ = nullptr;
        meCondOut_.notify_one();
    }

private:
    void run_() {
        while (running_) {
            std::shared_ptr<BaseMediaElement> currMe(nullptr);
            // try pick a media-element from input.
            {
                boost::unique_lock<boost::mutex> lock(mutex_);
                if (!me_) {
                    meCondIn_.wait(lock);
                }

                if (running_ && me_) {
                    me_.swap(currMe);
                    meCondOut_.notify_one();
                }
            }

            // run with post operation
            if (running_ && currMe) {
                process(currMe);

                // post output operation is single-thread.
                std::unique_lock<std::mutex> lock(postRunMutex_);
                if (running_) {
                    if (outputHandlers_.find(0) != outputHandlers_.end()) {
                        outputHandlers_[0](currMe);
                    }
                }
            }
        }

        // last call for graceful exit.
        std::unique_lock<std::mutex> lock(postRunMutex_);
        if (stopGraceful_) {
            if (me_) {
                if (outputHandlers_.find(0) != outputHandlers_.end()) {
                    outputHandlers_[0](me_);
                }
                me_ = nullptr;
            }
        }
    }

protected:
    bool running_ = false;

    // thread count
    uint8_t count_;

    // global mutex
    boost::mutex mutex_;

    // can using wait for interrupt
    boost::condition_variable cond_;

private:
    boost::condition_variable meCondIn_;
    boost::condition_variable meCondOut_;
    std::shared_ptr<BaseMediaElement> me_ = nullptr;

    std::vector<boost::thread> threads_;

    std::mutex postRunMutex_;

    bool stopGraceful_ = true;
};


class BaseMediaProcessCachePipe : public BaseMediaProcessPipe {
public:
    explicit BaseMediaProcessCachePipe(const size_t lowLevel = 0, const size_t highLevel = SIZE_MAX) :
        BaseMediaProcessPipe(), lowLevel_(lowLevel), highLevel_(highLevel) {
    }

    ~BaseMediaProcessCachePipe() {
        stop();
        wait();
    }

    virtual const size_t getInputCount() const {
        return 1;
    }

    virtual const size_t getOutputCount() const {
        return 1;
    }

    virtual bool dealHighLevel(const std::shared_ptr<BaseMediaElement> &mediaElement) {
        return false;
    };

    virtual void input(const size_t &index, const std::shared_ptr<BaseMediaElement> &mediaElement) {
        boost::unique_lock<boost::mutex> lock(mutex_);
        while (running_) {
            while (cache_.size() >= highLevel_) {
                // block here
                if (dealHighLevel(mediaElement)) {
                    return;
                }
                enterLowCond_.wait(lock);
            }

            if (running_ && (cache_.size() < highLevel_)) {
                cache_.insert(mediaElement);
                if (cache_.size() == 1) {
                    // first
                    firstCond_.notify_one();
                }
                return;
            }
        }
    }

    virtual void start() {
        reset();
        boost::unique_lock<boost::mutex> lock(mutex_);
        running_ = true;
        boost::thread t(&BaseMediaProcessCachePipe::run_, this);
        proc_.swap(t);
    }

    virtual void stop(bool graceful = true) {
        boost::unique_lock<boost::mutex> lock(mutex_);
        stopGraceful_ = graceful;
        running_ = false;
        enterLowCond_.notify_all();
        enterLowCond_.notify_all();
        firstCond_.notify_all();
    }

    virtual void wait() {
        if (proc_.joinable()) {
            proc_.join();
        }
    }

    virtual void reset() {
        stop(true);
        wait();

        boost::unique_lock<boost::mutex> lock(mutex_);
        cache_.clear();
    }

private:
    void run_() {
        while (running_) {
            std::shared_ptr<BaseMediaElement> me = nullptr;

            // pick out
            {
                boost::unique_lock<boost::mutex> lock(mutex_);
                if (cache_.size() > 0) {
                    std::set<std::shared_ptr<BaseMediaElement> >::iterator x = cache_.begin();
                    me = *x;

                    cache_.erase(x);
                    if (cache_.size() == lowLevel_) {
                        enterLowCond_.notify_one();
                    }
                }
                else {
                    // wait new till 1 second.
                    firstCond_.wait_for(lock, boost::chrono::seconds(1));
                }
            }

            if (me) {
                if (outputHandlers_.find(0) != outputHandlers_.end()) {
                    outputHandlers_[0](me);
                }
            }
        }

        if (stopGraceful_) {
            // process remain data.
            boost::unique_lock<boost::mutex> lock(mutex_);
            if (outputHandlers_.find(0) != outputHandlers_.end()) {
                for (auto me : cache_) {
                    outputHandlers_[0](me);
                }
            }
            cache_.clear();
        }

        running_ = false;
    }

private:
    bool running_ = false;
    boost::mutex mutex_;
    boost::condition_variable enterLowCond_;
    boost::condition_variable enterHighCond_;
    boost::condition_variable firstCond_;
    boost::thread proc_;

    size_t lowLevel_;
    size_t highLevel_;
    std::set<std::shared_ptr<BaseMediaElement> > cache_;
    bool stopGraceful_ = true;
};

#endif // MEDIA_PROCESS_H_