#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{ 
  if(is_closed()||data.empty()){
    return;
  }
  uint64_t push_size;
  push_size=min(available_capacity(),data.size());
  if(push_size==0) return;
  if(push_size>0){
    data.resize(push_size);
    chunks_.push_back(move(data));
  }
  bytes_pushed_+=push_size;
}

void Writer::close()
{
  closed_=true;
  // Your code here.
}

bool Writer::is_closed() const
{ 
  return closed_; // Your code here.
}

uint64_t Writer::available_capacity() const
{
  return capacity_-(bytes_pushed_-bytes_popped_); // Your code here.
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_; // Your code here.
}

string_view Reader::peek() const
{ 
  if(chunks_.empty()){
    return {};
  }
  string_view view=string_view{chunks_.front()};
  string_view result=view.substr(front_offset_);
  return result;
}

void Reader::pop( uint64_t len )
{ 
  uint64_t remaining=min(len,bytes_buffered());
  while(remaining>0){
    uint64_t available_in_front=chunks_.front().size()-front_offset_;
    uint64_t amount=min(remaining,available_in_front);
    front_offset_+=amount;
    bytes_popped_+=amount;
    remaining-=amount;
    if(front_offset_== chunks_.front().size()){
      chunks_.pop_front();
      front_offset_=0;
    }
  }
}

bool Reader::is_finished() const
{
  return closed_&&(bytes_buffered()==0); // Your code here.
}

uint64_t Reader::bytes_buffered() const
{
  return bytes_pushed_-bytes_popped_; // Your code here.
}

uint64_t Reader::bytes_popped() const
{ 
  return bytes_popped_; // Your code here.
}

