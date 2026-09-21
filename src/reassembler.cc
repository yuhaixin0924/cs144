#include "reassembler.hh"
#include "debug.hh"
#include <algorithm>
using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  Writer& writer = output_.writer();
  const uint64_t original_end = first_index + data.size(); // 计算原始结束位置
  if ( is_last_substring ) {
    eof_index_ = original_end;
  }
  const uint64_t unassembled = writer.bytes_pushed();
  const uint64_t unaccetable = unassembled + writer.available_capacity();
  uint64_t accepted_begin = max( first_index, unassembled );
  uint64_t accepted_end = min( unaccetable, original_end ); // 注意这个边界去不到
  if ( accepted_begin < accepted_end ) {
    uint64_t scan_position = accepted_begin;
    auto it = pending_.lower_bound( scan_position );
    if ( it != pending_.begin() ) {
      auto previous = prev( it );
      uint64_t previous_end = previous->first + previous->second.size();
      if ( previous_end > accepted_begin ) { // 判断是否与上一个字符串存在重合
        it = previous;
      }
    }

    while ( it != pending_.end() && it->first < accepted_end ) { // 还没处理完并且还有重叠部分
      uint64_t existing_begin = it->first;
      uint64_t existing_end = existing_begin + it->second.size();
      if ( scan_position < existing_begin ) {
        uint64_t gap_end = min( existing_begin, accepted_end );
        // 索引+字符串substr（相对first_index位置,长度）
        pending_.emplace( scan_position, data.substr( scan_position - first_index, gap_end - scan_position ) );
        scan_position = gap_end;
      }
      scan_position = existing_end; // 跳过已经存在的不用重新加载的字符串
      if ( scan_position >= accepted_end ) {
        break;
      }
      ++it; // 对比下一个
    }
    // 最后一个已有片段之后可能还有一段新数据。
    if ( scan_position < accepted_end ) {
      pending_.emplace( scan_position, data.substr( scan_position - first_index, accepted_end - scan_position ) );
    }
  }
  while ( true ) {
    const auto next = pending_.find( writer.bytes_pushed() );
    if ( next == pending_.end() ) {
      break;
    }
    string contiguous_data = move( next->second );
    pending_.erase( next );
    writer.push( move( contiguous_data ) );
  }
  // 只有真正写到流的结束位置才能关闭 Writer。
  if ( eof_index_.has_value() && writer.bytes_pushed() == eof_index_.value() ) {
    writer.close();
  }
}
// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t result = 0;
  for(const auto&[first_index,data]:pending_){
    result+=data.size();
  }
  return result;
}
