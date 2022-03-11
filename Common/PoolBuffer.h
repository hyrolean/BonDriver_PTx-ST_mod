//===========================================================================
#pragma once
#ifndef _POOLBUFFER_20210321220639424_H_INCLUDED_
#define _POOLBUFFER_20210321220639424_H_INCLUDED_
//---------------------------------------------------------------------------

#include <vector>
#include <functional>
//===========================================================================
namespace PRY8EAlByw {
//---------------------------------------------------------------------------

  // fixed_queue (アロケーションが発生しない順列)

template<typename T>
class fixed_queue
{
public:
  typedef T value_type ;
  typedef T& reference_type ;
  typedef T* pointer_type ;
  typedef size_t size_type ;
protected:
  pointer_type buff_ ;
  size_type cue_ ;
  size_type size_ ;
  size_type grew_ ;
public:
  fixed_queue(size_type fixed_size)
    : grew_(fixed_size), cue_(0), size_(0) {
    buff_ = new value_type[grew_] ;
  }
  ~fixed_queue() {
    delete [] buff_ ;
  }
  size_type capacity() const { return grew_ ; }
  size_type size() const { return size_ ; }
  bool empty() const { return size_==0 ; }
  bool full() const { return size_>=grew_ ; }
  bool push(const value_type &val) {
    if(full()) return false ;
    buff_[cue_+size_-(cue_+size_<grew_?0:grew_)] = val ;
    size_++ ;
    return true ;
  }
  bool push_front(const value_type &val) {
    if(full()) return false ;
	buff_[cue_?--cue_:cue_=grew_-1] = val ;
	size_++ ;
	return true ;
  }
  bool pop() {
    if(empty()) return false ;
    if(++cue_>=grew_) cue_=0 ;
    size_-- ;
    return true ;
  }
  bool pop_back() {
    if(empty()) return false ;
    size_-- ;
    return true ;
  }
  void clear() { cue_ = 0 ; size_ = 0 ; }
  reference_type front() {
  #ifdef _DEBUG
    if(empty()) throw std::range_error("fixed_queue: front() range error.") ;
  #endif
    return buff_[cue_] ;
  }
  reference_type back() {
  #ifdef _DEBUG
    if(empty()) throw std::range_error("fixed_queue: back() range error.") ;
  #endif
    return buff_[cue_+size_-1-(cue_+size_-1<grew_?0:grew_)] ;
  }
  reference_type operator[](size_type index) {
  #ifdef _DEBUG
    if(index>=size()) throw std::range_error("fixed_queue: operator[] range error.") ;
  #endif
    return buff_[cue_+index-(cue_+index<grew_?0:grew_)] ;
  }
};

  // pool_objects

template<class T>
class pool_objects {
	std::vector<T> pool_; // プール領域
	fixed_queue<size_t> uses_; // 使用領域インデックスリスト
	fixed_queue<size_t> empties_; // 空き領域インデックスリスト
	size_t maximum_pool_, minimum_pool_; // 最大容量／最小容量
protected:
	bool growup() {
		if(total()>=maximum_pool_) return false ;
		empties_.push_front(total());
		pool_.push_back(T());
		return true;
	}
public:
	pool_objects(size_t maximum_pool, size_t minimum_pool=0)
	 : maximum_pool_(maximum_pool), minimum_pool_(minimum_pool),
	   uses_(maximum_pool), empties_(maximum_pool) {
		if(minimum_pool_>maximum_pool_) minimum_pool_ = maximum_pool_;
		pool_.reserve(maximum_pool);
		for(decltype(empties_)::size_type i = 0; i<minimum_pool_ ; i++) {
			pool_.push_back(T());
			empties_.push(i);
		}
	}
	~pool_objects() {
		dispose();
	}
	T* head() { // empties の先頭を参照 ( push の前駆obj - prereferenced object )
		if(empties_.size()<=minimum_pool_&&!growup()) return nullptr;
		return &pool_[empties_.front()];
	}
	bool push() { // empties の先頭を uses の最後尾に移動
		if(empties_.size()<=minimum_pool_) return false;
		uses_.push(empties_.front());
		empties_.pop();
		return true ;
	}
	T* pull() { // uses の先頭を empties の最後尾に移動して参照
		if(empty()) return nullptr;
		empties_.push(uses_.front());
		uses_.pop();
		return &pool_[empties_.back()];
	}
	bool pull_undo() { // empties の最後尾を uses の先頭 に移して一つ戻す
		if(empties_.empty()) return false;
		uses_.push_front(empties_.back());
		empties_.pop_back();
		return true;
	}
	void clear() { while(pull()!=nullptr); }
	void dispose() { clear(); for(auto &v: pool_) v.free(); }
	bool empty() const { return uses_.empty(); }
	bool no_pool() const { return empties_.size()<=minimum_pool_&&total()>=maximum_pool_; }
	auto size() const { return uses_.size(); }
	auto total() const { return pool_.size(); }
};

  // pool_buffer_object

template<class T>
class pool_buffer_object {
	struct ref_t {
		ref_t(): data_(nullptr), ref_cnt_(1), size_(0), grew_(0) {}
		~ref_t() { free(); }
		void addref() {ref_cnt_++;}
		bool release() {
			if(!--ref_cnt_) {
				return true ;
			}
			return false;
		}
		void free() {
			if(data_!=nullptr) {
				delete [] data_ ;
				data_ = nullptr ;
			}
			size_ = 0 ; grew_=0 ;
		}
		bool resize(size_t new_size) {
			if(new_size<=grew_) { size_=new_size; return true; }
			T *new_data = new T[new_size];
			if(new_data!=nullptr) {
				if(data_) {
					for (size_t i = 0;i < size_; i++)
						new_data[i] = data_[i];
					delete [] data_ ;
				}
				data_ = new_data ;
				size_ = grew_ = new_size ;
				return true;
			}
			return false;
		}
		bool growup(size_t capa_size) {
			size_t sz = size();
			if(!resize(capa_size)) return false;
			if(!resize(sz)) return false;
			return true;
		}
		T* data() { return data_; }
		size_t size() const { return size_; }
		size_t capacity() const { return grew_; }
	    T& operator[](size_t index) { return data_[index] ; }
	private:
		T* data_;
		size_t size_;
		size_t grew_;
		size_t ref_cnt_;
	};
	ref_t* ref_;
public:
	pool_buffer_object() { ref_ = new ref_t() ; }
	pool_buffer_object(const pool_buffer_object &obj)
	{ ref_ = obj.ref_; ref_->addref(); }
	~pool_buffer_object()
	{ if(ref_->release()) delete ref_; }
	void free() { ref_->free(); }
	bool resize(size_t new_size) { return ref_->resize(new_size); }
	bool growup(size_t capa_size) { return ref_->growup(capa_size); }
	T* data() { return ref_->data(); }
	size_t size() const { return ref_->size(); }
	size_t capacity() const { return ref_->capacity(); }
    T& operator[](size_t index) { return (*ref_)[index] ; }
	pool_buffer_object& operator =(const pool_buffer_object &obj)
	{ if(ref_->release()) delete ref_; ref_=obj.ref_; ref_->addref(); return *this; }
};

  // pool_buffer

template<class T>
using pool_buffer = pool_objects< pool_buffer_object<T> > ;

//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
using namespace PRY8EAlByw ;
//===========================================================================
#endif // _POOLBUFFER_20210321220639424_H_INCLUDED_
