#include "wrapping_integers.hh"
#include "debug.hh"
#include <limits>
using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return zero_point+static_cast<uint32_t>(n);
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const uint64_t M =uint64_t{1}<<32;
  uint64_t q=checkpoint/M;
  uint64_t offset=raw_value_-zero_point.raw_value_;
  uint64_t candidate=q*M+offset;
  //与checkpoint在同一个区间的candidata N,注意还有candidata-M,candidata+M也可能成为最终的N
  if(checkpoint>candidate){
    if(checkpoint-candidate>M/2&&candidate<=numeric_limits<uint64_t>::max()-M){
      candidate+=M;//checkpoint更偏向candidata+M
    }
  }
  else{
    //第一次写的时候漏了candidate>=M，发生了无符号下溢
    if(candidate -checkpoint>M/2&&candidate>=M){
      candidate-=M;//checkpoint更偏向candidata-M
    }
  }
  return candidate;
}
