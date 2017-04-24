##ifndef  MEDIA_ELEMENT_H_
#define MEDIA_ELEMENT_H_

#include <cstddef>
#include <memory>
#include <map>
#include "boost/thread.hpp"
#include "boost/archive/text_iarchive.hpp"
#include "boost/archive/text_oarchive.hpp"

class BaseMediaBuffer {
public:
	BaseMediaBuffer(const size_t &size):size_(size) {
		if(size_) {
			data_ = new uint8_t[size_];
		} else {
			data_ = nullptr;
		}
	}

	virtual ~BaseMediaBuffer() {
		if (data_) {
			delete [] data_;
		}
	}

	void resize(const size_t &size) {
		uint8_t *newData = new uint8_t[size];
		size_t copyLength = std::min(size, size_);
		::memcpy(newData, data_, copyLength);
		delete [] data_;
		data_ = newData;
		size_ = size;
	}

	const size_t size() const {
		return size_;
	}

	uint8_t *data() const {
		return data_;
	}

protected:
	size_t size_;
	uint8_t *data_;
};

class BaseMediaElement {
public:
	BaseMediaElement();
	~BaseMediaElement();
	std::shared_ptr<BaseMediaBuffer> getMediaBuffer(const std::string &name) {
		boost::shared_lock<boost::shared_mutex> rlock(mediaDataMutex_);
		auto it = mediaData_.find(name);
		if (it != mediaData_.end()) {
			return it->second;
		} else {
			return nullptr;
		}
	}

	void setMediaBuffer(const std::string &name, const std::shared_ptr<BaseMediaBuffer> &mediaBuffer) {
		boost::unique_lock<boost::shared_mutex> wlock(mediaDataMutex_);
		mediaData_[name] = mediaBuffer;
	}

	template <typename T>
	const T getMetadata(const std::string &name) const {
		T v;
		getMetadata(name, &v);
		return std::forward<T>(v);
	}

	template <typename T>
	void getMetadata(const std::string &name, T *out) const {
		boost::shared_lock<boost::shared_mutex> rlock(metadataMutex_);
		auto it = metadata_.find(name);
		if (it != metadata_.end()) {
			std::istringstream is(it->second);
			boost::archive::text_iarchive ia(is);
			ia >> *out;
		} else {
			throw std::runtime_error("no such key in metadata.");
		}
	}

	template <typename T>
	void setMetadata(const std::string &name, const T &value) {
		std::ostringstream os;
		boost::archive::text_oarchive oa(os);
		oa << value;
		boost::unique_lock<boost::shared_mutex> wlock(metadataMutex_);
		metadata_[name] = os.str();
	}

private:
	mutable boost::shared_mutex metadataMutex_;
	std::map<std::string, std::string> metadata_;

	mutable boost::shared_mutex mediaDataMutex_;
	std::map<std::string, std::shared_ptr<BaseMediaBuffer> > mediaData_;
};

#endif  // MEDIA_ELEMENT_H_