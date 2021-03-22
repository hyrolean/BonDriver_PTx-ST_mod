//===========================================================================
#pragma once
#ifndef _POOLBUFFER_20210321220639424_H_INCLUDED_
#define _POOLBUFFER_20210321220639424_H_INCLUDED_
//---------------------------------------------------------------------------

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
	fixed_queue<T> objs_; // 使用領域
	fixed_queue<T> pool_; // 空き領域
	size_t maximum_pool_, minimum_pool_; // 最大容量／最小容量
protected:
	bool growup() {
		if(total()>=maximum_pool_) return false ;
		pool_.push_front(T());
		return true;
	}
public:
	pool_objects(size_t maximum_pool, size_t minimum_pool=0)
	 : maximum_pool_(maximum_pool), minimum_pool_(minimum_pool),
	   objs_(maximum_pool), pool_(maximum_pool) {
		size_t initial_pool = minimum_pool+1 ;
		while(initial_pool--) pool_.push(T());
	}
	~pool_objects() {
		dispose();
	}
	T* head() { // pool の先頭を参照 ( push の前駆obj - prereferenced object )
		if(pool_.size()<=minimum_pool_&&!growup()) return nullptr;
		return &pool_.front();
	}
	bool push() { // pool の先頭を objs の最後尾に移動
		if(pool_.size()<=minimum_pool_) return false;
		objs_.push(pool_.front());
		pool_.pop();
		return true ;
	}
	T* pull() { // objs の先頭を pool の最後尾に移動して参照
		if(empty()) return nullptr;
		pool_.push(objs_.front());
		objs_.pop();
		return &pool_.back();
	}
	void dispose() {
		clear();
		for(decltype(pool_.size()) i=0;i<pool_.size();i++) pool_[i].free();
	}
	void clear() { while(pull()!=nullptr); }
	bool empty() const { return objs_.empty(); }
	bool no_pool() const { return pool_.empty()&&total()>=maximum_pool_; }
	auto size() const { return objs_.size(); }
	auto total() const { return objs_.size()+pool_.size(); }
};

  // pool_buffer_object

template<class T>
class pool_buffer_object {
	T* data_;
	size_t size_;
	size_t grew_;
public:
	pool_buffer_object()
	 : data_(nullptr), size_(0), grew_(0) {}
	pool_buffer_object(const pool_buffer_object &obj)
	 : data_(obj.data_), size_(obj.size_), grew_(obj.grew_) {}
	void free() {
		if(data_!=nullptr) {
			delete [] data_ ;
			data_ = nullptr ;
		}
		size_ = 0 ; grew_=0 ;
	}
	bool resize(size_t new_size) {
		if(!new_size) { free(); return true; }
		if(new_size<=grew_) { size_=new_size; return true; }
		T *new_data = new T[new_size];
		if(new_data!=nullptr) {
			if(data_) {
				std::copy(&data_[0],&data_[size_],new_data);
				delete [] data_ ;
			}
			data_ = new_data ;
			size_ = grew_ = new_size ;
			return true;
		}
		return false;
	}
	T* data() { return data_; }
	size_t size() const { return size_; }
    T& operator[](size_t index) { return data_[index] ; }
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
